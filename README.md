# asynclog
High performance thread-safe in-memory logging with lockless ring buffer data structure, no system call log producer and asynchronous file write.

Example:
```
#include "asynclog.h"

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
