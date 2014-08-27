#include "list.h"

/******************************************************************************
 **函数名称: list_insert_head
 **功    能: 插入链表头
 **输入参数: 
 **     list: 单向链表
 **     node: 新结点
 **输出参数: 
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.08.24 #
 ******************************************************************************/
int list_insert_head(list_t *list, list_node_t *node)
{
    /* 1. 链表为空时 */
    if (NULL == list->head)
    {
        list->head = node;
        list->tail = node;
        node->prev = node;
        node->next = node;

        list->num = 1;
        return 0;
    }

    /* 1. 链表不空时 */
    node->next = list->head;
    list->head = node;

    ++list->num;
    return 0;
}

/******************************************************************************
 **函数名称: list_insert_tail
 **功    能: 插入链表尾
 **输入参数: 
 **     list: 单向链表
 **     node: 新结点
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     请调用者自己取释放返回结点和数据的内存空间
 **作    者: # Qifeng.zou # 2014.08.24 #
 ******************************************************************************/
int list_insert_tail(list_t *list, list_node_t *node)
{
    /* 1. 链表为空时 */
    if (NULL == list->tail)
    {
        list->head = node;
        list->tail = node;
        node->next = NULL;

        list->num = 1;
        return 0;
    }

    /* 1. 链表不空时 */
    list->tail->next = node;
    list->tail = node;
    node->next = NULL;

    ++list->num;
    return 0;

}

/******************************************************************************
 **函数名称: list_delete_tail
 **功    能: 删除链表尾
 **输入参数: 
 **     list: 单向链表
 **输出参数: NONE
 **返    回: 尾结点地址
 **实现描述: 
 **注意事项: 
 **     请调用者自己取释放返回结点和数据的内存空间
 **作    者: # Qifeng.zou # 2014.08.24 #
 ******************************************************************************/
list_node_t *list_delete_tail(list_t *list)
{
    list_node_t *prev, *tail;

    /* 1. 无数据 */
    if (NULL == list->head)
    {
        return NULL;
    }
    /* 2. 只有１个结点 */
    else if (list->head == list->tail)
    {
        tail = list->tail;

        list->tail = NULL;
        list->head = NULL;
        list->num = 0;
        return tail;
    }

    /* 3. 有多个结点 */
    prev = list->head;
    tail = list->tail;
    while (prev->next != list->tail)
    {
        prev = prev->next;
    }

    prev->next = NULL;
    list->tail = prev;

    --list->num;
    return tail;
}

/******************************************************************************
 **函数名称: list_assert
 **功    能: 链表检测
 **输入参数: 
 **     list: 单向链表
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.08.24 #
 ******************************************************************************/
void list_assert(list_t *list)
{
    int num = 0;
    list_node_t *curr;

    curr = list->head;
    while (NULL != curr)
    {
        ++num;
        curr = curr->next;
    }

    if (num != list->num)
    {
        abort();
    }
}
