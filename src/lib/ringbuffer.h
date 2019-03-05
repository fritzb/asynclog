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
// Thread-safe RingBuffer class
//

#ifndef MEMLOG_RINGBUFFER_H
#define MEMLOG_RINGBUFFER_H

#include <stdint.h>

// RingBuffer class
class RingBuffer {
public:
    static const uint64_t MARKER = 0xfeedf000faeef0feLL;
    static const uint32_t VERSION = 1;
    typedef uint32_t Location;
    struct Stats {
        uint32_t indexWrapAroundCount;
    };


    uint32_t allocate(unsigned int bufferLen);
    void get(uint8_t *dst, uint32_t srcIndex, unsigned int length);
    void set(uint32_t dstIndex, uint8_t *src, unsigned int length);
    int compare(uint32_t bufIndex, uint8_t *p, unsigned int length);

    uint8_t getByte(uint32_t dstIndex);
    uint32_t getInt(uint32_t dstIndex);
    uint64_t getLong64(uint32_t dstIndex);
    double getDouble(uint32_t dstIndex);
    void * getPtr(uint32_t dstIndex);
    uint32_t getString(uint32_t bufferIndex, char *dst);

    uint32_t getCurrentIndex() const;
    bool hasWrappedAround() const;
    Stats getStats() const;

    inline uint32_t size() const { return size_; }
    inline uint32_t normalize(uint32_t index) { return index % size(); }


    RingBuffer(unsigned int size, bool threadSafe=true);
    ~RingBuffer();

private:
    uint64_t marker_;
    uint32_t version_;
    uint8_t *buffer_;
    uint32_t size_;
    uint32_t currentIndex_;
    bool threadSafe_;
    Stats stats_;

    inline bool isIndexRollingOver(uint32_t index) const { return (index > (UINT32_MAX - 10240)); }
};

#endif //ASYNCLOG_RINGBUFFER_H
