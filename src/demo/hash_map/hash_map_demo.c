#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "redo.h"
#include "hash_map.h"

#define DATA_NUM    (15)

typedef struct
{
    int idx;
} hash_map_data_t;

static int64_t hash_cb(const hash_map_data_t *data)
{
    return (int64_t)data->idx;
}

static int cmp_cb(const hash_map_data_t *d1, const hash_map_data_t *d2)
{
    return (d1->idx - d2->idx);
}

int main(void)
{
    int idx;
    hash_map_t *map;
    hash_map_data_t *data, key;

    map = hash_map_creat(3, (hash_cb_t)hash_cb, (cmp_cb_t)cmp_cb, NULL);
    if (NULL == map) {
        fprintf(stderr, "Create hash map failed!");
        return -1;
    }

    for (idx=0; idx<DATA_NUM; ++idx) {
        data = (hash_map_data_t *)calloc(1, sizeof(hash_map_data_t));
        if (NULL == data) {
            fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
            return -1;
        }

        data->idx = idx;

        if (hash_map_insert(map, data, NONLOCK)) {
            fprintf(stderr, "Data may be conflict!");
            free(data);
        }
        fprintf(stderr, "Hash map data num:%lu\n", hash_map_total(map));
    }

    Sleep(5);

    for (idx=0; idx<DATA_NUM; ++idx) {
        key.idx = idx;

        data = hash_map_query(map, &key, NONLOCK);
        if (NULL == data) {
            fprintf(stderr, "Didn't find idx:%d!", idx);
            continue;
        }
        fprintf(stderr, "Data idx:%u\n", data->idx);
    }

    Sleep(5);

    for (idx=0; idx<DATA_NUM; ++idx) {
        key.idx = idx;

        data = hash_map_delete(map, &key, NONLOCK);
        if (NULL == data) {
            fprintf(stderr, "Didn't find idx:%d!", idx);
            continue;
        }
        fprintf(stderr, "Delete idx:%u\n", data->idx);
        free(data);
    }

    Sleep(5);

    for (idx=0; idx<DATA_NUM; ++idx) {
        key.idx = idx;

        data = hash_map_query(map, &key, NONLOCK);
        if (NULL == data) {
            fprintf(stderr, "Didn't find idx:%d!\n", idx);
            continue;
        }
        fprintf(stderr, "Data idx:%u\n", data->idx);
    }
    return 0;
}
