#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "MurmurHash3.h"
#include "sikv.h"

static struct hash_map *HMAP = NULL;

void set_hmap(struct hash_map *hmap)
{
    if (HMAP == NULL)
    {
        HMAP = hmap;
    }
}

struct hash_map *get_hmap()
{
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
        perror("Command is required\n");
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

uint32_t first_slot(uint32_t hash, int capacity)
{
    return hash & (capacity - 1);
}

uint32_t next_slot(uint32_t hash, int capacity)
{
    return (hash + 1) & (capacity - 1);
}

int resize_find_empty_slot(struct hash_map *hmap, char *buf, int buf_len, char *key, int key_len)
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

void rehash_buf(struct hash_map *hmap, char *buf, int buf_len)
{
    for (size_t i = 0; i < buf_len / RESIZE_POLICY; i += sizeof(struct KV))
    {
        struct KV *entry = (struct KV *)&hmap->arr[i];
        if (*(int8_t *)entry == EMPTY || entry->key == TOMBSTONE)
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
    if (cap > MAXIMUM_SIZE)
    {
        perror("hash_map_resize: Maximum memory exceeded");
        exit(EXIT_FAILURE);
    }
    char *buf = (char *)malloc(cap);
    if (buf == NULL)
    {
        perror("hash_map_resize: Unable to resize hash table");
        exit(EXIT_FAILURE);
    }
    memset(buf, EMPTY, cap);

    hmap->size = hmap->size - (hmap->capacity * sizeof(struct KV));
    hmap->capacity = hmap->capacity * policy;
    hmap->size += cap;
    rehash_buf(hmap, buf, cap);
    free(hmap->arr);
    hmap->arr = buf;
}

struct hash_map *KV_init(unsigned long capacity, hash_function hash_fn, KV_TYPE val_type)
{
    struct hash_map *hmap = (struct hash_map *)malloc(sizeof(struct hash_map));
    if (hmap == NULL)
    {
        perror("KV_hash_map_init: Unable to initialize hash table");
        exit(EXIT_FAILURE);
    }

    memset(hmap, 0, sizeof(struct hash_map));

    hmap->hash_fn = hash_fn;
    hmap->arr = (char *)malloc(capacity * sizeof(struct KV));
    if (hmap->arr == NULL)
    {
        free(hmap);
        perror("KV_hash_map_init: Unable to initialize array");
        exit(EXIT_FAILURE);
    }
    memset(hmap->arr, EMPTY, capacity * sizeof(struct KV));
    hmap->size = capacity * sizeof(struct KV);
    hmap->seed = 1;
    hmap->capacity = capacity;
    hmap->val_type = val_type;
    set_hmap(hmap);
    return get_hmap();
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
    hash = first_slot(hash, hmap->capacity);
    uint32_t start = hash;
    struct KV *entry = (struct KV *)&hmap->arr[hash * sizeof(struct KV)];

    if (*(int8_t *)entry == EMPTY || entry->key == TOMBSTONE || (entry->key_len == key_len && memcmp(entry->key, key, key_len) == 0))
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
        if (*(int8_t *)entry == EMPTY || entry->key == TOMBSTONE)
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
        fprintf(stderr, "entry_init: Unable to intialize entry key");
        return -1;
    }

    entry->val = (char *)malloc(entry->val_len);
    if (entry->val == NULL)
    {
        fprintf(stderr, "entry_init: Unable to intialize entry value");
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

    int slot = find_empty_slot(hmap, key, key_len);

    // KV already allocated during initialization. We just need to get our KV chunk
    struct KV *entry = (struct KV *)&hmap->arr[slot * sizeof(struct KV)];

    if (*(int8_t *)entry == EMPTY)
    {
        size = key_len + val_len + sizeof(struct KV);
        entry->key_len = key_len;
        entry->val_len = val_len;
        ret = entry_init(entry);
        if (ret < 0)
        {
            return ret;
        }
        memcpy(entry->key, key, key_len);
        memcpy(entry->val, val, val_len);
        entry->val[val_len - 1] = '\0';
        hmap->size += size;
        hmap->len += 1;

        // TODO: We can replace division later
        float lf = (float)hmap->len / hmap->capacity;
        if (lf >= LOAD_FACTOR)
        {
            printf("Resizing HashMap\n");
            hash_map_resize(hmap, RESIZE_POLICY);
        }
    }
    else
    {
        entry->val = (char *)malloc(val_len);
        if (entry->val == NULL)
        {
            fprintf(stderr, "KV_set: Unable to intialize entry value");
            return -1;
        }
        memcpy(entry->key, key, key_len);
        memcpy(entry->val, val, val_len);
        hmap->size -= entry->val_len;
        entry->val_len = val_len;
        hmap->size += val_len;
    }
    return 0;
}

int find(struct hash_map *hmap, char *key, int key_len)
{
    uint32_t hash = hmap->hash_fn(key, key_len, hmap->seed);
    hash = first_slot(hash, hmap->capacity);
    uint32_t start = hash;
    struct KV *entry = (struct KV *)&hmap->arr[hash * sizeof(struct KV)];

    if (entry->key != TOMBSTONE && (key_len == entry->key_len && memcmp(entry->key, key, entry->key_len) == 0))
    {
        return hash;
    }

    size_t i = 0;
    while ((*(int8_t *)entry != EMPTY || entry->key != TOMBSTONE) && i < hmap->capacity)
    {
        hash = next_slot(hash, hmap->capacity);

        // We need to stop the search where we started. If we get to the start point; the key does not exist
        if (hash == start)
        {
            break;
        }

        entry = (struct KV *)&hmap->arr[hash * sizeof(struct KV)];
        if (entry->key != TOMBSTONE && key_len == entry->key_len && memcmp(entry->key, key, entry->key_len) == 0)
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
    return entry->val;
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
    free(entry->key);
    entry->key = TOMBSTONE;
    return 0;
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

    serve(); // We should never return

    return 0;
}