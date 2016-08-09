#include "avl_tree.h"

#define INPUT_LEN   (128)
#define __AUTO_INPUT__

typedef struct
{
    uint64_t id;
} avl_data_t;

/* 打印结点 */
void avl_trav_print(avl_data_t *data, void *args)
{
    static uint32_t idx = 0;

    fprintf(stderr, "idx:%u id:%lu\n", ++idx, data->id);
}

/* 比较回调 */
void avl_cmp_cb(const avl_data_t *data1, const avl_data_t *data2)
{
    return (data1->id - data2->id);
}

int main(void)
{
    uint32_t key;
    int ret, idx, n;
    avl_tree_t *avl;
    rbt_tree_t *rbt;
    avl_data_t *data;
    char input[INPUT_LEN];

    /* > 创建AVL树 */
    avl = avl_creat(NULL, (cmp_cb_t)avl_cmp_cb);
    if (NULL == avl) {
        fprintf(stderr, "[%s][%d] Create avl failed!\n", __FILE__, __LINE__);
        return -1;
    }

    fprintf(stderr, "[%s][%d] Create avl success!\n", __FILE__, __LINE__);

    /* > 插入关键字 */
    for (n=0; n<5; n++) {
        for(idx=0; idx<1000; idx++) {
            key = idx;//random();
            data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
            data->id = key;
            avl_insert(avl, &key, sizeof(key), data);
        }
        for(idx=0; idx<1000-5; idx++) {
            key = idx;//random();
            data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
            data->id = key;
            avl_delete(avl, &key, sizeof(key), (void **)data);
            free(data);
        }
    }

    avl_trav(avl, (trav_cb_t)avl_trav_print, NULL);
    return 0;

    key = 14;
    data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
    data->id = key;
	avl_insert(avl, &key, sizeof(key), (void *)data);

    key = 28;
    data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
    data->id = key;
	avl_insert(avl, &key, sizeof(key), (void *)data);

    key = 34;
    data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
    data->id = key;
	avl_insert(avl, &key, sizeof(key), (void *)data);

    key = 37;
    data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
    data->id = key;
	avl_insert(avl, &key, sizeof(key), (void *)data);

    key = 48;
    data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
    data->id = key;
	avl_insert(avl, &key, sizeof(key), (void *)data);

    key = 39;
    data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
    data->id = key;
	avl_insert(avl, &key, sizeof(key), (void *)data);

    key = 38;
    data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
    data->id = key;
	avl_insert(avl, &key, sizeof(key), (void *)data);

    key = 40;
    data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
    data->id = key;
	avl_insert(avl, &key, sizeof(key), (void *)data);

    avl_print(avl);
    avl_trav(avl, (trav_cb_t)avl_trav_print, NULL);

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

            avl_delete(avl, &key, sizeof(key), (void *)&data);
            avl_print(avl);
            continue;
        }

        data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
        data->id = key;

        ret = avl_insert(avl, &key, sizeof(key), (void *)data);
        if (0 != ret) {
            fprintf(stderr, "[%s][%d] Insert failed!\n", __FILE__, __LINE__);
            break;
        }

        fprintf(stderr, "[%u] Insert avl success!\n", key);

        //avl_print(avl);
        avl_trav(avl, (trav_cb_t)avl_trav_print, NULL);
    }

    avl_destroy(avl, mem_dummy_dealloc, NULL);
    return 0;
}
