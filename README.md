# asynclog
High performance thread-safe in-memory logging with lockless ring buffer data structure.
The library is efficiently fast and non-blocking (Really non-blocking as no system call will be used when producing the log)  The in-memory buffer will be persistet to a log file asynchronously via a background thread.

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
[2019 Mar  4 17:36:16.442382600:0:I:main:6] Hello world 1000!!
```

File output:
```
cat rxtrace.txt
[2019 Mar  4 17:36:16.442382600:0:I:main:6] Hello world 1000!!
```

# benchmark

1 millions log took 350ms
