#include <iostream>
#include "asynclog.h"

using namespace std;

int main() {
    auto log = make_shared<Log>();
    log->traceVargs(true, __func__, __LINE__, 'I', "Hello world %d!\n", 1000);
    log->dump();
}
