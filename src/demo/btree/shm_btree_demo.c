#include "comm.h"
#include "shm_btree.h"

#define BTREE_M     (8)
#define BTREE_NUM   (1024)
#define INPUT_LEN   (32)
#define __AUTO_INPUT__

int main(void)
{
    int ret, key, idx;
    log_cycle_t *log;
    char input[INPUT_LEN];
    int *data;
    shm_btree_cntx_t *ctx;

    log = log_init(LOG_LEVEL_TRACE, "btree.log");
    if (NULL == log) {
        return -1;
    }

    /* > 创建B树 */
    ctx = shm_btree_creat("test.bt", BTREE_M, 32 * MB, log);
    if (NULL == ctx) {
        fprintf(stderr, "[%s][%d] Create btree failed!\n", __FILE__, __LINE__);
        return -1;
    }

    fprintf(stderr, "[%s][%d] Create btree success!\n", __FILE__, __LINE__);

    /* > 插入关键字 */
    for(idx=0; idx<BTREE_NUM; idx++) {
        data = (int *)shm_btree_alloc(ctx, sizeof(int));
        *data = idx;

        shm_btree_insert(ctx, idx, (void *)data);
    }

    shm_btree_print(ctx);

    /* > 操作B树 */
    while(1) {
        memset(input, 0, sizeof(input));
    
    #if defined(__AUTO_INPUT__)
        fprintf(stdout, "Input:");
        scanf(" %s", input);
        key = atoi(input);
    #else
        key = random()%BTREE_NUM;
    #endif

        if ((0 == strcasecmp(input, "q"))
            || (0 == strcasecmp(input, "quit"))
            || (0 == strcasecmp(input, "exit")))
        {
            break;
        }
        else if (0 == strcasecmp(input, "d")
            || 0 == strcasecmp(input, "delete"))
        {
            scanf(" %s", input);
            key = atoi(input);

            shm_btree_remove(ctx, key);
            shm_btree_print(ctx);
            continue;
        }

        data = shm_btree_alloc(ctx, sizeof(int));
        *data = key;

        ret = shm_btree_insert(ctx, key, (void *)data);
        if (0 != ret) {
            fprintf(stderr, "[%s][%d] Insert failed!\n", __FILE__, __LINE__);
            break;
        }

        fprintf(stderr, "[%d] Insert btree success!\n", key);

        shm_btree_print(ctx);
    }

    shm_btree_dump(ctx);
    return 0;
}
