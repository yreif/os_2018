#include "VirtualMemory.h"

#include <cstdio>
#include <cassert>

int main(int argc, char **argv) {
    VMinitialize();
    printf("number of pages is %llu\n", (long long int) NUM_PAGES);
    for (uint64_t i = 0; i < (2 * NUM_FRAMES) ; ++i) { // (2 * NUM_FRAMES)
        printf("writing to %llu\n", (long long int) i);
        VMwrite(5 * i * PAGE_SIZE, i);
    }

    for (uint64_t i = 0; i < (2 * NUM_FRAMES); ++i) {
        word_t value;
        printf("reading from %llu\n", (long long int) i);
        VMread(5 * i * PAGE_SIZE, &value);
        printf("read from %llu %d\n", (long long int) i, value);
        assert(uint64_t(value) == i);
    }
    printf("success\n");

    return 0;
}