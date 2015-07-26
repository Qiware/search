/******************************************************************************
 ** Copyright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: shm_list.c
 ** 版本号: 1.0
 ** 描  述: 双向链表的处理(通用双向链表)
 **         通过此处的函数简化双向链表的插入、删除操作，加快开发进程、减少人为
 **         错误.
 ** 作  者: # Qifeng.zou # 2015.07.26 #
 ******************************************************************************/
#include "shm_list.h"

/******************************************************************************
 **函数名称: shm_list_lpush
 **功    能: 插入链表头
 **输入参数: 
 **     addr: 起始地址
 **     list: 链表对象
 **     node: 新插入的结点(偏移 > 0)
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 空间在函数外部已经完成申请
 **作    者: # Qifeng.zou # 2015-07-26 22:50:32 #
 ******************************************************************************/
int shm_list_lpush(void *addr, shm_list_t *list, off_t node_off)
{
    shm_list_node_t *head, *node, *tail;

    node = (shm_list_node_t *)(addr + node_off);

    /* > 链表为空时 */
    if (0 == list->head)
    {
        list->head = node_off;
        node->prev = node_off;
        node->next = node_off;
        list->num = 1;
        return 0;
    }

    head = (shm_list_node_t *)(addr + list->head);
    tail = (shm_list_node_t *)(addr + head->prev);

    /* > 链表不空时 */
    node->prev = head->prev;
    node->next = list->head;
    head->prev = node_off;
    tail->next = node_off;
    list->head = node_off;
    ++list->num;
    return 0;
}

/******************************************************************************
 **函数名称: shm_list_lpop
 **功    能: 删除链表头
 **输入参数: 
 **     addr: 起始地址
 **     list: 双向链表
 **输出参数: NONE
 **返    回: 结点偏移
 **实现描述: 
 **注意事项: 链表结点的空间由外部进行回收
 **作    者: # Qifeng.zou # 2015-07-26 23:26:56 #
 ******************************************************************************/
off_t shm_list_lpop(void *addr, shm_list_t *list)
{
    off_t head_off;
    shm_list_node_t *head, *tail, *curr, *next;

    /* > 链表为空 */
    if (0 == list->head)
    {
        return 0;
    }

    head = (shm_list_node_t *)(addr + list->head);
    tail = (shm_list_node_t *)(addr + head->prev);

    head_off = list->head;

    /* > 链表只有１个结点 */
    if (head == tail)
    {
        list->head = 0;
        list->num = 0;
        return head_off;
    }

    /* > 链表有多个结点 */
    curr = head;
    next = (shm_list_node_t *)(addr + head->next);

    list->head = head->next;
    tail->next = head->next;
    next->prev = head->prev;
    --list->num;
    return head_off;
}

/******************************************************************************
 **函数名称: shm_list_rpush
 **功    能: 插入链表尾
 **输入参数: 
 **     addr: 起始地址
 **     list: 双向链表
 **     node_off: 新结点
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 请调用者自己取释放数据块空间
 **作    者: # Qifeng.zou # 2015-07-26 23:37:17 #
 ******************************************************************************/
int shm_list_rpush(void *addr, shm_list_t *list, off_t node_off)
{
    shm_list_node_t *head, *node, *tail;

    node = (shm_list_node_t *)(addr + node_off);

    /* > 链表为空 */
    if (0 == list->head)
    {
        list->head = node_off;
        node->prev = node_off;
        node->next = node_off;
        list->num = 1;
        return 0;
    }

    /* > 链表不空时 */
    head = (shm_list_node_t *)(addr + list->head);
    tail = (shm_list_node_t *)(addr + head->prev);

    tail->next = node_off;
    node->prev = head->prev;
    node->next = list->head;
    head->prev = node_off;
    ++list->num;
    return 0;
}

/******************************************************************************
 **函数名称: shm_list_rpop
 **功    能: 删除链表尾
 **输入参数: 
 **     addr: 起始地址
 **     list: 双向链表
 **输出参数: NONE
 **返    回: 尾节点偏移
 **实现描述: 
 **注意事项: 请调用者自己取释放结点和数据块空间
 **作    者: # Qifeng.zou # 2015-07-26 23:42:17 #
 ******************************************************************************/
off_t shm_list_rpop(void *addr, shm_list_t *list)
{
    off_t off;
    shm_list_node_t *head, *prev, *tail;

    /* > 无数据 */
    if (0 == list->head)
    {
        return 0; /* 无数据 */
    }

    /* > 只有１个结点 */
    head = (shm_list_node_t *)(addr + list->head);
    if (list->head == head->prev)
    {
        off = list->head;
        list->head = 0;
        list->num = 0;
        return off;
    }

    /* 3. 有多个结点 */
    off = head->prev;
    tail = (shm_list_node_t *)(addr + head->prev);
    prev = (shm_list_node_t *)(addr + tail->prev);

    prev->next = list->head;
    head->prev = tail->prev;
    --list->num;
    return off;
}
