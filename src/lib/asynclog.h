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

#ifndef __ASYNCLOG_H__
#define __ASYNCLOG_H__

#include <stdint.h>
#include <time.h>
#include <memory>
#include <pthread.h>
#include "ringbuffer.h"
#include "stream.h"

#define info(msg, ...) traceVargs(true, __func__, __LINE__, 'I', msg, ##__VA_ARGS__)

class Log {
public:
    static const uint32_t DEFAULT_BUFFER_SIZE = 20*1024*1024;
    static const uint64_t MARKER = 0xaf1cfeefbeefae0dLL;
    static const uint32_t VERSION = 1;
    static const uint32_t START_PATTERN = 0xbeedface;
    static const uint32_t END_PATTERN = 0xfadebeef;

    enum Level {
        off,
        debug,
        info,
        warn,
        error,
        fatal,
        trace,
        all,
    };

    class Collect;
    typedef uint32_t Marker;

    struct Trailer {
        uint32_t id;
        uint32_t pattern;
    };

    struct Header {
        Log::Marker     pattern;
        uint32_t        location; // optional
        uint32_t        id; // optional
        uint32_t        threadId;
        const char      *format;
        const char      *functionName;
        uint16_t        lineNumber;
        uint16_t        length; // optional
        char            tag;
        char            unused[3];
        struct timespec timestamp;
        char            stack[0];
    };

    void traceVargs(bool with_ts, const char *function_name, uint32_t line_number, char tag, const char *s, ...);
    uint32_t dumpRange(uint32_t start, uint32_t end, bool continueOnFailure, std::shared_ptr<Stream> stream);
    void dump(std::shared_ptr<Stream> stream=nullptr, bool detail=false);

    void dumpState(char *buffer, int bufferLen) const;
    void printState() const;

    Log(const char *filename="rxtrace.txt",
        int lines=DEFAULT_BUFFER_SIZE,
        bool enableCollect=true,
        bool redirectStd=false);
    ~Log();

private:
    uint64_t marker_;
    uint32_t version_;
    std::shared_ptr<Collect> collect_;
    std::shared_ptr<RingBuffer> ringBuffer_;
    std::shared_ptr<Stream> stream_;
    bool redirectStd_;
    FILE *fileHandle_;
    char filename_[1000];
    uint32_t globalId_;
    uint32_t bufLastWrittenIndex_;

    // Counters for debugging
    uint32_t getStringCorruptedCount;
    uint32_t glideCount_;
    uint64_t lostCollectCount_;
    uint32_t printFallCount_;
    uint32_t getNextHeaderFailCount_;
    uint32_t lastPrintedId_;
    uint64_t collectCount_;
    uint32_t freopenFailedCount_;
    uint32_t fwriteFailCount_;
    uint32_t fwriteEwouldblockCount_;
    uint32_t fwriteEintrCount_;
    uint32_t fwriteZeroCount_;
    int      fwriteErrno_;

    // Debug state
    Header debugLastHeader_;
    uint32_t debugState1_;
    uint32_t debugState2_;
    uint32_t debugState3_;
    uint32_t fullBufLenErr;
    uint32_t hdrPatErr;
    uint32_t hdrLenErr;
    uint32_t hdrTailErr;
    uint32_t debugIndex_;
    Log::Trailer debugTrailer_;
    Header debugHdr_;

    char * getTraceFilename() const;
    FILE* createTracefile(const char *filename, bool redirStd);
    uint32_t getLastWrittenIndex();
    std::shared_ptr<Stream> getStream() { return stream_; }
    uint32_t allocateId();

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

    void setLastWrittenIndex(uint32_t index);

    uint32_t setHeader(char *dst, const char *function_name, uint16_t lineNumber,
                       char tag, const char *s, bool withTs,
                       uint16_t length, // optional
                       uint32_t location, // optional
                       uint32_t id // optional
    );


    uint32_t indexInc(uint32_t index, uint32_t v);
    bool getLog(uint32_t index, char *buf);
    bool isEntryValid(uint32_t index, char *buf);
    uint32_t getNextHeader(uint32_t index, char *buf);
    uint32_t getNextHeaderIndex(uint32_t index);
    int cmpHeader(Header *entry1, Header *entry2);
    int printAtIndex(uint32_t index, char *dst, uint32_t *next_index,
                     bool retry,
                     Header *printed_header, int *string_length);
    unsigned int firstLine(void);
    uint32_t getTime(struct timespec *ts, char *ts_buf, unsigned int ts_buf_size);


};

class Log::Collect {
public:
    static const int DEFAULT_BUFFER_THRESHOLD_PCT = 0;
    // Sleep time is 100ms
    static const int SLEEP_USEC = 100000;
    // Spin sleep time is 10ms
    static const int SPIN_USEC = 100000;
    static const int SPINS = 10;
    static const int FLUSH_IN_SEC = 10;

    bool getEnable() const;
    void setEnable(bool enable);

    void resetBookmark();
    uint32_t collect();
    void dumpState(char *buffer, int bufferLen) const;

    Collect(Log *log, bool enable=false);
    ~Collect();

private:
    Log *log_;
    pthread_t collectorThread_;
    uint32_t collectorBookmark_;
    uint32_t bufferThresholdPct_;
    uint32_t prevCollectRangeStart_;
    uint32_t prevCollectRangeEnd_;

    bool enable_;
    unsigned int streamLastFlush_;
    unsigned int lastFlushCounter_;

    uint32_t getBufferThreshold();

    void idle();

    void setBufferThresholdPct(uint32_t value);
    bool shallCollect();
    void workerThread();
    void flush(void);

    static void* executeWorkerThread(void *ctx);
};

#endif
