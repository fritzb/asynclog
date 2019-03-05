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

void StringFormat::decodeStringFormat(char **dst_p, const char *src, va_list va) {
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
    char *dst = *dst_p;

    while (src[i] != 0) {
        // PERCENT
        if (src[i] == '%') {
            if (insidePercent) {
                insidePercent = false;
                goto next;
            }
            insidePercent = true;
            goto next;
        }

        if (insidePercent) {
            insidePercent = false;

            switch (src[i]) {
                case 'h':
                    switch (src[i+1]) {
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
                            if ((src[i+2] == 'x') ||
                                (src[i+2] == 'X') ||
                                (src[i+2] == 'u') ||
                                (src[i+2] == 'o')) {
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
                    switch (src[i+1]) {
                        case 'l':
                            if ((src[i+2] == 'x') ||
                                (src[i+2] == 'X') ||
                                (src[i+2] == 'd') ||
                                (src[i+2] == 'i') ||
                                (src[i+2] == 'u')) {
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
                    if (src[i] == '-' || src[i] == '+' || src[i] == ' ' || src[i] == '#' ||
                        src[i] == '.' || (isdigit(src[i]))) {
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

    *dst_p = dst;
}

uint32_t StringFormat::indexInc(uint32_t index, uint32_t v) {
    return index + v;
}

void StringFormat::strlcpy(char *dst, const char *src, size_t siz) {
    char *d = dst;
    const char *s = src;
    size_t n = siz;
    /* Copy as many bytes as will fit */
    if (n != 0) {
        while (--n != 0) {
            if ((*d++ = *s++) == '\0')
                break;
        }
    }
    /* Not enough room in dst, add NUL and traverse rest of src */
    if (n == 0) {
        if (siz != 0)
            *d = '\0';		/* NUL-terminate dst */
        while (*s++)
            ;
    }
}

uint32_t StringFormat::encodeStringFormat(const char *s, uint8_t *start_buf, char *dst, int maxLen,
                                          uint32_t *buf_index_p, int *string_length) {
    void *ptr;
    double d;
    char format[32];
    uint32_t start, i;
    bool eat;
    uint64_t u64;
    uint32_t u32;
    uint8_t u8;
    uint16_t u16;
    int max_string;
    uint32_t string_len;
    char *start_dst_buffer = dst;
    uint32_t buf_index = *buf_index_p;

    i = 0;
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
                                strlcpy(format, &s[start], i-start + 1);
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
                                        strlcpy(format, &s[start], i-start + 1);
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
                        strlcpy(format, &s[start], i-start + 1);
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
                        strlcpy(format, &s[start], i-start + 1);
                        u32 = memGetInt(start_buf, buf_index);
                        buf_index = indexInc(buf_index, sizeof(u32));
                        dst += sprintf(dst, format, u32);
                        eat = true;
                        break;

                    case 'f':
                        i++;
                        strlcpy(format, &s[start], i-start + 1);
                        d = memGetDouble(start_buf, buf_index);
                        buf_index = indexInc(buf_index, sizeof(d));
                        dst += sprintf(dst, format, d);
                        eat = true;
                        break;

                    case 'p':
                        i++;
                        strlcpy(format, &s[start], i-start + 1);
                        ptr = (void *)memGetPtr(start_buf, buf_index);
                        buf_index = indexInc(buf_index, sizeof(void *));
                        dst += sprintf(dst, format, ptr);
                        eat = true;
                        break;

                        // string
                    case 's':
                        i++;
                        strlcpy(format, &s[start], i-start + 1);
                        //printf("%s\n", format);

                        //string_len = getString(buf_index, dst);
                        //string_len = memGetString(start_buf, buf_index, dst);

                        // max_string is the maximum possible string length
                        // (excluding the null terminated char) in the buffer
                        max_string = ((maxLen) - buf_index) - 1;
                        if (max_string < 0) {
                            getStringCorruptedCount++;
                            return -1;
                        }

                        string_len = memGetString( start_buf, buf_index, dst, max_string);
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
                                    strlcpy(format, &s[start], i-start + 1);
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
                                strlcpy(format, &s[start], i-start + 1);
                                u32 = memGetInt(start_buf, buf_index);
                                buf_index = indexInc(buf_index, sizeof(u32));
                                dst += sprintf(dst, format, u32);
                                eat = true;
                                break;

                            case 'f':
                                i++;
                                strlcpy(format, &s[start], i-start + 1);
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

        // TODO: bound check
    }
    *dst = 0;

    if (dst > start_dst_buffer) {
        *string_length = ((int)(dst - start_dst_buffer));
    }

    *buf_index_p = buf_index;

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

