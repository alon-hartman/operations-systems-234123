#include <unistd.h>

void* smalloc(size_t size) {
    if(size == 0 || size > 100000000 /*10^8*/) {
        return nullptr;
    }
    void* res = sbrk(size);
    if((int)res == -1) {
        nullptr;
    }
    return res;
}