#include "allocator.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define HEAP_SIZE (1024u * 1024u)

/* ---------------------------------------------------------
 *   1. malloc: basic allocation and payload access
 *   --------------------------------------------------------- */

static void test_malloc_basic(void)
{
    Allocator* allocator = allocator_create();
    assert(allocator != NULL);

    void* ptr = allocator_malloc(allocator, 64);
    assert(ptr != NULL);

    memset(ptr, 0xAB, 64);

    for (size_t i = 0; i < 64; ++i) {
        assert(((unsigned char*)ptr)[i] == 0xAB);
    }

    allocator_free(allocator, ptr);
    allocator_destroy(allocator);

    puts("PASS: malloc basic");
}

/* ---------------------------------------------------------
 *   2. malloc: zero-size request
 *   --------------------------------------------------------- */

static void test_malloc_zero_size(void)
{
    Allocator* allocator = allocator_create();
    assert(allocator != NULL);

    assert(allocator_malloc(allocator, 0) == NULL);

    allocator_destroy(allocator);
    puts("PASS: malloc zero size");
}

/* ---------------------------------------------------------
 *   3. malloc: multiple distinct allocations
 *   --------------------------------------------------------- */

static void test_malloc_multiple(void)
{
    Allocator* allocator = allocator_create();
    assert(allocator != NULL);

    void* a = allocator_malloc(allocator, 64);
    void* b = allocator_malloc(allocator, 128);
    void* c = allocator_malloc(allocator, 256);

    assert(a != NULL);
    assert(b != NULL);
    assert(c != NULL);

    assert(a != b);
    assert(a != c);
    assert(b != c);

    memset(a, 0x11, 64);
    memset(b, 0x22, 128);
    memset(c, 0x33, 256);

    for (size_t i = 0; i < 64; ++i)
        assert(((unsigned char*)a)[i] == 0x11);

    for (size_t i = 0; i < 128; ++i)
        assert(((unsigned char*)b)[i] == 0x22);

    for (size_t i = 0; i < 256; ++i)
        assert(((unsigned char*)c)[i] == 0x33);

    allocator_free(allocator, b);
    allocator_free(allocator, a);
    allocator_free(allocator, c);

    allocator_destroy(allocator);
    puts("PASS: malloc multiple allocations");
}

/* ---------------------------------------------------------
 *   4. malloc: oversized request
 *   --------------------------------------------------------- */

static void test_malloc_oversized(void)
{
    Allocator* allocator = allocator_create();
    assert(allocator != NULL);

    assert(allocator_malloc(allocator, HEAP_SIZE) == NULL);
    assert(allocator_malloc(allocator, 2u * HEAP_SIZE) == NULL);

    allocator_destroy(allocator);
    puts("PASS: malloc oversized request");
}

/* ---------------------------------------------------------
 *   5. calloc: zero initialization
 *   --------------------------------------------------------- */

static void test_calloc_zero_initialization(void)
{
    Allocator* allocator = allocator_create();
    assert(allocator != NULL);

    size_t count = 128;
    size_t object_size = sizeof(int);

    int* ptr = allocator_calloc(allocator, count, object_size);
    assert(ptr != NULL);

    for (size_t i = 0; i < count; ++i) {
        assert(ptr[i] == 0);
    }

    allocator_free(allocator, ptr);
    allocator_destroy(allocator);

    puts("PASS: calloc zero initialization");
}

/* ---------------------------------------------------------
 *   6. calloc: zero arguments
 *   --------------------------------------------------------- */

static void test_calloc_zero_arguments(void)
{
    Allocator* allocator = allocator_create();
    assert(allocator != NULL);

    assert(allocator_calloc(allocator, 0, sizeof(int)) == NULL);
    assert(allocator_calloc(allocator, 10, 0) == NULL);
    assert(allocator_calloc(allocator, 0, 0) == NULL);

    allocator_destroy(allocator);
    puts("PASS: calloc zero arguments");
}

/* ---------------------------------------------------------
 *   7. calloc: multiplication overflow
 *   --------------------------------------------------------- */

static void test_calloc_overflow(void)
{
    Allocator* allocator = allocator_create();
    assert(allocator != NULL);

    assert(allocator_calloc(allocator, SIZE_MAX, 2) == NULL);

    allocator_destroy(allocator);
    puts("PASS: calloc overflow");
}

/* ---------------------------------------------------------
 *   8. calloc: allocator exhaustion
 *   --------------------------------------------------------- */

static void test_calloc_oversized(void)
{
    Allocator* allocator = allocator_create();
    assert(allocator != NULL);

    assert(allocator_calloc(allocator, HEAP_SIZE, 2) == NULL);

    allocator_destroy(allocator);
    puts("PASS: calloc oversized request");
}

