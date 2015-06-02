/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: list2.c
 ** 版本号: 1.0
 ** 描  述: 双向链表的处理(通用双向链表)
 **         通过此处的函数简化双向链表的插入、删除操作，加快开发进程、减少人为
 **         错误.
 ** 作  者: # Qifeng.zou # 2014.08.24 #
 ******************************************************************************/
#include "list2.h"

/******************************************************************************
 **函数名称: list2_assert
 **功    能: 链表检测
 **输入参数:
 **     list: 单向链表
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.08.24 #
 ******************************************************************************/
void list2_assert(list2_t *list)
{
    int num = 0;
    list2_node_t *curr = list->head, *tail;

    if (NULL == curr)
    {
        if (list->num)
        {
            abort();
        }
        return;
    }

    tail = list->head->prev;
    while (tail != curr)
    {
        ++num;
        curr = curr->next;
    }

    if (num != list->num
        || (list->num > 0
            && NULL == list->head->prev))
    {
        abort();
    }
}

/******************************************************************************
 **函数名称: list2_trav
 **功    能: 遍历扫描链表
 **输入参数:
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.02 #
 ******************************************************************************/
int list2_trav(list2_t *list, list2_trav_cb_t cb, void *args)
{
    list2_node_t *node, *tail;

    node = list->head;
    if (NULL != node) {
        tail = node->prev;
    }

    for (; NULL != node; node = node->next)
    {
        cb(node->data, args);

        if (node == tail) { break; }
    }

    return 0;
}
