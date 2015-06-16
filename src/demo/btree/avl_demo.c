#include "avl_tree.h"

#define INPUT_LEN   (128)
#define __AUTO_INPUT__

int main(void)
{
    void *data;
    uint64_t key;
    int ret, idx;
    avl_opt_t opt;
    avl_tree_t *avl;
    char input[INPUT_LEN];

    /* > 创建B树 */
    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    avl = avl_creat(&opt, (key_cb_t)avl_key_cb_int64, (avl_cmp_cb_t)avl_cmp_cb_int64);
    if (NULL == avl)
    {
        fprintf(stderr, "[%s][%d] Create avl failed!\n", __FILE__, __LINE__);
        return -1;
    }

    fprintf(stderr, "[%s][%d] Create avl success!\n", __FILE__, __LINE__);

    /* > 插入关键字 */
    for(idx=0; idx<100; idx++)
    {
        key = random();
        avl_insert(avl, &key, sizeof(key), NULL);
    }

    key = 14;
	avl_insert(avl, &key, sizeof(key), NULL);
    key = 28;
	avl_insert(avl, &key, sizeof(key), NULL);
    key = 34;
	avl_insert(avl, &key, sizeof(key), NULL);
    key = 37;
	avl_insert(avl, &key, sizeof(key), NULL);
    key = 48;
	avl_insert(avl, &key, sizeof(key), NULL);
    key = 39;
	avl_insert(avl, &key, sizeof(key), NULL);
    key = 38;
	avl_insert(avl, &key, sizeof(key), NULL);
    key = 40;
	avl_insert(avl, &key, sizeof(key), NULL);

    avl_print(avl);

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

            avl_delete(avl, &key, sizeof(key), &data);
            avl_print(avl);
            continue;
        }

        ret = avl_insert(avl, &key, sizeof(key), NULL);
        if (0 != ret)
        {
            fprintf(stderr, "[%s][%d] Insert failed!\n", __FILE__, __LINE__);
            break;
        }

        fprintf(stderr, "[%lu] Insert avl success!\n", key);

        avl_print(avl);
    }

    avl_destroy(avl, NULL, NULL);
    return 0;
}
