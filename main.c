#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "MurmurHash3.h"
#include "sikv.h"

#if USE_CUSTOM_ALLOC
// to be enabled once windows setup is complete
#if defined(__linux__)
#include <alloc.h>
#endif
#endif

static struct hash_map *HMAP = NULL;

void set_hmap(struct hash_map *hmap)
{
    if (HMAP == NULL)
    {
        HMAP = hmap;
    }
}

struct hash_map *KV_init(unsigned long capacity, hash_function hash_fn, KV_TYPE val_type)
{
    if (capacity && CHECK_POWER_OF_2(capacity) != 0)
    {
        fprintf(stderr, "ERROR: Hmap size must be a power of two\n");
        exit(EXIT_FAILURE);
    }

    struct hash_map *hmap = (struct hash_map *)malloc(sizeof(struct hash_map));
    if (hmap == NULL)
    {
        perror("KV_hash_map_init: Unable to initialize hash table");
        exit(EXIT_FAILURE);
    }

    memset(hmap, 0, sizeof(struct hash_map));

    hmap->hash_fn = hash_fn;

#if USE_CUSTOM_ALLOC
    struct KV_alloc_pool *pool = KV_alloc_pool_init(MIN_ALLOCATION_POOL_SIZE);
    if (pool == NULL)
    {
        perror("KV_hash_map_init: Unable to initialize pool");
        free(hmap);
        exit(EXIT_FAILURE);
    }
    memset(pool->data, EMPTY, MIN_ALLOCATION_POOL_SIZE + 1);
    hmap->pool = (char *)pool;
    hmap->arr = (char *)KV_malloc(pool, capacity * sizeof(struct KV));
#else
    hmap->arr = (char *)malloc(capacity * sizeof(struct KV));
#endif

    if (hmap->arr == NULL)
    {
        perror("KV_hash_map_init: Unable to initialize array");
        free(hmap);
        exit(EXIT_FAILURE);
    }

#if !USE_CUSTOM_ALLOC
    memset(hmap->arr, EMPTY, capacity * sizeof(struct KV));
#endif
    hmap->len = 0;
    hmap->size = capacity * sizeof(struct KV);
#if SIKV_VERBOSE
    printf("Initializing array of size=%i\n", hmap->size);
#endif
    hmap->seed = 1;
    hmap->capacity = capacity;
    hmap->val_type = val_type;
    HMAP = hmap;
    return hmap;
}

struct hash_map *KV_hmap()
{
    if (!HMAP)
    {
        KV_init(MIN_ENTRY_NUM, KV_hash_function, KV_STRING);
    }
    return HMAP;
}

size_t get_type_size(KV_TYPE val_type, char *val)
{
    switch (val_type)
    {
    case KV_INT16:
        return sizeof(int16_t);
    case KV_INT32:
        return sizeof(int32_t);
    case KV_INT64:
        return sizeof(int64_t);
    case KV_FLOAT:
        return sizeof(float);
    case KV_DOUBLE:
        return sizeof(KV_DOUBLE);
    case KV_STRING:
        return strlen(val);
    default:
        break;
    }
    return -1;
}

KV_CMD parse_cmd(char *cmd, int len)
{
    if (memcmp("SET", cmd, len) == 0)
    {
        return CMD_SET;
    }
    else if (memcmp("GET", cmd, len) == 0)
    {
        return CMD_GET;
    }
    else if (memcmp("PUT", cmd, len) == 0)
    {
        return CMD_PUT;
    }
    else if (memcmp("DEL", cmd, len) == 0)
    {
        return CMD_DEL;
    }
    else
    {
        return CMD_NOOP;
    }
}

