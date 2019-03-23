#include <iostream>
#include <chrono>
#include "log.h"

using namespace std;
using namespace memlog;

void performance_test1(shared_ptr<Log> log) {
    chrono::high_resolution_clock::time_point t1 = chrono::high_resolution_clock::now();
    for ( auto i = 0; i < 1000000; i++) {
        log->traceVargs(true, __func__, __LINE__, 'I', "Hello world %d!\n", i);
    }
    chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();

    auto duration = chrono::duration_cast<chrono::microseconds>( t2 - t1 ).count();
    cout << "1 millions write in " << duration << " microseconds" << endl;
}

int main() {
    auto log = std::make_shared<Log>();
    log->info("Hello world %d!\n", 1000L);
    log->info("Hello world %d!\n", 1001L);
    log->info("Hello world %d!\n", 1002L);
    log->dump();
    //performance_test1(log);
}
