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
// Collect class
//

#include <unistd.h>
#include "stream.h"
#include "log.h"

using namespace memlog;

void Log::Collect::idle () {
    usleep(SLEEP_USEC);

    lastFlushCounter_++;
    if (lastFlushCounter_ > (FLUSH_IN_SEC *  1000000) / SLEEP_USEC) {
        log_->getStream()->flush();
        lastFlushCounter_ = 0;
    }
}

void Log::Collect::resetBookmark() {
    collectorBookmark_  = log_->getLastWrittenIndex();
}

void Log::Collect::setEnable(bool enabled) {
    if (enabled == enable_) {
        return;
    }

    if (enabled) {
        resetBookmark();
        enable_ = enabled;
        if (pthread_create(&collectorThread_, NULL, executeWorkerThread, this)) {
            throw new std::exception();
        }
    } else {
        flush();
        enable_ = enabled;
        pthread_join(collectorThread_, nullptr);
    }
}

void Log::Collect::setBufferThresholdPct(uint32_t value) {
    if (bufferThresholdPct_ == value) {
        return;
    }

    bufferThresholdPct_ = value;
}

uint32_t Log::Collect::getBufferThreshold() {
    return ((bufferThresholdPct_ * log_->ringBuffer_->size()) / 100);
}



uint32_t Log::Collect::collect () {
    uint32_t bookmarkEnd = log_->getLastWrittenIndex();

    if (log_->ringBuffer_->normalize(collectorBookmark_) == log_->ringBuffer_->normalize(bookmarkEnd)) {
        // Empty buffer
    } else if (log_->ringBuffer_->normalize(collectorBookmark_) <
               log_->ringBuffer_->normalize(bookmarkEnd)) {
        prevCollectRangeStart_ = collectorBookmark_;
        prevCollectRangeEnd_ = bookmarkEnd;
        bookmarkEnd = log_->dumpRange(collectorBookmark_, bookmarkEnd - 1, true, log_->getStream());
    } else {
        // Wrapped around
        uint32_t rangeEnd = bookmarkEnd;

        // Top buffer
        prevCollectRangeStart_ = collectorBookmark_;
        prevCollectRangeEnd_ = log_->ringBuffer_->size();
        bookmarkEnd = log_->dumpRange( collectorBookmark_, log_->ringBuffer_->size() - 1, true, log_->getStream());

        // Bottom buffer
        prevCollectRangeStart_ = 0;
        prevCollectRangeEnd_ = rangeEnd;
        bookmarkEnd = log_->dumpRange( 0, rangeEnd - 1, true, log_->getStream());
    }

    collectorBookmark_ = bookmarkEnd;

    return bookmarkEnd;
}

bool Log::Collect::shallCollect() {
    if (log_->ringBuffer_->normalize(log_->getLastWrittenIndex()) ==
        log_->ringBuffer_->normalize(collectorBookmark_)) {
        return false;
    }

    if ((log_->ringBuffer_->normalize(log_->getLastWrittenIndex()) -
         log_->ringBuffer_->normalize(collectorBookmark_)) > getBufferThreshold()) {
        return true;
    }

    /* Wrapped-around logs */
    uint32_t sizeBlockEnd = log_->ringBuffer_->normalize(0xffffffff) -
                              log_->ringBuffer_->normalize(collectorBookmark_);
    uint32_t sizeBlockStart = log_->ringBuffer_->normalize(log_->getLastWrittenIndex());

    if ((sizeBlockEnd + sizeBlockStart) > getBufferThreshold()) {
        return true;
    }

    return false;
}

void Log::Collect::workerThread() {
    while (getEnable()) {
        if (getEnable() && shallCollect()) {
            collect();
            continue;
        }
        idle();
    }

    flush();
}

bool Log::Collect::getEnable() const {
    return enable_;
}

void Log::Collect::flush(void) {
    collect();
    log_->stream_->flush();
}

void* Log::Collect::executeWorkerThread(void *ctx) {
    Collect* collect = (Collect *)ctx;
    collect->workerThread();
    return nullptr;
}

void Log::Collect::dumpState(char *buffer, int bufferLen) const {
    char *bufferNext = buffer;

    bufferNext += sprintf(bufferNext, "Collector State:\n");
    bufferNext += sprintf(bufferNext, "Bookmark: %u\n", collectorBookmark_);
    bufferNext += sprintf(bufferNext, "buffer threshold pct: %u\n", bufferThresholdPct_);
    bufferNext += sprintf(bufferNext, "Enabled: %u\n", getEnable());
    bufferNext += sprintf(bufferNext, "Prev collect range start: %u\n", prevCollectRangeStart_);
    bufferNext += sprintf(bufferNext, "Prev collect range end: %u\n", prevCollectRangeEnd_);
}

Log::Collect::Collect(Log *log, bool enable)
        : log_(log), bufferThresholdPct_(DEFAULT_BUFFER_THRESHOLD_PCT) {
    setEnable(enable);
}

Log::Collect::~Collect() {
    setEnable(false);
}