/* ---------------------------------------------------------
 *   9. free: reuse released memory
 *   --------------------------------------------------------- */

static void test_free_and_reuse(void)
{
    Allocator* allocator = allocator_create();
    assert(allocator != NULL);

    void* first = allocator_malloc(allocator, 256);
    assert(first != NULL);

    allocator_free(allocator, first);

    void* second = allocator_malloc(allocator, 256);
    assert(second != NULL);

    /* First-fit should reuse the released block. */
    assert(first == second);

    allocator_free(allocator, second);
    allocator_destroy(allocator);

    puts("PASS: free and reuse");
}

/* ---------------------------------------------------------
 *   10. free: coalesce blocks freed in allocation order
 *   --------------------------------------------------------- */

static void test_free_coalesce_forward(void)
{
    Allocator* allocator = allocator_create();
    assert(allocator != NULL);

    void* a = allocator_malloc(allocator, 128);
    void* b = allocator_malloc(allocator, 128);
    void* c = allocator_malloc(allocator, 128);

    assert(a && b && c);

    allocator_free(allocator, a);
    allocator_free(allocator, b);
    allocator_free(allocator, c);

    void* large = allocator_malloc(allocator, HEAP_SIZE - 64);
    assert(large != NULL);

    allocator_free(allocator, large);
    allocator_destroy(allocator);

    puts("PASS: free coalesce forward");
}

/* ---------------------------------------------------------
 *   11. free: coalesce blocks freed in reverse order
 *   --------------------------------------------------------- */

static void test_free_coalesce_reverse(void)
{
    Allocator* allocator = allocator_create();
    assert(allocator != NULL);

    void* a = allocator_malloc(allocator, 128);
    void* b = allocator_malloc(allocator, 128);
    void* c = allocator_malloc(allocator, 128);

    assert(a && b && c);

    allocator_free(allocator, c);
    allocator_free(allocator, b);
    allocator_free(allocator, a);

    void* large = allocator_malloc(allocator, HEAP_SIZE - 64);
    assert(large != NULL);

    allocator_free(allocator, large);
    allocator_destroy(allocator);

    puts("PASS: free coalesce reverse");
}

/* ---------------------------------------------------------
 *   12. realloc: NULL pointer behaves like malloc
 *   --------------------------------------------------------- */

static void test_realloc_null_pointer(void)
{
    Allocator* allocator = allocator_create();
    assert(allocator != NULL);

    void* ptr = allocator_realloc(allocator, NULL, 128);
    assert(ptr != NULL);

    allocator_free(allocator, ptr);
    allocator_destroy(allocator);

    puts("PASS: realloc NULL pointer");
}

/* ---------------------------------------------------------
 *   13. realloc: zero size frees the allocation
 *   --------------------------------------------------------- */

static void test_realloc_zero_size(void)
{
    Allocator* allocator = allocator_create();
    assert(allocator != NULL);

    void* ptr = allocator_malloc(allocator, 128);
    assert(ptr != NULL);

    assert(allocator_realloc(allocator, ptr, 0) == NULL);

    /* The released space should be reusable. */
    void* large = allocator_malloc(allocator, HEAP_SIZE - 64);
    assert(large != NULL);

    allocator_free(allocator, large);
    allocator_destroy(allocator);

    puts("PASS: realloc zero size");
}

/* ---------------------------------------------------------
 *   14. realloc: shrink and preserve data
 *   --------------------------------------------------------- */

static void test_realloc_shrink(void)
{
    Allocator* allocator = allocator_create();
    assert(allocator != NULL);

    unsigned char* ptr = allocator_malloc(allocator, 512);
    assert(ptr != NULL);

    for (size_t i = 0; i < 128; ++i) {
        ptr[i] = (unsigned char)i;
    }

    unsigned char* resized = allocator_realloc(allocator, ptr, 128);
    assert(resized != NULL);

    for (size_t i = 0; i < 128; ++i) {
        assert(resized[i] == (unsigned char)i);
    }

    allocator_free(allocator, resized);
    allocator_destroy(allocator);

    puts("PASS: realloc shrink");
}

/* ---------------------------------------------------------
 *   15. realloc: grow into adjacent free space
 *   --------------------------------------------------------- */

