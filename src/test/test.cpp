#include <iostream>
#include "../utils/utils.h"

extern "C" {
    int test(void) {
        std::cout << "hello nig" << std::endl;
        testUtils();

        return 0;
    }
}
