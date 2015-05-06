#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "list.h"

#define LIST_NUM    (10)

typedef struct
{
    int idx;
} list_data_t;

int main(void)
{
    int idx;
    list_t list;
    list_data_t *data;
    list_node_t *prev;

    memset(&list, 0, sizeof(list));

    for (idx=0, prev=NULL; idx<LIST_NUM; ++idx)
    {
        data = (list_data_t *)calloc(1, sizeof(list_data_t));
        if (NULL == data)
        {
            fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
            return -1;
        }

        data->idx = idx;

        /* 插入测试 */
    #if 0
        list_push(&list, data);
        list_insert_tail(&list, data);
    #else        
        list_insert(&list, prev, data);
    #endif
    }

    /* 删除测试 */
    //list_remove(&list, node);
    list_lpop(&list);
    //list_remove_tail(&list);
    //list_remove(&list, del);

    return 0;
}
