#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "MurmurHash3.h"

#define LOAD_FACTOR (float)(0.5)           // 0-100
#define MAXIMUM_SIZE 1073741824UL // maximum limit of hashmap; default 1GB
// #define EMPTY (uint64_t)18446744073709551616
#define EVICT 1
#define RESIZE_POLICY 2
#define EMPTY (int8_t)(-1)
#define TOMBSTONE NULL

struct hash_map;
typedef uint32_t (*hash_function)(const void *key, int len, int seed);

struct hash_map *KV_init(unsigned long size, hash_function hash_fn);
int KV_set(struct hash_map *hmap, char *key, int key_len, char *val, int val_len);
void *KV_get(struct hash_map *hmap, char *key, int key_len);
void KV_delete(struct hash_map *hmap, char *key, int key_len);
void *find(struct hash_map *hmap, char *key, int key_len);
void KV_destroy(struct hash_map *hmap);
void hash_map_resize(struct hash_map *hmap, int policy);

struct KV
{
    char *key;
    char *val;
    int32_t key_len;
    int32_t val_len;
    // uint32_t hash;
};

struct hash_map
{
    int size; // size of hash map in bytes
    int len;
    int capacity;
    int seed;
#ifdef ALLOW_STATS
    uint64_t misses;
    uint64_t hits;
    uint64_t ref_count;
#endif
    char *arr; // array elements
    hash_function hash_fn;
};

int resize_find_empty_slot(struct hash_map *hmap, char *buf, int buf_len, char *key, int key_len)
{
    uint32_t hash = hmap->hash_fn(key, key_len, hmap->seed);
    hash = hash & (hmap->capacity - 1);
    void *entry = &buf[hash * sizeof(struct KV)];

    if (*(int8_t *)entry == EMPTY)
    {
        return hash;
    }

    size_t i = 0;
    while (*(int8_t *)entry != EMPTY && i < hmap->capacity)
    {
        hash = (hash + 1) & (hmap->capacity - 1);
        entry = &buf[hash * sizeof(struct KV)];
        if (*(int8_t *)entry == EMPTY)
        {
            return hash;
        }
        i = hash;
    }

    return -1;
}

void rehash_buf(struct hash_map *hmap, char *buf, int buf_len)
{
    for (size_t i = 0; i < buf_len / RESIZE_POLICY; i += sizeof(struct KV))
    {
        struct KV *entry = (struct KV *)&hmap->arr[i];
        if (*(int8_t *)entry == EMPTY || entry == TOMBSTONE)
        {
            continue;
        }
        int slot = resize_find_empty_slot(hmap, buf, buf_len, entry->key, entry->key_len);

        memcpy(&buf[slot * sizeof(struct KV)], entry, sizeof(struct KV));
    }
}

void hash_map_resize(struct hash_map *hmap, int policy)
{
    size_t cap = hmap->capacity * policy * sizeof(struct KV);
    if (cap > MAXIMUM_SIZE) {
        printf("hash_map_resize: Maximum memory exceeded");
        exit(1);
    }
    char *buf = (char *)malloc(cap);
    if (buf == NULL)
    {
        printf("hash_map_resize: Unable to resize hash table");
        KV_destroy(hmap);
        exit(1);
    }
    memset(buf, EMPTY, cap);

    hmap->capacity = hmap->capacity * policy;
    rehash_buf(hmap, buf, cap);
    free(hmap->arr);
    hmap->arr = buf;
}

struct hash_map *KV_init(unsigned long capacity, hash_function hash_fn)
{
    struct hash_map *hmap = (struct hash_map *)malloc(sizeof(struct hash_map));
    if (hmap == NULL)
    {
        printf("KV_hash_map_init: Unable to initialize hash table");
        exit(1);
    }

    memset(hmap, 0, sizeof(struct hash_map));

