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
// Stream implementation
//

#include <cstring>
#include <cstdlib>
#include <exception>
#include <memory>
#include "stream.h"

Stream::Stream() {
}


void Stream::dumpState(char *buffer, int length) const {
    buffer += sprintf(buffer, "\n");
    buffer += sprintf(buffer, "Stream stats:\n");
    buffer += sprintf(buffer, "Zlib error: %u\n", stats.zlibError);
    buffer += sprintf(buffer, "Zlib error no: %d\n", stats.zlibErrorNo);
    buffer += sprintf(buffer, "Zlib deflate init error: %u\n", stats.zlibDeflateInitError);
    buffer += sprintf(buffer, "Zlib deflate error: %u\n", stats.zlibDeflateError);
    buffer += sprintf(buffer, "IO error no: %u\n", stats.ioErrorNo);
    buffer += sprintf(buffer, "IO error: %u\n", stats.ioError);
    buffer += sprintf(buffer, "IO write error: %u\n", stats.ioWriteError);
    buffer += sprintf(buffer, "IO read error: %u\n", stats.ioReadError);
    buffer += sprintf(buffer, "IO seek error: %u\n", stats.ioSeekError);
    buffer += sprintf(buffer, "Compress error: %u\n", stats.compressError);
    buffer += sprintf(buffer, "Flush write count: %u\n", stats.flushWriteCount);
}

void Stream::write(char *s, unsigned len) {
    size_t written;

    written = fwrite(s, 1, len, fileHandle_);
    if (written != len || ferror(fileHandle_)) {
        stats.ioWriteError++;
    }
}

void Stream::cleanup() {
    flush();
    fileHandle_ = nullptr;
}

int Stream::setFile(FILE *file) {
    if (file == fileHandle_) {
        return 0;
    }

    flush();
    fileHandle_ = file;

    return 0;
}

Stream::Stats Stream::getStats() {
    return stats;
}

int Stream::flush() {
    if (fileHandle_) {
        fflush(fileHandle_);
    }
    return 0;
}

Stream::~Stream() {
    cleanup();
}

std::shared_ptr<Stream> Stream::create(FILE *file, Stream::CompressedMode mode) {
    int ret;
    std::shared_ptr<Stream> ctx;


    switch (mode) {
        case UNCOMPRESSED:
            ctx = std::make_shared<Stream>();
            if (!ctx) {
                return nullptr;
            }
            break;

        case BUFFERED_UNCOMPRESS:
            ctx = std::make_shared<StreamBuffered>();
            if (!ctx) {
                return nullptr;
            }
            break;
    }

    // Set FILE
    ret = ctx->setFile(file);
    if (ret != 0) {
        return nullptr;
    }
    return ctx;
}

int StreamBuffered::bufferHasRoom(unsigned int len) {
    unsigned int left = bufferSize - bufferIdx;
    return (len <= left);
}

std::shared_ptr<Stream> Stream::stdoutStream = nullptr;


StreamBuffered::StreamBuffered() {
    unsigned buffer_size = LOG_STREAM_BUFFER_SIZE;
    buffer = new char(buffer_size);
    buffer_size = LOG_STREAM_BUFFER_SIZE;
    if (!buffer) {
        throw new std::exception();
    }

    bufferIdx = 0;
}

unsigned int StreamBuffered::availableData() {
    return bufferIdx;
}

int StreamBuffered::flush() {
    size_t written;
    size_t dataInBuffer = (size_t) availableData();

    if (!fileHandle_) {
        return 0;
    }

    stats.flushWriteCount++;

    written = fwrite(buffer, 1, dataInBuffer, fileHandle_);
    if (written != dataInBuffer || ferror(fileHandle_)) {
        stats.ioErrorNo = 0;
        stats.ioWriteError++;
        return -1;
    }

    bufferIdx = 0;
    return 0;
}

unsigned StreamBuffered::getBufferSize() {
    return (bufferSize);
}

void StreamBuffered::cleanup() {
    if (buffer) {
        delete[] buffer;
        buffer = nullptr;
    }

}

void StreamBuffered::write(char *s, unsigned len) {
    if (len > getBufferSize()) {
        throw new std::exception();
    }

    if (!bufferHasRoom(len)) {
        flush();
    }

    memcpy(&buffer[bufferIdx], s, len);
    bufferIdx += len;
}

int StreamBuffered::empty() {
    return (bufferIdx == 0);
}


void CompressedBuffered::cleanup() {
    if (compressedBuffer) {
        delete[] compressedBuffer;
        compressedBuffer = nullptr;
    }
}

CompressedBuffered::CompressedBuffered()
        : compressedBuffer(new char[LOG_STREAM_COMPRESSED_BUFFER_SIZE]),
          compressedBufferSize(LOG_STREAM_COMPRESSED_BUFFER_SIZE) {
}
