#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void lauf_panic(const char* msg)
{
    fprintf(stderr, "[lauf] panic: %s\n", msg);
    abort();
}

void* lauf_heap_alloc(uint64_t size, uint64_t alignment)
{
#ifdef _MSC_VER
    alignment = (alignment > 0) && ((alignment & (alignment - 1)) == 0) ? alignment : 16;
    return _aligned_malloc(size, alignment);
#else
    return aligned_alloc(alignment, size);
#endif
}

void* lauf_heap_alloc_array(uint64_t count, uint64_t size, uint64_t alignment)
{
    // See lauf::round_to_multiple_of_alignment().
    uint64_t rounded_size = (size + alignment - 1) & ~(alignment - 1);
    return lauf_heap_alloc(count * rounded_size, alignment);
}

void lauf_heap_free(void* ptr)
{
#ifdef _MSC_VER
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

uint64_t lauf_heap_gc(void)
{
    return 0;
}

void lauf_memory_copy(void* dest, void* src, uint64_t count)
{
    memmove(dest, src, count);
}

void lauf_memory_fill(void* dest, uint64_t byte, uint64_t count)
{
    memset(dest, (unsigned char)byte, count);
}

int64_t lauf_memory_cmp(void* lhs, void* rhs, uint64_t count)
{
    return memcmp(lhs, rhs, count);
}

