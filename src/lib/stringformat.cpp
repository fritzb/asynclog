/* Copyright 2019 memlog Authors. All Rights Reserved.

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
//
// StringFormat class
//

#include <cstdint>
#include <cstdarg>
#include <assert.h>
#include <cctype>
#include <cstring>
#include <cstdio>
#include "stringformat.h"
#define LOG_MAX_LOG_TRACE_LINE (4096)

using namespace memlog;

// Store variable length arguments into args buffer
void StringFormat::encodeToArgsBuffer(const char *format, va_list args, char **argsBuffer) {
    int i{ 0 };
    bool insidePercent{ false };
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    double d;
    void *ptr;
    char *dst{ *argsBuffer };

    while (format[i] != 0) {
        if (format[i] == '%') {
            if (insidePercent) {
                insidePercent = false;
                goto next;
            }
            insidePercent = true;
            goto next;
        }

        if (insidePercent) {
            insidePercent = false;

            switch (format[i]) {
                case 'h':
                    switch (format[i+1]) {
                        case 'o':
                        case 'x':
                        case 'X':
                        case 'u':
                            u16 = va_arg(args, unsigned int);
                            dst += memSetWord(dst, u16);
                            break;

                        case 'd':
                        case 'i':
                            u16 = va_arg(args, int);
                            dst += memSetWord(dst, u16);
                            break;

                        case 'h':
                            if ((format[i+2] == 'x') ||
                                (format[i+2] == 'X') ||
                                (format[i+2] == 'u') ||
                                (format[i+2] == 'o')) {
                                u16 = va_arg(args, unsigned int);
                                dst += memSetWord(dst, u16);
                                continue;
                            }
                            break;
                    }
                    break;

                case '%':
                    break;

                case 'c':
                    u8 = va_arg(args, int);
                    dst += memSetByte(dst, u8);
                    break;

                case 'd':
                case 'u':
                case 'i':
                case 'x':
                case 'X':
                    u32 = va_arg(args, unsigned int);
                    dst += memSetInt(dst, u32);
                    break;

                case 'f':
                    d = va_arg(args, double);
                    dst += memSetDouble(dst, d);
                    break;

                case 'p':
                    ptr = va_arg(args, void *);
                    dst += memSetPtr(dst, ptr);
                    break;

                case 's':
                    ptr = va_arg(args, char *);
                    dst += memSetString(dst, (const char *) ptr);
                    break;

                case 'l':
                    switch (format[i+1]) {
                        case 'l':
                            if ((format[i+2] == 'x') ||
                                (format[i+2] == 'X') ||
                                (format[i+2] == 'd') ||
                                (format[i+2] == 'i') ||
                                (format[i+2] == 'u')) {
                                i += 2;
                                u64 = va_arg(args, long long);
                                dst += memSetLong64(dst, u64);
                                continue;
                            }
                            break;

                        case 'x':
                        case 'X':
                        case 'd':
                        case 'i':
                        case 'u':
                            u32 = va_arg(args, unsigned int);
                            dst += memSetInt(dst, u32);
                            break;

                        case 'f':
                            d = va_arg(args, double);
                            dst += memSetDouble(dst, d);
                            break;

                        default:
                            // Don't know, could be NULL terminated symbol
                            break;
                    }
                    break;

                default:
                    if (format[i] == '-' || format[i] == '+' || format[i] == ' ' || format[i] == '#' ||
                        format[i] == '.' || (isdigit(format[i]))) {
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

    *argsBuffer = dst;
}

uint32_t StringFormat::indexInc(uint32_t index, uint32_t v) {
    return index + v;
}

void StringFormat::strlcpy(char *dst, const char *src, size_t siz) {
    char *d = dst;
    const char *s = src;
    size_t n = siz;

    // Copy as many bytes as possibe
    if (n != 0) {
        while (--n != 0) {
            if ((*d++ = *s++) == '\0')
                break;
        }
    }

    // Not enough room in dst, add NUL and traverse rest of src
    if (n == 0) {
        if (siz != 0)
            *d = '\0'; // NUL-terminate dst
        while (*s++)
            ;
    }
}

uint32_t StringFormat::decodeFromArgsBuffer(const char *format,
                                            uint8_t *argsBuffer,
                                            uint32_t *argsBufferIndexPtr,
                                            char *outputString,
                                            int outputStringMaxLength,
                                            int *outputStringLength) {
    void *ptr;
    double d;
    char tempFormat[32];
    uint32_t start, i;
    bool consumed;
    uint64_t u64;
    uint32_t u32;
    uint8_t u8;
    uint16_t u16;
    int max_string;
    uint32_t string_len;
    char *startOutputString = outputString;
    uint32_t argsBufferIndex = *argsBufferIndexPtr;

    i = 0;
    while (format[i] != 0) {
        // PERCENT?
        consumed = false;
        start = i;
        if (format[i] == '%') {
            i++;

            // %#x or %-2.2d or %+2.2x
            if (format[i] && (format[i] == '0' || format[i] == ' ' || format[i] == '#' ||
                    format[i] == '-' || format[i] == '+')) {
                i++;
            }

            // %2.2x
            while (format[i] != 0) {
                if (isdigit(format[i])) {
                    i++;
                } else {
                    break;
                }
            }
            if (format[i] && format[i] == '.') {
                i++;
            }
            while (format[i] != 0) {
                if (isdigit(format[i])) {
                    i++;
                } else {
                    break;
                }
            }
            if (format[i]) {
                switch (format[i]) {
                    case 'h':
                        i++;
                        switch(format[i]) {
                            case 'd':
                            case 'u':
                            case 'x':
                            case 'X':
                            case 'o':
                            case 'i':
                                i++;
                                strlcpy(tempFormat, &format[start], i-start + 1);
                                u16 = memGetWord(argsBuffer, argsBufferIndex);
                                argsBufferIndex = indexInc(argsBufferIndex, sizeof(u16));
                                outputString += sprintf(outputString, tempFormat, u16);
                                consumed = true;
                                break;
                            case 'h':
                                i++;
                                switch(format[i]) {
                                    case 'x':
                                    case 'X':
                                    case 'u':
                                    case 'o':
                                        i++;
                                        strlcpy(tempFormat, &format[start], i-start + 1);
                                        u16 = memGetWord(argsBuffer, argsBufferIndex);
                                        argsBufferIndex = indexInc(argsBufferIndex, sizeof(u16));
                                        outputString += sprintf(outputString, tempFormat, u16);
                                        consumed = true;
                                        break;
                                }
                        }
                        break;

                    case 'c':
                        i++;
                        strlcpy(tempFormat, &format[start], i-start + 1);
                        u8 = memGetByte(argsBuffer, argsBufferIndex);
                        argsBufferIndex = indexInc(argsBufferIndex, sizeof(u8));
                        outputString += sprintf(outputString, tempFormat, u8);
                        consumed = true;
                        break;

                    case 'd':
                    case 'u':
                    case 'i':
                    case 'x':
                    case 'X':
                        i++;
                        strlcpy(tempFormat, &format[start], i-start + 1);
                        u32 = memGetInt(argsBuffer, argsBufferIndex);
                        argsBufferIndex = indexInc(argsBufferIndex, sizeof(u32));
                        outputString += sprintf(outputString, tempFormat, u32);
                        consumed = true;
                        break;

                    case 'f':
                        i++;
                        strlcpy(tempFormat, &format[start], i-start + 1);
                        d = memGetDouble(argsBuffer, argsBufferIndex);
                        argsBufferIndex = indexInc(argsBufferIndex, sizeof(d));
                        outputString += sprintf(outputString, tempFormat, d);
                        consumed = true;
                        break;

                    case 'p':
                        i++;
                        strlcpy(tempFormat, &format[start], i-start + 1);
                        ptr = (void *)memGetPtr(argsBuffer, argsBufferIndex);
                        argsBufferIndex = indexInc(argsBufferIndex, sizeof(void *));
                        outputString += sprintf(outputString, tempFormat, ptr);
                        consumed = true;
                        break;

                    // string
                    case 's':
                        i++;
                        strlcpy(tempFormat, &format[start], i-start + 1);
                        //printf("%format\n", tempFormat);

                        //string_len = getString(argsBufferIndex, outputString);
                        //string_len = memGetString(argsBuffer, argsBufferIndex, outputString);

                        // max_string is the maximum possible string length
                        // (excluding the null terminated char) in the buffer
                        max_string = ((outputStringMaxLength) - argsBufferIndex) - 1;
                        if (max_string < 0) {
                            getStringCorruptedCount++;
                            return -1;
                        }

                        string_len = memGetString(argsBuffer, argsBufferIndex, outputString, max_string);
                        outputString += string_len;

                        argsBufferIndex = indexInc(argsBufferIndex, string_len + 1);
                        consumed = true;
                        break;

                    // %lld %llu %llx
                    case 'l':
                        i++;
                        switch (format[i]) {
                            case'l':
                                i++;
                                if (format[i] && (format[i] == 'd' || format[i] == 'i' || format[i] == 'x' || format[i] == 'u' || format[i] == 'X')) {
                                    i++;
                                    strlcpy(tempFormat, &format[start], i-start + 1);
                                    u64 = memGetLong64(argsBuffer, argsBufferIndex);
                                    argsBufferIndex = indexInc(argsBufferIndex, sizeof(u64));
                                    outputString += sprintf(outputString, tempFormat, u64);
                                    consumed = true;
                                }
                                break;

                            case 'd':
                            case 'u':
                            case 'i':
                            case 'x':
                            case 'X':
                                i++;
                                strlcpy(tempFormat, &format[start], i-start + 1);
                                u32 = memGetInt(argsBuffer, argsBufferIndex);
                                argsBufferIndex = indexInc(argsBufferIndex, sizeof(u32));
                                outputString += sprintf(outputString, tempFormat, u32);
                                consumed = true;
                                break;

                            case 'f':
                                i++;
                                strlcpy(tempFormat, &format[start], i-start + 1);
                                d = memGetDouble(argsBuffer, argsBufferIndex);
                                argsBufferIndex = indexInc(argsBufferIndex, sizeof(d));
                                outputString += sprintf(outputString, tempFormat, d);
                                consumed = true;
                                break;

                            default:
                                break;
                        }
                        break;

                    case '%':
                        i++;
                        outputString += sprintf(outputString, "%%");
                        consumed = true;
                        break;

                    default:
                        break;
                }
            }
        }

        // Copy
        if (!consumed) {
            i = start;
            *outputString = format[i];
            outputString++;
            i++;
        }

        // TODO: bound check
    }
    *outputString = 0;

    if (outputString > startOutputString) {
        *outputStringLength = ((int)(outputString - startOutputString));
    }

    *argsBufferIndexPtr = argsBufferIndex;

    return 0;
}

// Write u8 into the non circular buffer
uint32_t StringFormat::memSetByte(char *s, uint8_t u8) {
    *(uint8_t *)s = u8;
    return sizeof(u8);
}

//Write u16 into the circular buffer
uint32_t StringFormat::memSetWord(char *s, uint16_t u16) {
    memcpy(s, &u16, sizeof(u16));
    return sizeof(u16);
}

// Write u32 into the circular buffer
uint32_t StringFormat::memSetInt(char *s, uint32_t u32) {
    memcpy(s, &u32, sizeof(u32));
    return sizeof(u32);
}

uint32_t StringFormat::memSetLong64(char *s, uint64_t u64) {
    memcpy(s, &u64, sizeof(u64));
    return sizeof(u64);
}

// Write double into the circular buffer
uint32_t StringFormat::memSetDouble(char *s, double d) {
    memcpy(s, &d, sizeof(d));
    return sizeof(d);
}

uint32_t StringFormat::memSetPtr(char *s, void *src) {
    memcpy(s, &src, sizeof(void *));
    return sizeof(void *);
}

uint32_t StringFormat::memSetString(char *s, const char *src) {
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
uint8_t StringFormat::memGetByte(uint8_t *buf, uint32_t dstIndex) {
    uint8_t u8;
    memcpy(&u8, (buf + dstIndex), sizeof(u8));
    return u8;
}

uint16_t StringFormat::memGetWord(uint8_t *buf, uint32_t dstIndex) {
    uint16_t u16;
    memcpy(&u16, (buf + dstIndex), sizeof(u16));
    return u16;
}

uint32_t StringFormat::memGetInt(uint8_t *buf, uint32_t dstIndex) {
    uint32_t u32;
    memcpy(&u32, (buf + dstIndex), sizeof(u32));
    return u32;
}

double StringFormat::memGetDouble(uint8_t *buf, uint32_t dstIndex) {
    double d;
    memcpy(&d, (buf + dstIndex), sizeof(d));
    return d;
}

void * StringFormat::memGetPtr(uint8_t *buf, uint32_t dstIndex) {
    void *ptr;
    memcpy(&ptr, (buf + dstIndex), sizeof(ptr));
    return ptr;
}

uint64_t StringFormat::memGetLong64(uint8_t *buf, uint32_t dstIndex) {
    uint64_t u64;
    memcpy(&u64, (buf + dstIndex), sizeof(u64));
    return u64;
}

uint32_t StringFormat::memGetString(uint8_t *buf, uint32_t bufferIndex, char *dst) {
    uint32_t di = 0;

    while (buf[bufferIndex] != 0) {
        dst[di] = buf[bufferIndex];
        di++;
        bufferIndex++;
    }
    return di;
}

// Get string from buffer (excluding the zero terminated character)
uint32_t StringFormat::memGetString(uint8_t *buf, uint32_t bufferIndex, char *dst, uint32_t maxStringLength) {
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

