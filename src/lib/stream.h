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
// Stream class
//

#ifndef MEMLOG_STREAM_H
#define MEMLOG_STREAM_H

#include <stdint.h>
#include <stdio.h>
#include <memory>
#include "stream.h"

class Stream {
public:
    static const unsigned int LOG_STREAM_BUFFER_SIZE = 1024*1024;
    static const unsigned int LOG_STREAM_COMPRESSED_BUFFER_SIZE = 1024*1024;

    static std::shared_ptr<Stream> stdoutStream;
    static std::shared_ptr<Stream> getStdoutStream() {
        if (stdoutStream == nullptr) {
            stdoutStream = create(stdout, UNCOMPRESSED);
        }
        return stdoutStream;
    }

    enum CompressedMode {
        UNCOMPRESSED,
        BUFFERED_UNCOMPRESS,
        DEFAULT=UNCOMPRESSED
    };

    struct Stats {
        int zlibErrorNo;
        unsigned int zlibError;
        unsigned int zlibDeflateInitError;
        unsigned int zlibDeflateError;
        unsigned int ioErrorNo;
        unsigned int ioError;
        unsigned int ioWriteError;
        unsigned int ioReadError;
        unsigned int ioSeekError;
        unsigned int compressError;
        unsigned int flushWriteCount;
    };
    Stream::Stats stats;

    static std::shared_ptr<Stream> create(FILE *file, Stream::CompressedMode mode=UNCOMPRESSED);

    void write(char *s, unsigned len);
    int flush();
    Stream::Stats getStats();
    void dumpState(char *buffer, int length) const;

    Stream();
    ~Stream();

protected:
    FILE* fileHandle_;
    virtual void cleanup();
    int setFile(FILE *file);
};

class StreamBuffered : public Stream {
public:
    int flush();
    void write(char *s, unsigned len);
    StreamBuffered();

protected:
    int empty();
    void cleanup();
    int bufferHasRoom(unsigned len);
    unsigned getBufferSize();

private:
    char *buffer;
    unsigned bufferSize;
    unsigned bufferIdx;
    unsigned int availableData();
};


class CompressedBuffered : public Stream {
public:
    CompressedBuffered();

protected:
    void cleanup();
    char *compressedBuffer;

private:
    unsigned int compressedBufferSize;
};

#endif //ASYNCLOG_STREAM_H