void *process_cmd(struct hash_map *hmap, int argc, char *argv[])
{
    if (argc < 1)
    {
        fprintf(stderr, "process_cmd: Command is required\n");
        exit(EXIT_FAILURE);
    }

    char *cmd = argv[0];
    int len = strlen(argv[0]);
    int ret;

    switch (parse_cmd(cmd, len))
    {
    case CMD_SET:
    case CMD_PUT:
        if (argc < 3)
        {
            fprintf(stderr, "SET Error: Value was not provided\n");
            return NULL;
        }

        ret = KV_set(hmap, argv[1], strlen(argv[1]), argv[2], get_type_size(hmap->val_type, argv[2]) + 1);
        if (ret == 0)
        {
            return SUCCESS;
        }
        break;
    case CMD_GET:
        if (argc < 2)
        {
            fprintf(stderr, "GET Error: Key was not provided\n");
            break;
        }
        return KV_get(hmap, argv[1], strlen(argv[1]));
    case CMD_DEL:
        if (argc < 2)
        {
            fprintf(stderr, "DELETE Error: Key was not provided\n");
            break;
        }
        ret = KV_delete(hmap, argv[1], strlen(argv[1]));
        if (ret == 0)
        {
            return SUCCESS;
        }
        break;
    default:
        fprintf(stderr, "Invalid command\n");
        break;
    }
    return NULL;
}

static uint32_t first_slot(uint32_t hash, int capacity)
{
    return hash & (capacity - 1);
}

static uint32_t next_slot(uint32_t hash, int capacity)
{
    return (hash + 1) & (capacity - 1);
}

static int resize_find_empty_slot(struct hash_map *hmap, char *buf, int buf_len, char *key, int key_len)
{
    uint32_t hash = hmap->hash_fn(key, key_len, hmap->seed);
    hash = first_slot(hash, hmap->capacity);
    uint32_t start = hash;
    void *entry = &buf[hash * sizeof(struct KV)];

    if (*(int8_t *)entry == EMPTY)
    {
        return hash;
    }

    size_t i = 0;
    while (*(int8_t *)entry != EMPTY && i < hmap->capacity)
    {
        hash = next_slot(hash, hmap->capacity);
        if (hash == start)
        {
            break;
        }
        entry = &buf[hash * sizeof(struct KV)];
        if (*(int8_t *)entry == EMPTY)
        {
            return hash;
        }
        i = hash;
    }

    return -1;
}

static __attribute__((unused)) void rehash_buf(struct hash_map *hmap, char *buf, int buf_len)
{
    for (size_t i = 0; i < buf_len / RESIZE_POLICY; i += sizeof(struct KV))
    {
        struct KV *entry = (struct KV *)&hmap->arr[i];
        if (*(int8_t *)entry == EMPTY || entry->data == TOMBSTONE)
        {
            continue;
        }
        int slot = resize_find_empty_slot(hmap, buf, buf_len, entry->data, entry->key_len);

        memcpy(&buf[slot * sizeof(struct KV)], entry, sizeof(struct KV));
    }
}

static void hash_map_resize(struct hash_map *hmap, int policy)
{
    size_t cap = hmap->capacity * policy * sizeof(struct KV);
    if (cap > MAXIMUM_SIZE)
    {
        perror("hash_map_resize: Maximum memory exceeded");
        exit(EXIT_FAILURE);
    }

#if !USE_CUSTOM_ALLOC
    char *buf = (char *)malloc(cap);
#else
    char *buf = (char *)KV_malloc((struct KV_alloc_pool *)hmap->pool, cap);
#endif

    if (buf == NULL)
    {
        perror("hash_map_resize: Unable to resize hash table");
        exit(EXIT_FAILURE);
    }
    memset(buf, EMPTY, cap);

    hmap->size = hmap->size - (hmap->capacity * sizeof(struct KV));
    memcpy(buf, hmap->arr, hmap->capacity * sizeof(struct KV));
    hmap->capacity = hmap->capacity * policy;
    hmap->size += cap;
    // rehash_buf(hmap, buf, cap);
#if !USE_CUSTOM_ALLOC
    free(hmap->arr);
#else
    KV_free((struct KV_alloc_pool *)hmap->pool, hmap->arr);
#endif
    hmap->arr = buf;
}

