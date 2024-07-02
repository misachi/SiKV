#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "alloc.h"

void test_KV_alloc_pool_init()
{
    size_t size = MIN_ALLOCATION_POOL_SIZE;
    struct KV_alloc_pool *pool = KV_alloc_pool_init(size);

    memset(pool->data, -1, size);

    assert(pool->size == size);
    assert(pool->data != NULL);
    assert(pool->data[size - 1] == -1);

    KV_alloc_pool_free(pool);
}

void test_KV_malloc()
{
    size_t size = MIN_ALLOCATION_POOL_SIZE;
    struct KV_alloc_pool *pool = KV_alloc_pool_init(size);

    size_t alloc_size = 32;
    char *alloc = KV_malloc(pool, alloc_size);

    assert(alloc != NULL);
    assert(pool->offset == (alloc_size + ALLOCATION_SIZE_OVERHEAD));

    assert(*(uint64_t *)(alloc - 8) == (alloc_size + ALLOCATION_SIZE_OVERHEAD));

    char *alloc2 = KV_malloc(pool, alloc_size);
    assert(alloc2 != NULL);
    assert(pool->offset == ((alloc_size + ALLOCATION_SIZE_OVERHEAD) * 2));

    assert(((alloc2 + alloc_size) - pool->data) == 80);

    KV_alloc_pool_free(pool);
}

void test_KV_free()
{
    size_t size = MIN_ALLOCATION_POOL_SIZE;
    struct KV_alloc_pool *pool = KV_alloc_pool_init(size);

    size_t alloc_size = 32;
    char *alloc = KV_malloc(pool, alloc_size);

    KV_free(pool, alloc);
    const char *item = get_freelist_item(1);

    assert(item != NULL);            // Check if chunk is placed in the right position in freelist
    assert(*(uint64_t *)item == 40); // Check size
    assert(*(char **)(item + 8) == NULL); // Previous chunk should be null; This is the head
    assert(*(char **)(item + 16) == NULL); // Next chunk should be null; We haven't free'd other similar sized chunks

    char *alloc2 = KV_malloc(pool, alloc_size);  // We'll get allocation from freelist

    item = get_freelist_item(1);
    assert(item == NULL); // We got our allocation from the freelist; The class should be empty now

    KV_free(pool, alloc2);

    // assert(*(char **)(item + 8) != NULL); // Next chunk should be null; We haven't free'd other similar sized chunks
    KV_alloc_pool_free(pool);
}

void test_KV_freelist_class_linked_list()
{
    size_t size = MIN_ALLOCATION_POOL_SIZE;
    struct KV_alloc_pool *pool = KV_alloc_pool_init(size);

    size_t alloc_size = 40;
    char *alloc = KV_malloc(pool, alloc_size);
    char *alloc2 = KV_malloc(pool, alloc_size);

    // Garbage data we're going to test for later
    memset(alloc, -1, alloc_size);
    memset(alloc2, -2, alloc_size);

    KV_free(pool, alloc);
    KV_free(pool, alloc2);

    const char *item = get_freelist_item(2);
    assert(item != NULL);

    assert(*(uint64_t *)item == 48); // Check size
    assert(*(char **)(item + 8) == NULL); // Previous chunk should be null; Last allocation makes the new head
    assert(*(char **)(item + 16) != NULL); // Next chunk should be non-empty; We free'd 2 allocations of the same size
    
    char *next_alloc = *(char **)(item + 16);
    assert(next_alloc != NULL);
    char *prev_alloc = *(char **)(next_alloc + 8); // Get head pointer from pointer madness; Just want to know if the pointers are placed correctly

    // Check that the chunks are arranged correctly in the freelist class; We use the garbage data we put on the chunks
    assert(*(next_alloc+ALLOCATION_SIZE_OVERHEAD+MAX_ALLOCATION_OVERHEAD) == -1);
    assert(*(prev_alloc+ALLOCATION_SIZE_OVERHEAD+MAX_ALLOCATION_OVERHEAD) == -2);

    assert(*(uint64_t *)next_alloc == 48); // Check size
    assert(*(char **)(next_alloc + 8) != NULL); // Previous chunk should be null; This is the head
    assert(*(char **)(next_alloc + 16) == NULL); // Next chunk should be null; We haven't free'd other similar sized chunks

    KV_alloc_pool_free(pool);
}

int main(int argc, char *argv[])
{
    test_KV_alloc_pool_init();
    test_KV_malloc();
    test_KV_free();
    test_KV_freelist_class_linked_list();
    return 0;
}
