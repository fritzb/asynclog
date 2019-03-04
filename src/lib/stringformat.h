//
// StringFromat
//

#ifndef ASYNCLOG_STRINGFORMAT_H
#define ASYNCLOG_STRINGFORMAT_H


class StringFormat {

public:
    void parseStringFormat(char *dst, const char *src, va_list va);
    uint32_t indexInc(uint32_t index, uint32_t v);

    /* Memory buffer utilities */
    uint32_t memSetByte(char *s, uint8_t u8);
    uint32_t memSetWord(char *s, uint16_t u16);
    uint32_t memSetInt(char *s, uint32_t u32);
    uint32_t memSetLong64(char *s, uint64_t u64);
    uint32_t memSetDouble(char *s, double d);
    uint32_t memSetPtr(char *s, void *src);
    uint32_t memSetString(char *s, const char *src);
    uint8_t memGetByte(uint8_t *buf, uint32_t dstIndex);
    uint16_t memGetWord(uint8_t *buf, uint32_t dstIndex);
    uint32_t memGetInt(uint8_t *buf, uint32_t dstIndex);
    double memGetDouble(uint8_t *buf, uint32_t dstIndex);
    uint64_t memGetLong64(uint8_t *buf, uint32_t dstIndex);
    void * memGetPtr(uint8_t *buf, uint32_t dstIndex);
    uint32_t memGetString(uint8_t *buf, uint32_t buf_index, char *dst);
    uint32_t memGetString(uint8_t *buf, uint32_t buf_index, char *dst, uint32_t maxStringLength);
    uint32_t getStringCorruptedCount;
    uint32_t decodeStringFormat(const char *s, uint8_t *start_buf, char *dst, int maxLen, uint32_t *buf_index_p, int *string_length);
};

#endif //ASYNCLOG_STRINGFORMAT_H
