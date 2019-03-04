cmake_minimum_required(VERSION 3.10)
project(asynclog)

set(CMAKE_CXX_STANDARD 14)

include_directories(src/lib)

add_library(asynclog
        src/lib/asynclog.cpp
        src/lib/asynclog.h src/lib/ringbuffer.cpp src/lib/ringbuffer.h src/lib/atomic.cpp src/lib/atomic.h src/lib/stream.cpp src/lib/stream.h src/lib/collector.cpp src/lib/stringformat.cpp src/lib/stringformat.h)

add_executable(asynclogTest
        src/lib/asynclog.cpp
        src/lib/asynclog.h
        src/test/main.cpp src/lib/ringbuffer.cpp src/lib/ringbuffer.h src/lib/atomic.cpp src/lib/atomic.h src/lib/stream.cpp src/lib/stream.h src/lib/collector.cpp src/lib/stringformat.cpp src/lib/stringformat.h)

target_link_libraries(asynclogTest pthread)

