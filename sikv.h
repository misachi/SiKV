#ifndef _KV_DB_
#define _KV_DB_

#include <stdlib.h>
#include <stdbool.h>
// #include <stdatomic.h>

#define LOAD_FACTOR (float)0.85 // 0-100
#define MAXIMUM_SIZE 1073741824UL // maximum limit of hashmap; default 1GB
// #define EMPTY (uint64_t)18446744073709551616
#define EVICT 1
#define RESIZE_POLICY 2
#define EMPTY (int8_t)-1
#define TOMBSTONE NULL
#define SUCCESS (void *)-1
#define BUFFSZ 1024
#define SIKV_VERBOSE 1
#define MIN_ENTRY_NUM 4UL
#define CHECK_POWER_OF_2(num) ((num) & ((num) - 1L))
#define USE_CUSTOM_ALLOC 1

typedef enum
{
    CMD_SET,
    CMD_GET,
    CMD_PUT,
    CMD_DEL,
    CMD_NOOP
} KV_CMD;

typedef enum
{
    KV_INT16,
    KV_INT32,
    KV_INT64,
    KV_FLOAT,
    KV_DOUBLE,
    KV_STRING
} KV_TYPE;

typedef uint32_t (*hash_function)(const void *key, int len, int seed);

struct KV
{
    int32_t key_len;
    int32_t val_len;
    char *data;
};

struct KV_item_array
{
    int size;
    char *arr; // array elements
    struct KV_item_array *next;
};

struct hash_map
{
    int size; // size of hash map in bytes
    int len;
    int capacity;
    int seed;
    KV_TYPE val_type;
#if USE_CUSTOM_ALLOC
    char *pool;
#endif
#ifdef ALLOW_STATS
    uint64_t misses;
    uint64_t hits;
    uint64_t ref_count;
#endif
    char *arr;
    struct KV_item_array item_arr;
    hash_function hash_fn;
};

struct hash_map *KV_init(unsigned long capacity, hash_function hash_fn, KV_TYPE val_type, bool alloc_concurrent_access);
int KV_set(struct hash_map *hmap, char *key, int key_len, char *val, int val_len);
void *KV_get(struct hash_map *hmap, char *key, int key_len);
int KV_delete(struct hash_map *hmap, char *key, int key_len);
void KV_destroy();
void *process_cmd(struct hash_map *hmap, int argc, char *argv[]);
uint32_t KV_hash_function(const void *key, int len, int seed);
void serve(int argc, char *argv[]);
struct hash_map *KV_hmap(bool alloc_concurrent_access);
void set_hmap(struct hash_map *hmap);

#endif // _KV_DB_