#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <assert.h>
#include <errno.h>

#include "alloc.h"

#define ALIGN_MASK(SZ) ((SZ) - (1UL))
#define ALIGN_TO_SIZE(X, MASK) ((MASK + X) & ~MASK)
#define IS_ALIGNED(X, MASK) ((X % MASK) == 0)

static struct KV_alloc_pool *alloc_pool[MAX_ALLOCATION_POOLS_NUM] = {NULL};
static int num_pools;
// static struct KV_alloc_freelist *alloc_freelist = NULL;
static char *freelist[MAX_FREELIST_NUM_CLASSES] = {NULL};

const char *get_freelist_item(int idx)
{
    return (const char *)freelist[idx];
}

static void *KV_mmap_allocate(size_t size)
{
    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (data == MAP_FAILED)
    {
        fprintf(stderr, "mmap_allocate: unable to allocate size=%lu: %s\n", size, strerror(errno));
        return NULL;
    }
    return data;
}

static int KV_mmap_deallocate(void *ptr, size_t size)
{
    if (ptr == NULL)
    {
        return 0;
    }

    return munmap(ptr, size);
}

struct KV_alloc_pool *KV_alloc_pool_init(size_t size)
{
    size = ALIGN_TO_SIZE(size, ALIGN_MASK(MIN_ALLOCATION_POOL_SIZE));
    struct KV_alloc_pool *pool = NULL;

    if ((num_pools + 1) > MAX_ALLOCATION_POOLS_NUM)
    {
        fprintf(stderr, "KV_alloc_pool_init: exceeded memory size=%lu\n", size);
        return NULL;
    }

    pool = malloc(sizeof(struct KV_alloc_pool));
    if (pool == NULL)
    {
        fprintf(stderr, "KV_alloc_pool_init: malloc: unable to allocate size=%lu: %s\n", size, strerror(errno));
        return NULL;
    }

    pool->offset = pool->size = 0;
    pool->data = NULL;
    // pool->prev = pool->next = NULL;
    pool->data = KV_mmap_allocate(size);
    if (pool->data == NULL)
    {
        fprintf(stderr, "KV_alloc_pool_init: mmap_allocate: unable to allocate size=%lu: %s\n", size, strerror(errno));
        return NULL;
    }
    pool->size += size;
    num_pools += 1;
    alloc_pool[num_pools - 1] = pool;
    return (struct KV_alloc_pool *)pool;
}

void KV_alloc_pool_free(struct KV_alloc_pool *pool)
{
    if (pool != NULL)
    {
        if (KV_mmap_deallocate(pool->data, pool->size) == -1)
        {
            perror("mmap_deallocate");
        }
        else
        {
            free(pool);
        }
    }
}

static int KV_get_freelist_alloc_class(size_t size)
{
    size = ALIGN_TO_SIZE(size, ALIGN_MASK(ALLOCATION_CLASSES_INCR_SIZE));
    if (size <= MIN_ALLOCATION_CLASS_SIZE + (ALLOCATION_CLASSES_INCR_SIZE * MAX_FREELIST_NUM_CLASSES))
    {
        return (size - MIN_ALLOCATION_CLASS_SIZE) / ALLOCATION_CLASSES_INCR_SIZE;
    }
    return MAX_FREELIST_NUM_CLASSES - 1;
}

static void *KV_remove_from_freelist_head(size_t size)
{
    assert(IS_ALIGNED(size, ALLOCATION_CLASSES_INCR_SIZE));

    int alloc_class = KV_get_freelist_alloc_class(size);
    assert(alloc_class < MAX_FREELIST_NUM_CLASSES);
    char *alloc_class_head = freelist[alloc_class];

    char *next_alloc = NULL;
    // char *prev_alloc = NULL;

    // TODO: Implement best fit strategy
    if (!alloc_class_head)
    {
        return NULL;
    }

    assert(*(char **)(alloc_class_head + 8) == NULL);

    next_alloc = *(char **)(alloc_class_head + 16);
    if (next_alloc)
    {
        *(char **)(next_alloc + 8) = NULL; // Previous chunk
        freelist[alloc_class] = next_alloc;
    }
    else
    {
        freelist[alloc_class] = next_alloc;
    }

    return alloc_class_head;
}

static void KV_add_to_freelist(char *alloc_start, size_t size)
{
    char *alloc_class_head = NULL; // First chunk from freelist class
    int alloc_class = KV_get_freelist_alloc_class(size);
    assert(alloc_class < MAX_FREELIST_NUM_CLASSES);
    alloc_class_head = freelist[alloc_class];

    if (!alloc_class_head) // Empty
    {
        freelist[alloc_class] = alloc_start;
        *(char **)(alloc_start + 8) = NULL;
        *(char **)(alloc_start + 16) = NULL;
    }
    else
    {
        assert(*(char **)(alloc_class_head + 8) == NULL);

        freelist[alloc_class] = alloc_start;
        *(char **)(alloc_start + 8) = NULL;              // New head previous chunk
        *(char **)(alloc_start + 16) = alloc_class_head; // New head next chunk
        *(char **)(alloc_class_head + 8) = alloc_start;  // Old head previous chunk

        assert(*(char **)(alloc_start + 16) != NULL);
        assert(*(char **)(alloc_class_head + 8) != NULL);
    }
}

void *KV_malloc(struct KV_alloc_pool *pool, size_t size)
{
    size = ALIGN_TO_SIZE(size + ALLOCATION_SIZE_OVERHEAD, ALIGN_MASK(ALLOCATION_CLASSES_INCR_SIZE));
    char *alloc = KV_remove_from_freelist_head(size);

    if (alloc)
    {
        return (void *)alloc + ALLOCATION_SIZE_OVERHEAD;
    }

    if (!pool)
    {
        // fprintf(stderr, "KV_malloc: unable to allocate size=%lu\n", size);
        fprintf(stderr, "KV_malloc: invalid memory pool");
        return NULL;
    }

    if (!pool->data)
    {
        fprintf(stderr, "KV_malloc: invalid memory address");
        return NULL;
    }

    if ((size + pool->offset) >= pool->size)
    {
#ifdef SIKV_VERBOSE
        printf("memory limit of current pool exceeded for size=%lu\n", size);
#endif
        return NULL;
    }

    alloc = pool->data + pool->offset;
    pool->offset += size;
    *(uint64_t *)alloc = size;
    return (void *)alloc + ALLOCATION_SIZE_OVERHEAD;
}

void KV_free(struct KV_alloc_pool *pool, void *ptr)
{
    // int64_t size = *(int64_t *)(ptr - ALLOCATION_SIZE_OVERHEAD);
    // int alloc_class = KV_get_freelist_alloc_class(size);
    char *alloc_start = ptr - ALLOCATION_SIZE_OVERHEAD;
    uint64_t size = *(uint64_t *)alloc_start;
    KV_add_to_freelist(alloc_start, size);
}