bool max_size_reached(int sz, int max_sz)
{
    return sz >= max_sz;
}

uint32_t KV_hash_function(const void *key, int len, int seed)
{
    uint32_t hash = 0;
    MurmurHash3_x86_32(key, len, seed, &hash);
    return hash;
}

static int find_empty_slot(struct hash_map *hmap, char *key, int key_len)
{
    uint32_t hash = hmap->hash_fn(key, key_len, hmap->seed);
    hash = first_slot(hash, hmap->capacity);
    uint32_t start = hash;
    struct KV *entry = (struct KV *)&hmap->arr[hash * sizeof(struct KV)];

    if (*(int8_t *)entry == EMPTY || entry->data == TOMBSTONE || (entry->key_len == key_len && memcmp(entry->data, key, key_len) == 0))
    {
        return hash;
    }

    size_t i = 0;
    while (*(int8_t *)entry != EMPTY && i < hmap->capacity)
    {
        hash = next_slot(hash, hmap->capacity);
        if (hash == start)
        {
            break;
        }
        entry = (struct KV *)&hmap->arr[hash * sizeof(struct KV)];
        if (*(int8_t *)entry == EMPTY || entry->data == TOMBSTONE)
        {
            return hash;
        }
        i = hash;
    }

    return -1;
}

static int entry_init(struct hash_map *hmap, struct KV *entry)
{
    size_t size = entry->key_len + entry->val_len;
    char *data = NULL;

#if !USE_CUSTOM_ALLOC
    // entry->key = (char *)malloc(entry->key_len);
    data = (char *)malloc(size);
#else
    // entry->key = (char *)KV_malloc((struct KV_alloc_pool *)hmap->pool, entry->key_len);
    data = (char *)KV_malloc((struct KV_alloc_pool *)hmap->pool, size);
#endif

    if (data == NULL)
    {
        fprintf(stderr, "entry_init: Unable to intialize data");
        return -1;
    }

    entry->data = data;

    return 0;

    //     if (entry->key == NULL)
    //     {
    //         fprintf(stderr, "entry_init: Unable to intialize entry key");
    //         return -1;
    //     }

    // #if !USE_CUSTOM_ALLOC
    //     entry->val = (char *)malloc(entry->val_len);
    // #else
    //     entry->val = (char *)KV_malloc((struct KV_alloc_pool *)hmap->pool, entry->val_len);
    // #endif

    //     if (entry->val == NULL)
    //     {
    //         fprintf(stderr, "entry_init: Unable to intialize entry value");
    // #if !USE_CUSTOM_ALLOC
    //         // free(entry->key);
    // #else
    //         KV_free((struct KV_alloc_pool *)hmap->pool, entry->key);
    // #endif
    //         return -1;
    //     }
    //     return 0;

    // char *chunk = (char *)malloc(e->key_len + e->val_len);
}

