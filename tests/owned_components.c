#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

void *__real_malloc(size_t size);
void __real_free(void *pointer);
int argos_entry(void);
static void *allocations[8192];
static size_t allocated, live;

void *__wrap_malloc(size_t size) {
    void *pointer = __real_malloc(size);
    if (!pointer || allocated == 8192) abort();
    allocations[allocated++] = pointer;
    ++live;
    return pointer;
}

void __wrap_free(void *pointer) {
    if (!pointer) return;
    for (size_t i = 0; i < allocated; ++i) {
        if (allocations[i] == pointer) {
            allocations[i] = NULL;
            --live;
            __real_free(pointer);
            return;
        }
    }
    fputs("double free or allocation not owned by runtime\n", stderr);
    abort();
}

static void verify_cleanup(void) {
    if (live != 0 || allocated < 8) {
        fprintf(stderr, "invalid ownership cleanup: allocated=%zu live=%zu\n", allocated, live);
        _Exit(97);
    }
}

int main(void) {
    if (atexit(verify_cleanup) != 0) return 98;
    return argos_entry();
}