    hmap->hash_fn = hash_fn;
    hmap->arr = (char *)malloc(capacity * sizeof(struct KV));
    if (hmap->arr == NULL)
    {
        printf("KV_hash_map_init: Unable to initialize array");
        free(hmap);
        exit(1);
    }
    memset(hmap->arr, EMPTY, capacity * sizeof(struct KV));
    hmap->size = 0;
    hmap->seed = 1;
    hmap->capacity = capacity;
    return hmap;
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

int find_empty_slot(struct hash_map *hmap, char *key, int key_len)
{
    uint32_t hash = hmap->hash_fn(key, key_len, hmap->seed);
    hash = hash & (hmap->capacity - 1);
    void *entry = &hmap->arr[hash * sizeof(struct KV)];

    if (*(int8_t *)entry == EMPTY)
    {
        return hash;
    }

    size_t i = 0;
    while (*(int8_t *)entry != EMPTY && i < hmap->capacity)
    {
        hash = (hash + 1) & (hmap->capacity - 1);
        entry = &hmap->arr[hash * sizeof(struct KV)];
        if (*(int8_t *)entry == EMPTY)
        {
            return hash;
        }
        i = hash;
    }

    return -1;
}

int entry_init(struct KV *entry)
{
    // TODO: Allocate both key and value on the same chunk
    entry->key = (char *)malloc(entry->key_len);
    if (entry->key == NULL)
    {
        printf("entry_init: Unable to intialize entry key");
        return -1;
    }

    entry->val = (char *)malloc(entry->val_len);
    if (entry->val == NULL)
    {
        printf("entry_init: Unable to intialize entry value");
        free(entry->key);
        return -1;
    }
    return 0;

    // char *chunk = (char *)malloc(e->key_len + e->val_len);
}

int KV_set(struct hash_map *hmap, char *key, int key_len, char *val, int val_len)
{
    size_t size;
    int ret;
    struct KV *entry = NULL;

    size = key_len + val_len + sizeof(struct KV);
    int slot = find_empty_slot(hmap, key, key_len);

    // KV already allocated during initialization. We just need to get our KV chunk
    entry = (struct KV *)&hmap->arr[slot * sizeof(struct KV)];

    entry->key_len = key_len;
    entry->val_len = val_len;
    ret = entry_init(entry);
    if (ret < 0)
    {
        return ret;
    }
    memcpy(entry->key, key, key_len);
    memcpy(entry->val, val, val_len);
    hmap->size = size;
    hmap->len += 1;

    // TODO: We can replace division later
    float lf = (float)hmap->len / hmap->capacity;
    if (lf > LOAD_FACTOR)
    {
        printf("Resizing HashMap\n");
        hash_map_resize(hmap, RESIZE_POLICY);
    }
    return 0;
}

void *find(struct hash_map *hmap, char *key, int key_len)
{
    uint32_t hash = hmap->hash_fn(key, key_len, hmap->seed);
    hash = hash & (hmap->capacity - 1);
    struct KV *entry = (struct KV *)&hmap->arr[hash * sizeof(struct KV)];

    if (key_len == entry->key_len && memcmp(entry->key, key, entry->key_len) == 0)
    {
        return entry->val;
    }

    size_t i = 0;
    while ((*(int8_t *)entry != EMPTY || entry != TOMBSTONE) && i < hmap->capacity)
    {
        hash = (hash + 1) & (hmap->capacity - 1);
        entry = (struct KV *)&hmap->arr[hash * sizeof(struct KV)];
        if (key_len == entry->key_len && memcmp(entry->key, key, entry->key_len) == 0)
        {
            return entry->val;
        }
        i = hash;
    }

    return (void *)-1;
}

void *KV_get(struct hash_map *hmap, char *key, int key_len)
{
    return find(hmap, key, key_len);
}

void KV_destroy(struct hash_map *hmap)
{
    int len = hmap->capacity * sizeof(struct KV);
    for (size_t i = 0; i < len; i += sizeof(struct KV))
    {
        if (hmap->arr[i] != EMPTY)
        {
            struct KV *entry = (struct KV *)&hmap->arr[i];
            free(entry->key);
            free(entry->val);
        }
    }

    free(hmap->arr);
    free(hmap);
}

int set_int_val(struct hash_map *hmap, char *key, int key_len, int val)
{
    char *v = (char *)&val;
    int ret = KV_set(hmap, key, key_len, v, sizeof(val));
    if (ret < 0)
    {
        printf("Unable to insert key-value");
    }
    return ret;
}

int main(int argc, char *argv[])
{
    char *keys[] = {"foo", "bar", "noop", "foo", "kai", "boop", "baruuituityuy"};
    int vals[] = {100, 200, 300, 400, 500, 600, 700};

    // printf("%ld\n\n\n", sizeof(keys));

    // for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    // {
    // uint32_t hash = KV_hash_function(keys[i], sizeof(keys[i]), 1);
    // printf("%d\n", hash);
    // printf("%d\n", hash % 10);
    // }
    struct hash_map *hmap = KV_init(4, KV_hash_function);

    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    {
        set_int_val(hmap, keys[i], sizeof(keys[i]), vals[i]);
    }

    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    {
        char *ret = (char *)KV_get(hmap, keys[i], sizeof(keys[i]));
        if (ret == (void *)-1)
        {
            printf("Not found\n");
        }
        else
        {
            printf("%s: %d\n", keys[i], *(int *)ret);
        }
    }

    KV_destroy(hmap);

    return 0;
}