# memlog
High performance thread-safe in-memory logging with lockless ring buffer data structure.
The library is efficiently fast and completely non-blocking (true non-blocking as no system call will be used when producing the log). The in-memory buffer will be persisted to a log file asynchronously.

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

250ms for 1M logs on Macbook Air i5
```
./memlogTest
[2019 Mar  5 03:12:14.760732000:0:I:main:20] Hello world 1000!
[2019 Mar  5 03:12:14.760766000:1:I:main:21] Hello world 1001!
[2019 Mar  5 03:12:14.760766000:2:I:main:22] Hello world 1002!
1 millions write in 253829 microseconds

Process finished with exit code 0
```
