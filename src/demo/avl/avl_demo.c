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
int avl_cmp_cb(const avl_data_t *data1, const avl_data_t *data2)
{
    return (data1->id - data2->id);
}

int main(void)
{
    int ret;
    uint32_t id;
    avl_tree_t *avl;
    avl_data_t *data, key;
    char input[INPUT_LEN];

    /* > 创建AVL树 */
    avl = avl_creat(NULL, (cmp_cb_t)avl_cmp_cb);
    if (NULL == avl) {
        fprintf(stderr, "[%s][%d] Create avl failed!\n", __FILE__, __LINE__);
        return -1;
    }

    fprintf(stderr, "[%s][%d] Create avl success!\n", __FILE__, __LINE__);

#if 0
    /* > 插入关键字 */
    int n;
    for (n=0; n<5; n++) {
        int idx;
        for(idx=0; idx<1000; idx++) {
            id = idx;//random();
            data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
            data->id = id;
            avl_insert(avl, data);
        }
        for(idx=0; idx<1000-5; idx++) {
            id = idx;//random();
            data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
            data->id = id;

            key.id = id;
            avl_delete(avl, (void *)&key, (void **)data);
            free(data);
        }
    }
#endif

    id = 256;
    data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
    data->id = id;
	avl_insert(avl, (void *)data);

    id = 1280;
    data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
    data->id = id;
	avl_insert(avl, (void *)data);

    avl_trav(avl, (trav_cb_t)avl_trav_print, NULL);

    /////////////////
    avl_destroy(avl, (mem_dealloc_cb_t)mem_dealloc, NULL);

    fprintf(stderr, "Destroy avl success!\n");

    return 0;
    /////////////////

    id = 34;
    data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
    data->id = id;
	avl_insert(avl, (void *)data);

    id = 37;
    data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
    data->id = id;
	avl_insert(avl, (void *)data);

    id = 48;
    data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
    data->id = id;
	avl_insert(avl, (void *)data);

    id = 39;
    data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
    data->id = id;
	avl_insert(avl, (void *)data);

    id = 38;
    data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
    data->id = id;
	avl_insert(avl, (void *)data);

    id = 40;
    data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
    data->id = id;
	avl_insert(avl, (void *)data);

    avl_print(avl);
    avl_trav(avl, (trav_cb_t)avl_trav_print, NULL);


    avl_destroy(avl, (mem_dealloc_cb_t)mem_dealloc, NULL);
    return 0;

    /* > 操作B树 */
    while(1) {
        memset(input, 0, sizeof(input));
    
    #if defined(__AUTO_INPUT__)
        fprintf(stdout, "Input:");
        scanf(" %s", input);
        id = atoi(input);
    #else
        id = random()%BTREE_NUM;
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
            id = atoi(input);

            key.id = id;

            avl_delete(avl, (void *)&key, (void *)&data);
            avl_print(avl);
            continue;
        }

        data = (avl_data_t *)calloc(1, sizeof(avl_data_t));
        data->id = id;

        ret = avl_insert(avl, (void *)data);
        if (0 != ret) {
            fprintf(stderr, "[%s][%d] Insert failed!\n", __FILE__, __LINE__);
            break;
        }

        fprintf(stderr, "[%u] Insert avl success!\n", id);

        //avl_print(avl);
        avl_trav(avl, (trav_cb_t)avl_trav_print, NULL);
        break;
    }

    avl_destroy(avl, (mem_dealloc_cb_t)mem_dealloc, NULL);
    fprintf(stderr, "Destroy avl success!\n");
    return 0;
}