int KV_set(struct hash_map *hmap, char *key, int key_len, char *val, int val_len)
{
    size_t size;
    int ret;
    int temp;

    int slot = find_empty_slot(hmap, key, key_len);

    // KV already allocated during initialization. We just need to get our KV chunk
    struct KV *entry = (struct KV *)&hmap->arr[slot * sizeof(struct KV)];
    size = key_len + val_len;

    if (*(int8_t *)entry == EMPTY)
    {
#if SIKV_VERBOSE
        printf("Writing object of size=%zu\n", size);
#endif
        entry->key_len = key_len;
        entry->val_len = val_len;
        ret = entry_init(hmap, entry);
        if (ret < 0)
        {
            return ret;
        }
        memcpy(entry->data, key, key_len);
        memcpy((char *)&entry->data[key_len], val, val_len);
        entry->data[size - 1] = '\0';
        hmap->size += size;
        hmap->len += 1;

        // TODO: We can replace division later
        float lf = (float)hmap->len / hmap->capacity;
        if (lf >= LOAD_FACTOR)
        {
            temp = hmap->capacity;
            hash_map_resize(hmap, RESIZE_POLICY);
#if SIKV_VERBOSE
            printf("Resizing HashMap from array size=%zu to array size=%zu; current memory usage for data=%i bytes\n", temp * sizeof(struct KV), hmap->capacity * sizeof(struct KV), hmap->size);
#endif
        }
    }
    else
    {
#if !USE_CUSTOM_ALLOC
        entry->data = (char *)malloc(size);
#else
        entry->data = (char *)KV_malloc((struct KV_alloc_pool *)hmap->pool, size);
#endif
        if (entry->data == NULL)
        {
            fprintf(stderr, "KV_set: Unable to intialize entry value");
            return -1;
        }

        memcpy(entry->data, key, key_len);
        memcpy((char *)&entry->data[key_len], val, val_len);
        // hmap->size -= entry->val_len;
        entry->val_len = val_len;
        hmap->size += size;
    }
    return 0;
}

static int find(struct hash_map *hmap, char *key, int key_len)
{
    uint32_t hash = hmap->hash_fn(key, key_len, hmap->seed);
    hash = first_slot(hash, hmap->capacity);
    uint32_t start = hash;
    struct KV *entry = (struct KV *)&hmap->arr[hash * sizeof(struct KV)];
    // char key[key_len];
    // memcpy(key, entry->data, key_len);

    if (entry->data != TOMBSTONE && (key_len == entry->key_len && memcmp(entry->data, key, entry->key_len) == 0))
    {
        return hash;
    }

    size_t i = 0;
    while ((*(int8_t *)entry != EMPTY || entry->data != TOMBSTONE) && i < hmap->capacity)
    {
        hash = next_slot(hash, hmap->capacity);

        // We need to stop the search where we started. If we get to the start point; the key does not exist
        if (hash == start)
        {
            break;
        }

        entry = (struct KV *)&hmap->arr[hash * sizeof(struct KV)];
        if (entry->data != TOMBSTONE && key_len == entry->key_len && memcmp(entry->data, key, entry->key_len) == 0)
        {
            return hash;
        }
        i = hash;
    }

    return -1;
}

void *KV_get(struct hash_map *hmap, char *key, int key_len)
{
    int64_t slot = find(hmap, key, key_len);
    struct KV *entry = NULL;
    if (slot == -1)
    {
        return NULL;
    }
    entry = (struct KV *)&hmap->arr[slot * sizeof(struct KV)];
    return (void *)&entry->data[key_len];
}

int KV_delete(struct hash_map *hmap, char *key, int key_len)
{
    struct KV *entry = NULL;
    int64_t slot = find(hmap, key, key_len);
    if (slot <= -1)
    {
        return -1;
    }

    entry = (struct KV *)&hmap->arr[slot * sizeof(struct KV)];
#if !USE_CUSTOM_ALLOC
    free(entry->data);
#else
    KV_free((struct KV_alloc_pool *)hmap->pool, entry->data);
#endif
    hmap->size -= (entry->key_len + entry->val_len);
    entry->data = TOMBSTONE;
    return 0;
}

void KV_destroy()
{
    struct hash_map *hmap = HMAP;
    if (hmap)
    {
#if !USE_CUSTOM_ALLOC
        int len = hmap->capacity * sizeof(struct KV);
        for (size_t i = 0; i < len; i += sizeof(struct KV))
        {
            if (hmap->arr[i] != EMPTY)
            {
                struct KV *entry = (struct KV *)&hmap->arr[i];
                free(entry->data);
                // free(entry->val);
            }
        }
        free(hmap->arr);
#else
        KV_alloc_pool_free((struct KV_alloc_pool *)hmap->pool);
#endif
        free(hmap);
    }
}

int main(int argc, char *argv[])
{

    serve(argc, argv); // We should never return

    return 0;
}