static void test_realloc_grow_in_place(void)
{
    Allocator* allocator = allocator_create();
    assert(allocator != NULL);

    unsigned char* ptr = allocator_malloc(allocator, 128);
    assert(ptr != NULL);

    memset(ptr, 0x5A, 128);

    /*
     *       The initial free region follows ptr, so the allocator
     *       should be able to grow this allocation in place.
     */
    unsigned char* resized = allocator_realloc(allocator, ptr, 512);
    assert(resized != NULL);

    /* In-place growth should preserve the address. */
    assert(resized == ptr);

    for (size_t i = 0; i < 128; ++i) {
        assert(resized[i] == 0x5A);
    }

    allocator_free(allocator, resized);
    allocator_destroy(allocator);

    puts("PASS: realloc grow in place");
}

/* ---------------------------------------------------------
 *   16. realloc: relocate when the next block is allocated
 *   --------------------------------------------------------- */

static void test_realloc_relocate(void)
{
    Allocator* allocator = allocator_create();
    assert(allocator != NULL);

    unsigned char* a = allocator_malloc(allocator, 128);
    void* blocker = allocator_malloc(allocator, 128);

    assert(a != NULL);
    assert(blocker != NULL);

    for (size_t i = 0; i < 128; ++i) {
        a[i] = (unsigned char)(i ^ 0xA5);
    }

    unsigned char* resized = allocator_realloc(allocator, a, 4096);
    assert(resized != NULL);

    /*
     *       The allocated blocker prevents in-place growth.
     *       Relocation should preserve the original payload.
     */
    for (size_t i = 0; i < 128; ++i) {
        assert(resized[i] == (unsigned char)(i ^ 0xA5));
    }

    allocator_free(allocator, resized);
    allocator_free(allocator, blocker);
    allocator_destroy(allocator);

    puts("PASS: realloc relocation");
}

/* ---------------------------------------------------------
 *   17. realloc: failed growth preserves the original block
 *   --------------------------------------------------------- */

static void test_realloc_failure_preserves_original(void)
{
    Allocator* allocator = allocator_create();
    assert(allocator != NULL);

    unsigned char* ptr = allocator_malloc(allocator, 128);
    assert(ptr != NULL);

    memset(ptr, 0xC3, 128);

    void* resized = allocator_realloc(allocator, ptr, HEAP_SIZE);
    assert(resized == NULL);

    /*
     *       realloc failure must leave the original allocation valid.
     */
    for (size_t i = 0; i < 128; ++i) {
        assert(ptr[i] == 0xC3);
    }

    allocator_free(allocator, ptr);
    allocator_destroy(allocator);

    puts("PASS: realloc failure preserves original");
}

/* ---------------------------------------------------------
 *   18. realloc: reject size overflow
 *   --------------------------------------------------------- */

static void test_realloc_size_overflow(void)
{
    Allocator* allocator = allocator_create();
    assert(allocator != NULL);

    void* ptr = allocator_malloc(allocator, 128);
    assert(ptr != NULL);

    assert(allocator_realloc(allocator, ptr, SIZE_MAX) == NULL);

    /* The original allocation should remain valid. */
    memset(ptr, 0x7B, 128);

    for (size_t i = 0; i < 128; ++i) {
        assert(((unsigned char*)ptr)[i] == 0x7B);
    }

    allocator_free(allocator, ptr);
    allocator_destroy(allocator);

    puts("PASS: realloc size overflow");
}

/* ---------------------------------------------------------
 *   19. reset: restore the initial heap
 *   --------------------------------------------------------- */

static void test_reset(void)
{
    Allocator* allocator = allocator_create();
    assert(allocator != NULL);

    void* a = allocator_malloc(allocator, 128);
    void* b = allocator_malloc(allocator, 256);

    assert(a && b);

    allocator_reset(allocator);

    /*
     *       Do not use or free a or b after reset:
     *       reset invalidates the previous allocation state.
     */
    void* large = allocator_malloc(allocator, HEAP_SIZE - 64);
    assert(large != NULL);

    allocator_free(allocator, large);
    allocator_destroy(allocator);

    puts("PASS: reset");
}

/* ---------------------------------------------------------
 *   Test runner
 *   --------------------------------------------------------- */

int main(void)
{
    test_malloc_basic();
    test_malloc_zero_size();
    test_malloc_multiple();
    test_malloc_oversized();

    test_calloc_zero_initialization();
    test_calloc_zero_arguments();
    test_calloc_overflow();
    test_calloc_oversized();

    test_free_and_reuse();
    test_free_coalesce_forward();
    test_free_coalesce_reverse();

    test_realloc_null_pointer();
    test_realloc_zero_size();
    test_realloc_shrink();
    test_realloc_grow_in_place();
    test_realloc_relocate();
    test_realloc_failure_preserves_original();
    test_realloc_size_overflow();

    test_reset();

    puts("\nAll allocator tests passed.");
    return 0;
}
