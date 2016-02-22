#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "shm_list.h"
#include "shm_hash.h"

#define SHM_HASH_LEN    (1024)
#define SHM_HASH_MAX    (2048)
#define SHM_HASH_PATH   "shm_hash.key"

typedef struct
{
    int idx;
} list_data_t;

int shm_list_test(void);

int main(void)
{
    return shm_list_test();
}

static int shm_list_cmp(int *idx, list_data_t *data)
{
    return *idx - data->idx;
}

int shm_list_test(void)
{
    int idx;
    shm_hash_t *sh;
    list_data_t *data;

    sh = shm_hash_creat(SHM_HASH_PATH, SHM_HASH_LEN, SHM_HASH_MAX, sizeof(list_data_t));
    if (NULL == sh) {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    for (idx=0; idx<SHM_HASH_MAX; ++idx) {
        data = (list_data_t *)shm_hash_alloc(sh);
        if (NULL == data) {
            fprintf(stderr, "Alloc from shm-hash failed! idx:%d\n", idx);
            break;
        }

        data->idx = idx;

        if (shm_hash_push(sh, &idx, sizeof(idx), (void *)data)) {
            fprintf(stderr, "Dealloc from shm-hash failed! idx:%d\n", idx);
            shm_hash_dealloc(sh, data);
        }
    }

    for (idx=0; idx<SHM_HASH_MAX; ++idx) {
        data = (list_data_t *)shm_hash_pop(sh, &idx, sizeof(idx), (cmp_cb_t)shm_list_cmp);
        if (NULL == data) {
            abort();
        }
    }

    return 0;
}
