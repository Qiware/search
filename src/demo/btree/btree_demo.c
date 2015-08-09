#include "btree.h"

#define BTREE_M     (3)
#define BTREE_NUM   (20)
#define INPUT_LEN   (32)
#define __AUTO_INPUT__

int main(void)
{
    int ret, key, idx;
    btree_t *btree;
    btree_opt_t opt;
    log_cycle_t *log;
    char input[INPUT_LEN];
    void *data;

    log = log_init(LOG_LEVEL_TRACE, "btree.log");
    if (NULL == log)
    {
        return -1;
    }

    /* > 创建B树 */
    opt.log = log;
    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    btree = btree_creat(BTREE_M, &opt);
    if (NULL == btree)
    {
        fprintf(stderr, "[%s][%d] Create btree failed!\n", __FILE__, __LINE__);
        return -1;
    }

    fprintf(stderr, "[%s][%d] Create btree success!\n", __FILE__, __LINE__);

    /* > 插入关键字 */
    for(idx=0; idx<BTREE_NUM; idx++)
    {
        btree_insert(btree, random()%20, (void *)0+idx);
    }

	btree_insert(btree, 14, (void *)0+idx);
	btree_insert(btree, 28, (void *)0+idx);
	btree_insert(btree, 34, (void *)0+idx);
	btree_insert(btree, 37, (void *)0+idx);
	btree_insert(btree, 48, (void *)0+idx);
	btree_insert(btree, 39, (void *)0+idx);
	btree_insert(btree, 38, (void *)0+idx);
	btree_insert(btree, 40, (void *)0+idx);

    btree_print(btree);

    /* > 操作B树 */
    while(1)
    {
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

            btree_remove(btree, key, &data);
            btree_print(btree);
            continue;
        }

        ret = btree_insert(btree, key, (void *)0+key);
        if (0 != ret)
        {
            fprintf(stderr, "[%s][%d] Insert failed!\n", __FILE__, __LINE__);
            break;
        }

        fprintf(stderr, "[%d] Insert btree success!\n", key);

        btree_print(btree);
    }

    btree_destroy(btree);
    return 0;
}
