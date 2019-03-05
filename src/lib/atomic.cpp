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

#include "atomic.h"

Atomic64::Atomic64(uint64_t initial) :
        value_(initial) {
}

uint64_t Atomic64::atomic64Add(uint32_t *lowBit, uint32_t *highBit, uint32_t n)
{
    uint32_t currentLow, current_high = 0;
    uint64_t result = 0;

    currentLow = __sync_fetch_and_add(lowBit, n);
    if (UINT32_MAX - currentLow < n) {
        current_high = __sync_add_and_fetch(highBit, 1);
    }
    result = current_high;
    result <<= 32;
    result |= ((currentLow + n) & UINT32_MAX);

    return result;
}

uint64_t Atomic64::addAndGet(uint64_t add) {
    uint32_t *valuePtr = (uint32_t *)&value_;
    uint32_t *lowPtr = valuePtr;
    uint32_t *highPtr = valuePtr + 1;
    return atomic64Add(lowPtr, highPtr, add);
}

Atomic32::Atomic32(uint32_t initial) :
        value_(initial) {
}

uint32_t Atomic32::addAndGet(uint32_t add) {
    uint32_t newValue;
    bool casSuccess;
    do {
        uint32_t currentValue = value_;
        newValue = currentValue + add;
        casSuccess = __sync_bool_compare_and_swap(&value_, currentValue, newValue);
    } while (!casSuccess);

    return newValue;
}
