#include <iostream>
#include <dlfcn.h>
#include "utils/utils.h"
#include "test/test.h"

using std::cout;
using std::cerr;
using std::endl;

void testPlugin() {
    void* handle = dlopen("./target/lib/testlib.so", RTLD_LAZY);
    if (!handle) {
        cerr << "failed to load plugin" << endl;
        exit(1);
    }

    auto testfunc = (test_t)dlsym(handle, "test");
    if (!testfunc) {
        cerr << "failed to find test symbol" << endl;
        dlclose(handle);

        exit(1);
    }

    testfunc();
}

int main() {
    cout << "hello auntie" << endl;
    testUtils();
    testPlugin();
    return 0;
}
