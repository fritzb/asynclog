# asynclog
High performance thread-safe in-memory logging with lockless data structure, no system call log producer and asynchronous file write.

Example:
```
int main() {
    auto log = std::make_shared<Log>();
    log->traceVargs(true, __func__, __LINE__, 'I', "Hello world %d!\n", 1000L);
    log->dump();
}
```
