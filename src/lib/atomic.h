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
// Atomic class
//

#ifndef MEMLOG_ATOMIC_H
#define MEMLOG_ATOMIC_H


#include <cstdint>

class Atomic64 {
public:
    Atomic64(uint64_t initial);
    uint64_t addAndGet(uint64_t add);

private:
    uint64_t atomic64Add(uint32_t *lowBit, uint32_t *highBit, uint32_t n);
    uint64_t value_;
};

class Atomic32 {
public:
    Atomic32(uint32_t initial);
    uint32_t addAndGet(uint32_t add);
    uint32_t getAndAdd(uint32_t add);

private:
    uint32_t value_;
};


#endif //ASYNCLOG_ATOMIC_H
