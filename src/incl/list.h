#if !defined(__LIST_H__)
#define __LIST_H__

/* 单向链表结点 */
typedef struct _list_node_t
{
    void *data;
    struct _list_node_t *next;
} list_node_t;

/* 单向链表对象 */
typedef struct
{
    int num;
    list_node_t *head;
    list_node_t *tail;
} list_t;

void list_assert(list_t *list);

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
static inline int list_insert_head(list_t *list, list_node_t *node)
{
    /* 1. 链表为空时 */
    if (NULL == list->head)
    {
        list->head = node;
        list->tail = node;
        node->next = NULL;

        list->num = 1;
        return 0;
    }

    /* 2. 链表不空时 */
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
static inline int list_insert_tail(list_t *list, list_node_t *node)
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
 **函数名称: list_insert
 **功    能: 在PREV后插入结点NODE
 **输入参数: 
 **     list: 单向链表
 **     prev: 前一结点
 **     node: 新结点
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     1. 如果结点prev为NULL, 则将结点node插入链表头
 **     2. 如果结点prev为tail, 则将结点node插入链表尾
 **     3. 如果结点prev不为NULL, 则将结点node插入prev后
 **作    者: # Qifeng.zou # 2014.09.24 #
 ******************************************************************************/
static inline int list_insert(list_t *list, list_node_t *prev, list_node_t *node)
{
    if (NULL == prev)
    {
        return list_insert_head(list, node);
    }
    else if (list->tail == prev)
    {
        return list_insert_tail(list, node);
    }

    node->next = prev->next;
    prev->next = node;
    ++list->num;
    return 0;
}

/******************************************************************************
 **函数名称: list_remove_head
 **功    能: 删除链表头
 **输入参数: 
 **     list: 单向链表
 **输出参数: NONE
 **返    回: 头结点地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.24 #
 ******************************************************************************/
static inline list_node_t *list_remove_head(list_t *list)
{
    list_node_t *head;

    /* 1. 无数据 */
    if (NULL == list->head)
    {
        return NULL;
    }
    /* 2. 只有１个结点 */
    else if (list->head == list->tail)
    {
        head = list->head;

        list->tail = NULL;
        list->head = NULL;
        list->num = 0;
        return head;
    }

    /* 3. 有多个结点 */
    head = list->head;
    list->head = head->next;
    --list->num;
    return head;
}

/******************************************************************************
 **函数名称: list_remove_tail
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
static inline list_node_t *list_remove_tail(list_t *list)
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
 **函数名称: list_remove
 **功    能: 删除PREV后的结点NODE
 **输入参数: 
 **     list: 单向链表
 **     node: 需删结点
 **输出参数: NONE
 **返    回: 结点地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.24 #
 ******************************************************************************/
static inline list_node_t *list_remove(list_t *list, list_node_t *node)
{
    list_node_t *prev, *curr;

    prev = list->head;
    curr = list->head;
    while (NULL != curr)
    {
        if (curr == node)
        {
            /* 删除头结点 */
            if (list->head == curr)
            {
                if (list->head == list->tail)
                {
                    list->head = NULL;
                    list->tail = NULL;
                    list->num = 0;
                    return node;
                }

                list->head = curr->next;
                --list->num;
                return node;
            }
            /* 删除尾结点 */
            else if (curr == list->tail)
            {
                prev->next = curr->next;
                list->tail = prev;
                --list->num;
                return node;
            }

            prev->next = curr->next;
            --list->num;
            return node;
        }

        prev = curr;
        curr = curr->next;
    }

    return node;
}
#endif /*__LIST_H__*/
