# memlog
High performance thread-safe in-memory logging with lockless ring buffer data structure.
The library is efficiently fast and completely non-blocking (true non-blocking as no system call are used when producing the log). The in-memory buffer can be persisted to a log file asynchronously.

Example:
```
#include "log.h"

int main() {
    auto log = std::make_shared<Log>();
    log->info("Hello world %d!\n", 1000L);
    log->dump();
}
```

Output:
```
[2019 Mar  4 17:36:16.442382600:0:I:main:6] Hello world 1000!
```

File output:
```
cat rxtrace.txt
[2019 Mar  4 17:36:16.442382600:0:I:main:6] Hello world 1000!
```

# benchmark

Memlog is 2x faster than calling sprintf library. Memlog stores traces in the memory for later retreival. Optionaly, the buffer can be spill to disk asynchronously.

```
MEMLOG
test: log->info("Hello world %d!\n", i)
memlog: 10M logs/sec on Xeon servers
memlog: 2M logs/sec on Macbook Air

SPRINTF:
test: sprintf(buffer, "Hello world %d!\n", i)
sprintf: 5M logs/sec on Xeon servers
```

```
void performance_test1(shared_ptr<Log> log) {
    chrono::high_resolution_clock::time_point t1 = chrono::high_resolution_clock::now();
    for (auto i = 0; i < 1000000; i++) {
        log->info("Hello world %d!\n", i);
    }
    chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();

    auto duration = chrono::duration_cast<chrono::microseconds>(t2 - t1).count();
    cout << "1 millions write in " << duration << " microseconds" << endl;
}


./memlogTest
[2019 Mar  5 03:12:14.760732000:0:I:main:20] Hello world 1000!
[2019 Mar  5 03:12:14.760766000:1:I:main:21] Hello world 1001!
[2019 Mar  5 03:12:14.760766000:2:I:main:22] Hello world 1002!
1 millions write in 253829 microseconds

Process finished with exit code 0
```
