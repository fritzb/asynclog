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


//
// RingBuffer class
//
#include <cstring>
#include "ringbuffer.h"

RingBuffer::RingBuffer(unsigned int size, bool threadSafe)
        : size_(size), buffer_(new uint8_t[size]()), marker_(MARKER), version_(VERSION), threadSafe_(threadSafe) {
}

RingBuffer::~RingBuffer() {
    delete[] buffer_;
}

RingBuffer::Stats RingBuffer::getStats() const {
    return stats_;
}

// Allocate a space in ring buffer
uint32_t RingBuffer::allocate(unsigned int bufferLen)
{
    uint32_t ret;

    // For performance reason, check the wrap around index only when it is about to wrap around.
    if (isIndexRollingOver(currentIndex_)) {
        if (threadSafe_) {
            bool writeSuccess = false;
            uint32_t new_wrap_count = stats_.indexWrapAroundCount + 1;
            while (isIndexRollingOver(currentIndex_) && !writeSuccess) {
                uint32_t currentIndex = currentIndex_;
                uint32_t currentIndexNormalized = normalize(currentIndex);
                writeSuccess = __sync_bool_compare_and_swap(&(currentIndex_), currentIndex, currentIndexNormalized);
                if (writeSuccess) {
                    stats_.indexWrapAroundCount = new_wrap_count;
                }
            }
        } else {
            currentIndex_ = normalize(currentIndex_);
            stats_.indexWrapAroundCount++;
        }
    }

    if (threadSafe_) {
        ret = __sync_fetch_and_add(&(currentIndex_), bufferLen);
    } else {
        ret = currentIndex_;
        currentIndex_ += bufferLen;
    }

    return ret;
}

uint32_t RingBuffer::getCurrentIndex() const
{
    return currentIndex_;
}

bool RingBuffer::hasWrappedAround() const
{
    return stats_.indexWrapAroundCount > 0;
}

// Get a byte array from the stack
void RingBuffer::get (uint8_t *dst, uint32_t srcIndex, unsigned int length)
{
    srcIndex = normalize(srcIndex);

    // Non-wrap around case
    if (srcIndex + length <= size()) {
        memcpy(dst, &(buffer_[srcIndex]), length);
        return;
    }

    // Wrap around case
    uint32_t copyLength = size() - srcIndex;
    memcpy(dst, &(buffer_[srcIndex]), copyLength);
    memcpy(dst + copyLength, &(buffer_[0]), length - copyLength);
}

uint8_t RingBuffer::getByte(uint32_t dstIndex)
{
    dstIndex = normalize(dstIndex);
    return buffer_[dstIndex];
}

uint32_t RingBuffer::getInt(uint32_t dstIndex)
{
    uint32_t u32;
    get((uint8_t *)&u32, dstIndex, sizeof(u32));
    return u32;
}

double RingBuffer::getDouble(uint32_t dstIndex)
{
    double d;
    get((uint8_t *)&d, dstIndex, sizeof(d));
    return d;
}

void * RingBuffer::getPtr(uint32_t dstIndex)
{
    void *ptr;
    get((uint8_t *)&ptr, dstIndex, sizeof(ptr));
    return ptr;
}

uint64_t RingBuffer::getLong64(uint32_t dstIndex)
{
    uint64_t u64;
    get((uint8_t *)&u64, dstIndex, sizeof(u64));
    return u64;
}

uint32_t RingBuffer::getString(uint32_t bufferIndex, char *dst)
{
    uint32_t di = 0;
    bufferIndex = normalize(bufferIndex);

    while (buffer_[bufferIndex] != 0 && di < size()) {
        dst[di] = buffer_[bufferIndex];
        di++;
        bufferIndex++;
        bufferIndex = normalize(bufferIndex);
    }
    return di;
}

// Copy bytes into the circular buffer
void RingBuffer::set(uint32_t dstIndex, uint8_t *src, unsigned int length) {
    dstIndex = normalize(dstIndex);
    // Non-wrap around
    if (dstIndex + length <= size()) {
        memcpy(&(buffer_[dstIndex]), src, length);
        return;
    }
    // Wrap around
    uint32_t copyLength = size() - dstIndex;
    memcpy(&(buffer_[dstIndex]), src, copyLength);
    memcpy(&(buffer_[0]), src+copyLength, length - copyLength);
}

// Compare bytes from the circular buffer
int RingBuffer::compare(uint32_t bufferIndex, uint8_t *p, unsigned int length)
{
    uint32_t di = 0;
    bufferIndex = normalize(bufferIndex);

    while (length) {
        if (buffer_[bufferIndex] != p[di]) {
            return -1;
        }
        length--;
        di++;
        bufferIndex++;
        bufferIndex = normalize(bufferIndex);
    }
    return 0;
}
