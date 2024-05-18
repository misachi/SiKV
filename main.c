#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "MurmurHash3.h"

#define LOAD_FACTOR (int8_t)50;  // 0-100
#define MAXIMUM_SIZE 1073741824UL; // maximum limit of hashmap; default 1GB
// #define EMPTY (uint64_t)18446744073709551616
#define EVICT 1
#define RESIZE_POLICY 2
#define EMPTY (int8_t)(-1)
#define TOMBSTONE NULL

struct hash_map;
typedef uint32_t (*hash_function)(const void *key, int len, int seed);

struct hash_map *KV_hash_map_init(unsigned long size, hash_function hash_fn);
void KV_set(struct hash_map *hmap, char *key, int key_len, char *val, int val_len);
void *KV_get(struct hash_map *hmap, char *key, int key_len);
void KV_delete(struct hash_map *hmap, char *key, int key_len);
void *find(struct hash_map *hmap, char *key, int key_len);
void KV_hash_map_destroy(struct hash_map *hmap);
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

void rehash_buf(struct hash_map *hmap, char *buf, int buf_len)
{
    for (size_t i = 0; i < buf_len; i += sizeof(struct KV))
    {
        struct KV *entry = (struct KV *)&hmap->arr[i];
        uint32_t hash = hmap->hash_fn(entry->key, entry->key_len, hmap->seed);
        memcpy(&buf[hash % hmap->capacity], entry, sizeof(struct KV));
    }
}

void hash_map_resize(struct hash_map *hmap, int policy)
{
    size_t cap = hmap->capacity * policy * sizeof(struct KV);
    char *arr = (char *)malloc(cap);
    if (arr == NULL)
    {
        printf("hash_map_resize: Unable to resize hash table");
        exit(1);
    }
    memset(arr, EMPTY, cap);

    hmap->capacity = hmap->capacity * policy;
    // memcpy(arr, hmap->arr, hmap->len * sizeof(struct KV));
    rehash_buf(hmap, arr, cap);
    free(hmap->arr);
    hmap->arr = arr;
}

struct hash_map *KV_hash_map_init(unsigned long capacity, hash_function hash_fn)
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
    hash = hash % hmap->capacity;
    void *entry = &hmap->arr[hash * sizeof(struct KV)];

    if (*(int8_t *)entry == EMPTY)
    {
        return hash;
    }

    size_t i = 0;
    while (*(int8_t *)entry != EMPTY && i < hmap->capacity)
    {
        hash = (hash + 1) % hmap->capacity;
        entry = &hmap->arr[hash * sizeof(struct KV)];
        if (*(int8_t *)entry == EMPTY)
        {
            return hash;
        }
        i = hash;
    }

    return -1;
}

void entry_init(struct KV* entry) {
    // TODO: Allocate both key and value on the same chunk
    entry->key = (char *)malloc(entry->key_len);
    if (entry->key == NULL)
    {
        printf("entry_init: Unable to intialize entry key");
        exit(1);
    }

    entry->val = (char *)malloc(entry->val_len);
    if (entry->val == NULL)
    {
        printf("entry_init: Unable to intialize entry value");
        exit(1);
    }

    // char *chunk = (char *)malloc(e->key_len + e->val_len);
}

void KV_set(struct hash_map *hmap, char *key, int key_len, char *val, int val_len)
{
    size_t size;
    struct KV *entry = NULL;

    size = key_len + val_len + sizeof(struct KV);
    int slot = find_empty_slot(hmap, key, key_len);
    if (slot < 0)
    {
        hash_map_resize(hmap, RESIZE_POLICY);
    }
    entry = (struct KV *)malloc(sizeof(struct KV));  // Remove allocation
    if (entry == NULL)
    {
        printf("KV_set: Unable to intialize entry");
        exit(1);
    }

    // KV already allocated during initialization. We just need to get our KV chunk
    entry = (struct KV*)&hmap->arr[slot * sizeof(struct KV)];
    // entry->key = key;
    entry->key_len = key_len;
    entry->val_len = val_len;
    entry_init(entry);
    memcpy(entry->key, key, key_len);
    // entry->val = val;
    memcpy(entry->val, val, val_len);
    // memcpy((char *)&hmap->arr[slot * sizeof(struct KV)], (char *)entry, sizeof(struct KV));
    hmap->size = size;
    hmap->len += 1;
}

void *find(struct hash_map *hmap, char *key, int key_len) {
    uint32_t hash = hmap->hash_fn(key, key_len, hmap->seed);
    hash = hash % hmap->capacity;
    struct KV *entry = (struct KV *)&hmap->arr[hash * sizeof(struct KV)];

    if (key_len == entry->key_len && memcmp(entry->key, key, entry->key_len) == 0)
    {
        return entry->val;
    }

    size_t i = 0;
    while ((*(int8_t *)entry == EMPTY || entry == TOMBSTONE) && i < hmap->capacity)
    {
        hash = (hash + 1) % hmap->capacity;
        entry = (struct KV *)&hmap->arr[hash * sizeof(struct KV)];
        if (key_len == entry->key_len && memcmp(entry->key, key, entry->key_len))
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

void KV_hash_map_destroy(struct hash_map *hmap)
{
    free(hmap->arr);
    free(hmap);
}

int main(int argc, char *argv[])
{
    // const char *keys[] = {"foo", "bar", "noop", "foo", "kai", "boop", "baruuituityuy"};

    // printf("%ld\n\n\n", sizeof(keys));

    // for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    // {
    // uint32_t hash = KV_hash_function(keys[i], sizeof(keys[i]), 1);
    // printf("%d\n", hash);
    // printf("%d\n", hash % 10);
    // }
    struct hash_map *hmap = KV_hash_map_init(10, KV_hash_function);
    int v = 15008;
    char *key = "foo";
    char *val = (char*)&v;
    int key_len = sizeof(key);
    int val_len = sizeof(v);
    KV_set(hmap, key, key_len, val, val_len);

    char *ret = (char *)KV_get(hmap, key, key_len);
    if (ret == (void *)-1)
    {
        printf("Error\n");
    }

    printf("Res: %d\n", *(int*)ret);

    int v2 = 3764;
    char *key2 = "f001";
    char *val2 = (char*)&v2;
    int key_len2 = sizeof(key2);
    int val_len2 = sizeof(v2);
    KV_set(hmap, key2, key_len2, val2, val_len2);

    char *ret2 = (char *)KV_get(hmap, key2, key_len2);
    if (ret2 == (void *)-1)
    {
        printf("Error\n");
    }

    printf("Res: %d\n", *(int*)ret2);

    KV_hash_map_destroy(hmap);

    return 0;
}