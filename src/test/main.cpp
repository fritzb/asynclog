#include <iostream>
#include "asynclog.h"

int main() {
    Log *log = new Log();
    log->traceVargs(true, __func__, __LINE__, 'I', "Hello world %d!\n", 1000);
    log->dump();
    delete(log);

}
