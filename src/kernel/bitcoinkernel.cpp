#include <kernel/bitcoinkernel.h>

#include <iostream>

void HelloKernel() {
    LOCK(::cs_main);
    std::cout << "Hello Kernel!";
}
