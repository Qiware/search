#if !defined(__LIST2_H__)
#define __LIST2_H__

/* 双向链表结点 */
typedef struct _list2_node_t
{
    void *data;
    struct _list2_node_t *prev;
    struct _list2_node_t *next;
} list2_node_t;

/* 双向链表对象 */
typedef struct
{
    int num;
    list2_node_t *head;
} list2_t;

/******************************************************************************
 **函数名称: list2_insert
 **功    能: 插入链表头
 **输入参数: 
 **     list: 双向链表
 **     node: 新节点
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     新结点的空间有外界分配，删除时，请记得释放空间.
 **作    者: # Qifeng.zou # 2014.08.24 #
 ******************************************************************************/
static inline int list2_insert(list2_t *list, list2_node_t *node)
{
    list2_node_t *tail;

    /* 1. 链表为空时 */
    if (NULL == list->head)
    {
        list->head = node;
        node->prev = node;
        node->next = node;
        list->num = 1;
        return 0;
    }

    /* 2. 链表不空时 */
    tail = list->head->prev;

    node->next = list->head;
    list->head->prev = node;
    node->prev = tail;
    tail->next = node;
    list->head = node;
    ++list->num;
    return 0;
}

/******************************************************************************
 **函数名称: list2_delete
 **功    能: 删除链表头
 **输入参数: 
 **     list: 双向链表
 **     node: 新结点
 **输出参数: 
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.08.24 #
 ******************************************************************************/
static inline list2_node_t *list2_delete(list2_t *list)
{
    list2_node_t *tail, *curr;

    /* 1. 链表为空 */
    if (NULL == list->head)
    {
        return NULL;
    }

    tail = list->head->prev;

    /* 2. 链表只有１个结点 */
    if (list->head == tail)
    {
        list->head = NULL;
        list->num = 0;
        return tail;
    }

    /* 3. 链表有多个结点 */
    curr = list->head;

    list->head = list->head->next;
    tail->next = list->head;
    list->head->prev = tail;

    --list->num;
    return curr;
}

/******************************************************************************
 **函数名称: list2_insert_tail
 **功    能: 插入链表尾
 **输入参数: 
 **     list: 双向链表
 **     node: 新结点
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     请调用者自己取释放返回结点和数据的内存空间
 **作    者: # Qifeng.zou # 2014.08.24 #
 ******************************************************************************/
static inline int list2_insert_tail(list2_t *list, list2_node_t *node)
{
    list2_node_t *tail;

    /* 1. 链表为空 */
    if (NULL == list->head)
    {
        list->head = node;
        list->head->prev = node;
        node->prev = node;
        node->next = node;

        list->num = 1;
        return 0;
    }

    tail = list->head->prev;
    /* 2. 链表不空时 */
    tail->next = node;
    node->prev = tail;
    node->next = list->head;
    list->head->prev = node;

    ++list->num;
    return 0;
}

/******************************************************************************
 **函数名称: list2_delete_tail
 **功    能: 删除链表尾
 **输入参数: 
 **     list: 双向链表
 **输出参数: NONE
 **返    回: 尾结点地址
 **实现描述: 
 **注意事项: 
 **     请调用者自己取释放返回结点和数据的内存空间
 **作    者: # Qifeng.zou # 2014.08.24 #
 ******************************************************************************/
static inline list2_node_t *list2_delete_tail(list2_t *list)
{
    list2_node_t *prev, *tail;

    /* 1. 无数据 */
    if (NULL == list->head)
    {
        return NULL;
    }
    /* 2. 只有１个结点 */
    else if (list->head == list->head->prev)
    {
        tail = list->head;

        list->head = NULL;
        list->num = 0;
        return tail;
    }

    /* 3. 有多个结点 */
    tail = list->head->prev;
    prev = tail->prev;

    prev->next = list->head;
    list->head->prev = prev;

    --list->num;
    return tail;
}
#endif /*__LIST2_H__*/
