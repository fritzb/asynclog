/* Copyright 2019 asynclog Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/


// TODO: Rewrite the entire code (originally written in C)

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cctype>
#include <pthread.h>
#include <inttypes.h>

#include "asynclog.h"
#include "ringbuffer.h"



uint32_t Log::getTime(struct timespec *ts, char *ts_buf, unsigned int ts_buf_size) {
    struct tm result;

    localtime_r(&ts->tv_sec, &result);
    strftime(ts_buf, ts_buf_size, "%Y %h %e %T", &result);
    sprintf(&ts_buf[20], ".%9.9ld", ts->tv_nsec);
    return strlen(ts_buf);
}

uint32_t Log::allocateId() {
    uint32_t id = globalId_;
    globalId_++;

    if (globalId_ == START_PATTERN ||
        globalId_ == END_PATTERN) {
        globalId_++;
    }
    return id;
}

// Write u8 into the non circular buffer
uint32_t Log::memSetByte(char *s, uint8_t u8) {
    *(uint8_t *)s = u8;
    return sizeof(u8);
}

//Write u16 into the circular buffer
uint32_t Log::memSetWord(char *s, uint16_t u16) {
    memcpy(s, &u16, sizeof(u16));
    return sizeof(u16);
}

// Write u32 into the circular buffer
uint32_t Log::memSetInt(char *s, uint32_t u32) {
    memcpy(s, &u32, sizeof(u32));
    return sizeof(u32);
}

uint32_t Log::memSetLong64(char *s, uint64_t u64) {
    memcpy(s, &u64, sizeof(u64));
    return sizeof(u64);
}

// Write double into the circular buffer
uint32_t Log::memSetDouble(char *s, double d) {
    memcpy(s, &d, sizeof(d));
    return sizeof(d);
}

uint32_t Log::memSetPtr(char *s, void *src) {
    memcpy(s, &src, sizeof(void *));
    return sizeof(void *);
}

uint32_t Log::memSetString(char *s, const char *src) {
    const char *NULL_STRING = "(null)";
    if (!src) {
        strcpy(s, NULL_STRING);
        return strlen(NULL_STRING) + 1;
    }

    uint32_t str_size = strlen(src);
    memcpy(s, src, str_size + 1);
    return str_size + 1;
}

// Read from memory API
uint8_t Log::memGetByte(uint8_t *buf, uint32_t dstIndex) {
    uint8_t u8;
    memcpy(&u8, (buf + dstIndex), sizeof(u8));
    return u8;
}

uint16_t Log::memGetWord(uint8_t *buf, uint32_t dstIndex) {
    uint16_t u16;
    memcpy(&u16, (buf + dstIndex), sizeof(u16));
    return u16;
}

uint32_t Log::memGetInt(uint8_t *buf, uint32_t dstIndex) {
    uint32_t u32;
    memcpy(&u32, (buf + dstIndex), sizeof(u32));
    return u32;
}

double Log::memGetDouble(uint8_t *buf, uint32_t dstIndex) {
    double d;
    memcpy(&d, (buf + dstIndex), sizeof(d));
    return d;
}

void * Log::memGetPtr(uint8_t *buf, uint32_t dstIndex) {
    void *ptr;
    memcpy(&ptr, (buf + dstIndex), sizeof(ptr));
    return ptr;
}

uint64_t Log::memGetLong64(uint8_t *buf, uint32_t dstIndex) {
    uint64_t u64;
    memcpy(&u64, (buf + dstIndex), sizeof(u64));
    return u64;
}

uint32_t Log::memGetString(uint8_t *buf, uint32_t bufferIndex, char *dst) {
    uint32_t di = 0;

    while (buf[bufferIndex] != 0) {
        dst[di] = buf[bufferIndex];
        di++;
        bufferIndex++;
    }
    return di;
}

// Get string from buffer (excluding the zero terminated character)
uint32_t Log::memGetString(uint8_t *buf, uint32_t bufferIndex, char *dst, uint32_t maxStringLength) {
    uint32_t di = 0;

    while (buf[bufferIndex] != 0) {
        if ((di+1) > maxStringLength) {
            getStringCorruptedCount++;
            return di;
        }
        dst[di] = buf[bufferIndex];
        di++;
        bufferIndex++;
    }
    return di;
}

uint32_t Log::setHeader(char *dst,
                        const char *function_name,
                        uint16_t lineNumber,
                        char tag,
                        const char *s,
                        bool withTs,
                        uint16_t length, // optional
                        uint32_t location, // optional
                        uint32_t id // optional
) {
    Log::Header *hdr = (Log::Header *)dst;

    // Set the timestamp
    if (withTs) {
        clock_gettime(CLOCK_REALTIME, &hdr->timestamp);
    } else {
        memset(&hdr->timestamp, 0, sizeof(hdr->timestamp));
    }

    // Set the start pattern
    hdr->pattern = START_PATTERN;
    hdr->id = id;
    hdr->length = length;
    hdr->functionName = function_name;
    hdr->lineNumber = lineNumber;
    hdr->tag = tag;
    hdr->location = ringBuffer_->normalize(location);

    // Set the format string
    hdr->format = s;

    return sizeof(Log::Header);
}

void Log::setLastWrittenIndex(uint32_t index) {
    bufLastWrittenIndex_ = index;
}

uint32_t Log::getLastWrittenIndex() {
    return bufLastWrittenIndex_;
}

#define LOG_MEM_ALIGN(ret) (ret)
//#define LOG_MEM_ALIGN(ret) \
//    ((ret + (sizeof(void *) - 1)) & ~(sizeof(void *) - 1))

#define LOG_MAX_LOG_TRACE_LINE 4096
void Log::traceVargs(bool with_ts, const char *function_name, uint32_t line_number, char tag, const char *s, ...) {
    va_list va;
    int i = 0;
    bool insidePercent = false;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    double d;
    uint32_t location;
    uint32_t buffer_len, allignedBufferLen;
    void *ptr;
    char buffer[LOG_MAX_LOG_TRACE_LINE + 1];
    char *dst = buffer;

    va_start(va, s);

    dst += sizeof(Log::Header);

    while (s[i] != 0) {
        // PERCENT
        if (s[i] == '%') {
            if (insidePercent) {
                insidePercent = false;
                goto next;
            }
            insidePercent = true;
            goto next;
        }

        if (insidePercent) {
            insidePercent = false;

            switch (s[i]) {
                case 'h':
                    switch (s[i+1]) {
                        case 'o':
                        case 'x':
                        case 'X':
                        case 'u':
                            u16 = va_arg(va, unsigned int);
                            dst += memSetWord(dst, u16);
                            break;

                        case 'd':
                        case 'i':
                            u16 = va_arg(va, int);
                            dst += memSetWord(dst, u16);
                            break;

                        case 'h':
                            if ((s[i+2] == 'x') ||
                                (s[i+2] == 'X') ||
                                (s[i+2] == 'u') ||
                                (s[i+2] == 'o')) {
                                u16 = va_arg(va, unsigned int);
                                dst += memSetWord(dst, u16);
                                continue;
                            }
                            break;
                    }
                    break;

                case '%':
                    break;

                case 'c':
                    u8 = va_arg(va, int);
                    dst += memSetByte(dst, u8);
                    break;

                case 'd':
                case 'u':
                case 'i':
                case 'x':
                case 'X':
                    u32 = va_arg(va, unsigned int);
                    dst += memSetInt(dst, u32);
                    break;

                case 'f':
                    d = va_arg(va, double);
                    dst += memSetDouble(dst, d);
                    break;

                case 'p':
                    ptr = va_arg(va, void *);
                    dst += memSetPtr(dst, ptr);
                    break;

                case 's':
                    ptr = va_arg(va, char *);
                    dst += memSetString(dst, (const char *) ptr);
                    break;

                case 'l':
                    switch (s[i+1]) {
                        case 'l':
                            if ((s[i+2] == 'x') ||
                                (s[i+2] == 'X') ||
                                (s[i+2] == 'd') ||
                                (s[i+2] == 'i') ||
                                (s[i+2] == 'u')) {
                                i += 2;
                                u64 = va_arg(va, long long);
                                dst += memSetLong64(dst, u64);
                                continue;
                            }
                            break;

                        case 'x':
                        case 'X':
                        case 'd':
                        case 'i':
                        case 'u':
                            u32 = va_arg(va, unsigned int);
                            dst += memSetInt(dst, u32);
                            break;

                        case 'f':
                            d = va_arg(va, double);
                            dst += memSetDouble(dst, d);
                            break;

                        default:
                            // Don't know, could be NULL terminated symbol
                            break;
                    }
                    break;

                default:
                    if (s[i] == '-' || s[i] == '+' || s[i] == ' ' || s[i] == '#' ||
                        s[i] == '.' || (isdigit(s[i]))) {
                        insidePercent = true;
                        goto next;
                    }

                    //printf("Unhandled symbol '%c', format: %s", s[i], s);
                    assert(0);
                    insidePercent = false;
                    goto next;
            }
        }

        next:
        i++;
    }
    va_end(va);

    // Copy the temporary buffer
    buffer_len = (uint32_t)(dst - buffer);
    //printf("buffer_len: %d\n", buffer_len);

    uint32_t id = allocateId();

    // Add trailer marker
    Log::Trailer marker = { id, END_PATTERN };
    memcpy(dst, &marker, sizeof(Log::Trailer));
    buffer_len += sizeof(Log::Trailer);
    allignedBufferLen = LOG_MEM_ALIGN(buffer_len);

    // If alignment is enabled, pad it with 0
    int aligned_padding = allignedBufferLen - buffer_len;
    if (aligned_padding > 0) {
        memset(buffer + buffer_len, 0, allignedBufferLen-buffer_len);
        assert(allignedBufferLen - buffer_len < sizeof(void *));
    }

    assert(allignedBufferLen <= LOG_MAX_LOG_TRACE_LINE);

    // Allocate
    location = ringBuffer_->allocate(allignedBufferLen);

    // Write header
    setHeader(buffer, function_name, line_number, tag, s, with_ts, buffer_len, location, id);

    // Copy buffer to circular buffer
    ringBuffer_->set(location, (uint8_t *) buffer, allignedBufferLen);

    setLastWrittenIndex(ringBuffer_->getCurrentIndex());
}

FILE *Log::createTracefile(const char *filename, bool redirStd) {
    FILE *fileHandle;
    fileHandle = fopen(filename, "a+");

    if (redirStd) {
        FILE *f;
        f = freopen(filename, "a", stdout);
        if (!f) {
            freopenFailedCount_++;
        }

        f = freopen(filename, "a", stderr);
        if (!f) {
            freopenFailedCount_++;
        }
    }

    return fileHandle;
}

uint32_t Log::indexInc(uint32_t index, uint32_t v) {
    return index + v;
}

// Get a valid log stored in index.
// Return true if the index contain a valid log
bool Log::getLog(uint32_t index, char *buf) {
    Header read_hdr_buf;
    Header *read_hdr = &read_hdr_buf;
    Header *hdr = (Header *)&buf[0];
    Trailer *trailer_ptr;
    uint16_t length;
    uint32_t id;

    index = ringBuffer_->normalize(index);

    // Get the header to get the length
    ringBuffer_->get((uint8_t *)read_hdr, index, sizeof(Header));
    memcpy(&debugHdr_, read_hdr, sizeof(Header));

    // Validate begining marker
    if (read_hdr->pattern != START_PATTERN) {
        debugIndex_ = index;
        hdrPatErr++;
        debugState1_ = 1;
        debugState2_ = 0;
        debugState3_ = 0;
        return false;
    }

    // Validate length
    if ((read_hdr->length < (sizeof(Header) +
                             sizeof(Trailer))) ||
        (read_hdr->length > LOG_MAX_LOG_TRACE_LINE)) {
        debugIndex_ = index;
        fullBufLenErr++;
        debugState1_ = 0;
        debugState2_ = 1;
        debugState3_ = 0;
        return false;
    }

    length = read_hdr->length;
    id = read_hdr->id;

    // Now copy the complete log with header and length
    ringBuffer_->get((uint8_t *)hdr, index, length);

    // Validate Header
    if (memcmp(read_hdr, hdr, sizeof(Header)) != 0) {
        debugIndex_ = index;
        hdrLenErr++;
        debugState1_ = 2;
        debugState2_ = 0;
        debugState3_ = 0;
        return false;
    }

    // Check trailer
    trailer_ptr = (Trailer *)(((uint8_t *)hdr) + length - sizeof(Trailer));

    // Construct a trailer
    Trailer trailer = { id, END_PATTERN };

    if (memcmp(trailer_ptr, &trailer, sizeof(Trailer)) != 0) {
        memcpy(&debugTrailer_, trailer_ptr, sizeof(Trailer));
        debugIndex_ = index;
        hdrTailErr++;
        debugState1_ = 0;
        debugState2_ = 0;
        debugState3_ = 1;
        return false;
    }

    return true;
}

bool Log::isEntryValid(uint32_t index, char *buf) {
    char scratch_buffer[LOG_MAX_LOG_TRACE_LINE * 2];
    if (buf == NULL) {
        buf = scratch_buffer;
    }
    return getLog(index, buf);
}

// Find current or next header from the circular buffer
uint32_t Log::getNextHeader(uint32_t index, char *buf) {
    // Re-allign pointer
    Marker pattern = START_PATTERN;
    uint32_t loop = 0;

    do {
        while (ringBuffer_->compare(index, (uint8_t *) &pattern, sizeof(pattern))) {
            index++;
            loop++;
            if (loop > ringBuffer_->size()) {
                getNextHeaderFailCount_++;
                return 0;
            }
        }

        // Got a good index, try to validate the complete log entry.
        if (isEntryValid(index, buf)) {
            break;
        } else {
            index++;
            loop++;
            if (loop > ringBuffer_->size()) {
                getNextHeaderFailCount_++;
                return 0;
            }
            continue;
        }
    } while(1);

    glideCount_ = loop;
    return index;
}

uint32_t Log::getNextHeaderIndex(uint32_t index) {
    return getNextHeader(index, NULL);
}

//
// Compare entry1.id and entry2.id
// Return < 0 if entry1 is less than entry2
// Return > 0 if entry1 is bigger than entry2
// Return 0 if entry1 is equal than entry2
//
int Log::cmpHeader(Header *entry1, Header *entry2) {
    if (entry1->id == entry2->id) {
        return 0;
    } else if (entry1->id < entry2->id) {
        // If the entry id differences are so large, most likely the id is
        // experiencing 32bit wraparound
        if ((entry2->id-entry1->id) > 0x7FFFFFFF) {
            return (0xffffffff - entry2->id) + entry1->id;
        }
        return entry1->id - entry2->id;
    } else {
        // If the entry id differences are so large, most likely the id is
        // experiencing 32bit wraparound
        if ((entry1->id - entry2->id) > 0x7FFFFFFF) {
            return (0xffffffff - entry1->id) + entry2->id;
        }
        return entry1->id - entry2->id;
    }
}

//
// next_index: the next log entry index
//
// Return 0 on success
// Return -1 on print failure, and the error message string on the dst.
//
int Log::printAtIndex(uint32_t index, char *dst, uint32_t *next_index,
                      bool retry, Header *printed_header, int *string_length) {
    char scratch_buffer_[LOG_MAX_LOG_TRACE_LINE * 2];
    Header *hdr = (Header *)&scratch_buffer_[0];
    const char *s;
    char format[32];
    uint32_t buf_index;
    uint32_t start, i;
    bool eat;
    uint64_t u64;
    uint32_t u32;
    uint8_t u8;
    uint16_t u16;
    double d;
    uint32_t string_len;
    void *ptr;
    uint8_t *start_buf;
    int max_string;
    char *start_dst_buffer = dst;
    uint32_t hdrid;

    *string_length = 0;

    *next_index = index;

    // Set dst to NULL terminated string
    *dst = 0;

    // Find the next header
    //index = getNextHeader(index);

    // Consumer is faster than producer
    int count = 0;
    bool success;
    do {
        success = getLog(index, (char *) hdr);
        if (success || !retry) {
            break;
        }

        usleep(Log::Collect::SPIN_USEC);
        count++;
    } while (count < Log::Collect::SPINS);

    if (!success) {
        printFallCount_++;
        return -1;
    }

    hdrid = hdr->id;

    // Store the printed header if requested by caller
    if (printed_header) {
        memcpy(printed_header, hdr, sizeof(Header));
    }
    // Store last printed header for debugging
    memcpy(&debugLastHeader_, hdr, sizeof(Header));

    // Print the time stamp if exists
    if (hdr->timestamp.tv_sec != 0) {
        *dst++ = '[';
        dst += getTime(&hdr->timestamp, dst, LOG_MAX_LOG_TRACE_LINE);
        if (hdr->functionName) {
            dst += snprintf(dst,LOG_MAX_LOG_TRACE_LINE,":%u:%c:%s:%u] ",
                            hdr->id,
                            hdr->tag, hdr->functionName, hdr->lineNumber);
        } else if (hdr->lineNumber) {
            dst += sprintf(dst, ":%u:%c:%u] ",
                           hdr->id, hdr->tag, hdr->lineNumber);
        } else if (hdr->tag){
            dst += sprintf(dst, ":%u:%c] ", hdr->id, hdr->tag);
        } else {
            dst += sprintf(dst, ":%u:%u] ", hdr->id, hdr->length);
        }
    } else {
        // Verbose debug
        //dst += sprintf(dst, "[%d:%d] ", hdr->id, hdr->length);
    }

    // Move the stack index
    //buf_index = indexInc(index, sizeof(header));
    buf_index = indexInc(0, sizeof(Header));

    // Parse the format string
    i = 0;
    s = &hdr->format[0];
    start_buf = (uint8_t *)hdr;
    while (s[i] != 0) {
        // PERCENT?
        eat = false;
        start = i;
        if (s[i] == '%') {
            i++;

            // %#x or %-2.2d or %+2.2x
            if (s[i] && (s[i] == '0' || s[i] == ' ' || s[i] == '#' ||
                         s[i] == '-' || s[i] == '+')) {
                i++;
            }

            // %2.2x
            while (s[i] != 0) {
                if (isdigit(s[i])) {
                    i++;
                } else {
                    break;
                }
            }
            if (s[i] && s[i] == '.') {
                i++;
            }
            while (s[i] != 0) {
                if (isdigit(s[i])) {
                    i++;
                } else {
                    break;
                }
            }
            if (s[i]) {
                switch (s[i]) {
                    case 'h':
                        i++;
                        switch(s[i]) {
                            case 'd':
                            case 'u':
                            case 'x':
                            case 'X':
                            case 'o':
                            case 'i':
                                i++;
                                strncpy(format, &s[start], i-start+1);
                                u16 = memGetWord(start_buf, buf_index);
                                buf_index = indexInc(buf_index, sizeof(u16));
                                dst += sprintf(dst, format, u16);
                                eat = true;
                                break;
                            case 'h':
                                i++;
                                switch(s[i]) {
                                    case 'x':
                                    case 'X':
                                    case 'u':
                                    case 'o':
                                        i++;
                                        strncpy(format, &s[start], i-start+1);
                                        u16 = memGetWord(start_buf, buf_index);
                                        buf_index = indexInc(buf_index, sizeof(u16));
                                        dst += sprintf(dst, format, u16);
                                        eat = true;
                                        break;
                                }
                        }
                        break;

                    case 'c':
                        i++;
                        strncpy(format, &s[start], i-start+1);
                        u8 = memGetByte(start_buf, buf_index);
                        buf_index = indexInc(buf_index, sizeof(u8));
                        dst += sprintf(dst, format, u8);
                        eat = true;
                        break;

                    case 'd':
                    case 'u':
                    case 'i':
                    case 'x':
                    case 'X':
                        i++;
                        strncpy(format, &s[start], i-start+1);
                        u32 = memGetInt(start_buf, buf_index);
                        buf_index = indexInc(buf_index, sizeof(u32));
                        dst += sprintf(dst, format, u32);
                        eat = true;
                        break;

                    case 'f':
                        i++;
                        strncpy(format, &s[start], i-start+1);
                        d = memGetDouble(start_buf, buf_index);
                        buf_index = indexInc(buf_index, sizeof(d));
                        dst += sprintf(dst, format, d);
                        eat = true;
                        break;

                    case 'p':
                        i++;
                        strncpy(format, &s[start], i-start+1);
                        ptr = (void *)memGetPtr(start_buf, buf_index);
                        buf_index = indexInc(buf_index, sizeof(void *));
                        dst += sprintf(dst, format, ptr);
                        eat = true;
                        break;

                        // string
                    case 's':
                        i++;
                        strncpy(format, &s[start], i-start+1);
                        //printf("%s\n", format);

                        //string_len = getString(buf_index, dst);
                        //string_len = memGetString(start_buf, buf_index, dst);

                        // max_string is the maximum possible string length
                        // (excluding the null terminated char) in the buffer
                        max_string = ((hdr->length - sizeof(Trailer)) -
                                      buf_index) - 1;
                        if (max_string < 0) {
                            getStringCorruptedCount++;
                            return -1;
                        }

                        string_len = memGetString(
                                start_buf, buf_index, dst, max_string);
                        dst += string_len;

                        buf_index = indexInc(buf_index, string_len + 1);
                        eat = true;
                        break;

                        // %lld %llu %llx
                    case 'l':
                        i++;
                        switch (s[i]) {
                            case'l':
                                i++;
                                if (s[i] && (s[i] == 'd' || s[i] == 'i' || s[i] == 'x' || s[i] == 'u' || s[i] == 'X')) {
                                    i++;
                                    strncpy(format, &s[start], i-start+1);
                                    u64 = memGetLong64(start_buf, buf_index);
                                    buf_index = indexInc(buf_index, sizeof(u64));
                                    dst += sprintf(dst, format, u64);
                                    eat = true;
                                }
                                break;

                            case 'd':
                            case 'u':
                            case 'i':
                            case 'x':
                            case 'X':
                                i++;
                                strncpy(format, &s[start], i-start+1);
                                u32 = memGetInt(start_buf, buf_index);
                                buf_index = indexInc(buf_index, sizeof(u32));
                                dst += sprintf(dst, format, u32);
                                eat = true;
                                break;

                            case 'f':
                                i++;
                                strncpy(format, &s[start], i-start+1);
                                d = memGetDouble(start_buf, buf_index);
                                buf_index = indexInc(buf_index, sizeof(d));
                                dst += sprintf(dst, format, d);
                                eat = true;
                                break;

                            default:
                                break;
                        }
                        break;

                    case '%':
                        i++;
                        dst += sprintf(dst, "%%");
                        eat = true;
                        break;

                    default:
                        break;
                }
            }
        }

        // Copy
        if (!eat) {
            i = start;
            *dst = s[i];
            dst++;
            i++;
        }
        if (index + buf_index > ringBuffer_->size()) {
            break;
        }
    }

    *dst = 0;

    if (dst > start_dst_buffer) {
        *string_length = ((int)(dst - start_dst_buffer));
    }

    // Adding extra trailer bytes, and return it the incoming index
    //*next_index = LOG_MEM_ALIGN(buf_index, sizeof(Trailer));
    *next_index = LOG_MEM_ALIGN(
            indexInc(buf_index, index) +
            sizeof(Trailer));

    // For statistic
    lastPrintedId_ = hdrid;

    return 0;
}

unsigned int Log::firstLine(void) {
    uint32_t current = ringBuffer_->getCurrentIndex();

    if (current >= ringBuffer_->size() || ringBuffer_->hasWrappedAround()) {
        // wrapped around case
        current = ringBuffer_->normalize(current);
        return getNextHeaderIndex(current);
    }
    return 0;
}

uint32_t Log::dumpRange(uint32_t start, uint32_t end, bool continueOnFailure, Stream *stream) {
    char traceBuffer[LOG_MAX_LOG_TRACE_LINE * 2];
    uint32_t new_i, i;
    int traceBufferLen = 0;
    int err;
    Header printedHeader;

    // Normalized both start and end index
    start = ringBuffer_->normalize(start);
    end = ringBuffer_->normalize(end) + 1;
    if (start > end) {
        end = ringBuffer_->size();
    }
    i = start;

    memset(&printedHeader, 0, sizeof(printedHeader));
    bool retry = true;
    if (continueOnFailure) {
        retry = false;
    }

    while (i < end) {
        err = printAtIndex(i, traceBuffer, &new_i, retry, &printedHeader, &traceBufferLen);
        // Producer is faster the log consumer. Realign the collector bookmark to
        // the next log entry, and print the error.
        if (err) {
            Header hdr;
            uint64_t lost = 0;

            // Find the next header and get the header content
            new_i = getNextHeader(i, traceBuffer);
            memcpy(&hdr, traceBuffer, sizeof(hdr));

            // Print the error and bail out
            traceBufferLen = sprintf(traceBuffer,
                                     "<<<< Logs are discarded. Index is realigned to id: %u lastid: %u pat: 0x%x lastwrittenidx: %u loc: %u "
                                     "new_i: %u prev_id: %u prev_loc: %u fail: %u >>>>>\n",
                                     hdr.id, lastPrintedId_, hdr.pattern,
                                     bufLastWrittenIndex_,
                                     hdr.location, new_i,
                                     printedHeader.id, printedHeader.location,
                                     getNextHeaderFailCount_);
            stream->write(traceBuffer, traceBufferLen);

            // Return to the next scanned line
            if (!continueOnFailure) {
                return new_i;
            }
        }


        // Stop if we exceeded the requested range, and dont print
        if (new_i > end) {
            break;
        }
        i = new_i;

        // Start printing
        if (!err) {
            assert(traceBufferLen <= LOG_MAX_LOG_TRACE_LINE);
            collectCount_++;
            stream->write(traceBuffer, traceBufferLen);
        }
    }

    return i;
}

char * Log::getTraceFilename() const {
    return (char *)filename_;
}

void Log::dump(Stream *stream, bool detail) {
    char buf[1000];
    uint32_t i, end;

    if (stream == nullptr) {
        stream = Stream::getStdoutStream();
    }

    i = ringBuffer_->normalize(firstLine());
    end = ringBuffer_->normalize(ringBuffer_->getCurrentIndex());

    if (detail) {
        sprintf(buf, "Global id: %d First line: %d Last line: %d\n\n", globalId_, i, end);
        stream->write(buf, strlen(buf));
    }
    i = dumpRange(i, end - 1, false, stream);
    if (detail) {
        sprintf(buf, "Next printed index: %u\n", i);
        stream->write(buf, strlen(buf));
        dumpState(buf, sizeof(buf));
        stream->write(buf, strlen(buf));
    }
    stream->flush();
}

void Log::dumpState(char *buffer, int bufferLen) const {
    char *bufferNext = buffer;

    bufferNext += sprintf(bufferNext, "Counters:\n");
    bufferNext += sprintf(bufferNext, "File name: %s\n", getTraceFilename());
    bufferNext += sprintf(bufferNext, "Size: %u\n", ringBuffer_->size());
    bufferNext += sprintf(bufferNext, "Current Index: %u\n", ringBuffer_->getCurrentIndex());
    bufferNext += sprintf(bufferNext, "Global Id: %u\n", globalId_);
    bufferNext += sprintf(bufferNext, "Glide: %u\n", glideCount_);
    bufferNext += sprintf(bufferNext, "Collected trace: %" PRId64 "\n", collectCount_);
    bufferNext += sprintf(bufferNext, "Fwrite fail: %u\n", fwriteFailCount_);
    bufferNext += sprintf(bufferNext, "Fwrite ewouldblock: %u\n", fwriteEwouldblockCount_);
    bufferNext += sprintf(bufferNext, "Fwrite eintr: %u\n", fwriteEintrCount_);
    bufferNext += sprintf(bufferNext, "Fwrite zero: %u\n", fwriteZeroCount_);
    bufferNext += sprintf(bufferNext, "Fwrite errno: %d\n", fwriteErrno_);
    bufferNext += sprintf(bufferNext, "Lost lines: %" PRId64 "\n", lostCollectCount_);
    bufferNext += sprintf(bufferNext, "Index 32bit Wrap Around: %u\n", ringBuffer_->getStats().indexWrapAroundCount);
    bufferNext += sprintf(bufferNext, "Print Fail: %u\n", printFallCount_);
    bufferNext += sprintf(bufferNext, "Header pattern mismatch count: %u\n", hdrPatErr);
    bufferNext += sprintf(bufferNext, "hdr length mismatch count: %u\n", hdrLenErr);
    bufferNext += sprintf(bufferNext, "Trailer pattern mismatch count: %u\n", hdrTailErr);
    bufferNext += sprintf(bufferNext, "Buf length validation count: %u\n", fullBufLenErr);
    bufferNext += sprintf(bufferNext, "Get next header fail: %u\n", getNextHeaderFailCount_);
    bufferNext += sprintf(bufferNext, "Get string corrupted: %u\n", getStringCorruptedCount);

}

void Log::printState() const {
    char state[1000];
    dumpState(state, sizeof(state));
    printf("\n%s\n", state);

    char collectorState[1000];
    collect_->dumpState(collectorState, sizeof(collectorState));
    printf("\n%s\n", collectorState);

    Stream::Stats streamStats = stream_->getStats();
    printf("\n");
    printf("Stream stats:\n");
    printf("Zlib error: %u\n", streamStats.zlibError);
    printf("Zlib error no: %d\n", streamStats.zlibErrorNo);
    printf("Zlib deflate init error: %u\n", streamStats.zlibDeflateInitError);
    printf("Zlib deflate error: %u\n", streamStats.zlibDeflateError);
    printf("IO error no: %u\n", streamStats.ioErrorNo);
    printf("IO error: %u\n", streamStats.ioError);
    printf("IO write error: %u\n", streamStats.ioWriteError);
    printf("IO read error: %u\n", streamStats.ioReadError);
    printf("IO seek error: %u\n", streamStats.ioSeekError);
    printf("Compress error: %u\n", streamStats.compressError);
    printf("Flush write count: %u\n", streamStats.flushWriteCount);
}

Log::Log(const char *filename, int lines, bool enableCollect, bool redirectStd)
        : marker_(MARKER), version_(VERSION), ringBuffer_(new RingBuffer(lines)),
          redirectStd_(redirectStd) {
    strncpy(filename_, filename, sizeof(filename_));
    fileHandle_ = createTracefile(filename, redirectStd);
    stream_ = Stream::create(fileHandle_);
    collect_ = new Collect(this, enableCollect);
}

Log::~Log() {
    if (collect_) {
        collect_->setEnable(false);
        delete collect_;
    }
    if (fileHandle_) {
        fflush(fileHandle_);
        fclose(fileHandle_);
        fileHandle_ = nullptr;
    }
    if (stream_) delete stream_;
    if (ringBuffer_) delete ringBuffer_;
}
