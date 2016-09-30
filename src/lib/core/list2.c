/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
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
 **实现描述: 出现异常直接abort()
 **注意事项:
 **作    者: # Qifeng.zou # 2014.08.24 #
 ******************************************************************************/
void list2_assert(list2_t *list)
{
    int num = 0;
    list2_node_t *curr;

    if (NULL == list->head) {
        if (list->num) { abort(); }
        return;
    }

    curr = list->head;
    do {
        ++num;
        curr = curr->next;
        if (NULL == curr->data
            || NULL == curr->prev
            || NULL == curr->next) {
            abort();
        }
    } while (curr != list->head);

    if (num != list->num) { abort(); }
}

/******************************************************************************
 **函数名称: list2_creat
 **功    能: 创建双向链表
 **输入参数:
 **     opt: 链表选项
 **输出参数: NONE
 **返    回: 双向链表
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.11 23:51:29 #
 ******************************************************************************/
list2_t *list2_creat(list2_opt_t *opt)
{
    list2_t *list;
    list2_opt_t _opt;

    if (NULL == opt) {
        opt = &_opt;
        opt->pool = (void *)NULL;
        opt->alloc = (mem_alloc_cb_t)mem_alloc;
        opt->dealloc = (mem_dealloc_cb_t)mem_dealloc;
    }

    list = opt->alloc(opt->pool, sizeof(list2_t));
    if (NULL == list) {
        return NULL;
    }

    list->num = 0;
    list->head = NULL;

    list->pool = opt->pool;
    list->alloc = opt->alloc;
    list->dealloc = opt->dealloc;

    return list;
}

/******************************************************************************
 **函数名称: list2_lpush
 **功    能: 插入链表头
 **输入参数:
 **     list: 双向链表
 **     data: 数据块
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 新结点的空间有外界分配，删除时，请记得释放空间.
 **作    者: # Qifeng.zou # 2014.08.24 #
 ******************************************************************************/
int list2_lpush(list2_t *list, void *data)
{
    list2_node_t *node, *tail;

    node = list->alloc(list->pool, sizeof(list2_node_t));
    if (NULL == node) {
        return -1;
    }

    node->data = data;

    /* 1. 链表为空时 */
    if (NULL == list->head) {
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
 **函数名称: list2_lpop
 **功    能: 删除链表头
 **输入参数:
 **     list: 双向链表
 **输出参数: NONE
 **返    回: 数据块
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.08.24 #
 ******************************************************************************/
void *list2_lpop(list2_t *list)
{
    void *data;
    list2_node_t *tail, *curr;

    /* 1. 链表为空 */
    if (NULL == list->head) {
        return NULL;
    }

    tail = list->head->prev;

    /* 2. 链表只有１个结点 */
    if (list->head == tail) {
        list->head = NULL;
        list->num = 0;

        data = tail->data;
        list->dealloc(list->pool, tail);
        return data;
    }

    /* 3. 链表有多个结点 */
    curr = list->head;

    list->head = list->head->next;
    tail->next = list->head;
    list->head->prev = tail;

    --list->num;

    data = curr->data;
    list->dealloc(list->pool, curr);
    return data;
}

/******************************************************************************
 **函数名称: list2_delete
 **功    能: 删除链表头
 **输入参数:
 **     list: 双向链表
 **     node: 被删结点
 **输出参数:
 **返    回: 结点地址
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.10 #
 ******************************************************************************/
void *list2_delete(list2_t *list, list2_node_t *node)
{
    void *data = node->data;

    /* 1. 只有一个结点时 */
    if (node == node->prev) {
        list->num = 0;
        list->head = NULL;

        list->dealloc(list->pool, node);
        return data;
    }

    /* 2. 含有多个结点时 */
    if (node == list->head) {
        list->head = node->next;
    }

    node->prev->next = node->next;
    node->next->prev = node->prev;

    --list->num;

    list->dealloc(list->pool, node);
    return data;
}

/******************************************************************************
 **函数名称: list2_rpush
 **功    能: 插入链表尾
 **输入参数:
 **     list: 双向链表
 **     node: 新结点
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 请调用者自己取释放数据块空间
 **作    者: # Qifeng.zou # 2014.08.24 #
 ******************************************************************************/
int list2_rpush(list2_t *list, void *data)
{
    list2_node_t *node, *tail;

    node = list->alloc(list->pool, sizeof(list2_node_t));
    if (NULL == node) {
        return -1;
    }

    node->data = data;

    /* 1. 链表为空 */
    if (NULL == list->head) {
        list->head = node;
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
 **函数名称: list2_rpop
 **功    能: 删除链表尾
 **输入参数:
 **     list: 双向链表
 **输出参数: NONE
 **返    回: 尾节点数据块地址
 **实现描述:
 **注意事项: 请调用者自己取释放数据块空间
 **作    者: # Qifeng.zou # 2014.08.24 #
 ******************************************************************************/
void *list2_rpop(list2_t *list)
{
    void *data;
    list2_node_t *prev, *tail;

    /* 1. 无数据 */
    if (NULL == list->head) {
        return NULL;
    }
    /* 2. 只有１个结点 */
    else if (list->head == list->head->prev) {
        tail = list->head;

        list->head = NULL;
        list->num = 0;

        data = tail->data;
        list->dealloc(list->pool, tail);
        return data;
    }

    /* 3. 有多个结点 */
    tail = list->head->prev;
    prev = tail->prev;

    prev->next = list->head;
    list->head->prev = prev;

    --list->num;

    data = tail->data;
    list->dealloc(list->pool, tail);
    return data;
}

/******************************************************************************
 **函数名称: list2_trav
 **功    能: 遍历扫描链表
 **输入参数:
 **     list: 链表对象
 **     cb: 遍历回调
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.02 #
 ******************************************************************************/
int list2_trav(list2_t *list, trav_cb_t cb, void *args)
{
    list2_node_t *node, *tail;

    node = list->head;
    if (NULL != node) {
        tail = node->prev;
    }

    for (; NULL != node; node = node->next) {
        cb(node->data, args);
        if (node == tail) { break; }
    }

    return 0;
}

/******************************************************************************
 **函数名称: list2_find
 **功    能: 查找链表
 **输入参数:
 **     list: 链表对象
 **     cb: 查找回调
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 挂载数据
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.04.09 07:32:57 #
 ******************************************************************************/
void *list2_find(list2_t *list, find_cb_t cb, void *args)
{
    list2_node_t *node, *tail;

    node = list->head;
    if (NULL == node) {
        return NULL;
    }

    tail = node->prev;
    for (; ; node = node->next) {
        if (cb(node->data, args)) {
            return node->data;
        }
        if (node == tail) { break; }
    }

    return NULL;
}

/******************************************************************************
 **函数名称: list2_find_and_del
 **功    能: 查找&删除满足条件的一个结点
 **输入参数:
 **     list: 链表对象
 **     cb: 查找回调
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 挂载数据
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.07.28 22:28:18 #
 ******************************************************************************/
void *list2_find_and_del(list2_t *list, find_cb_t cb, void *args)
{
    void *data;
    list2_node_t *node, *tail;

    node = list->head;
    if (NULL == node) {
        return NULL;
    }

    tail = node->prev;
    for (; ; node = node->next) {
        if (cb(node->data, args)) {
            data = node->data;
            --list->num;
            if (0 == list->num) {
                list->head = NULL;
            }
            else {
                node->prev->next = node->next;
                node->next->prev = node->prev;
                if (node == list->head) {
                    list->head = node->next;
                }
            }

            list->dealloc(list->pool, node);
            return data;
        }
        if (node == tail) { break; }
    }

    return NULL;
}

/******************************************************************************
 **函数名称: list2_roll
 **功    能: 滚动链表
 **输入参数:
 **     list: 链表对象
 **输出参数: NONE
 **返    回: 链表头挂载数据
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.07.28 13:36:16 #
 ******************************************************************************/
void *list2_roll(list2_t *list)
{
    void *data;

    if (NULL == list->head) {
        return NULL;
    }

    data = list->head->data;
    list->head = list->head->next;

    return data;
}

/******************************************************************************
 **函数名称: list2_destroy
 **功    能: 销毁链表
 **输入参数:
 **     list: 链表对象
 **     dealloc: 内存回收
 **     pool: 内存池
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.07.28 22:18:20 #
 ******************************************************************************/
void list2_destroy(list2_t *list, mem_dealloc_cb_t dealloc, void *pool)
{
    list2_node_t *curr, *next, *tail;

    if (NULL == list->head) {
        list->dealloc(list->pool, list);
        return;
    }

    tail = list->head->prev;
    for (curr = list->head; NULL != curr; curr = next) {
       next = curr->next;
       dealloc(pool, curr->data);
       list->dealloc(list->pool, curr);
       if (curr == tail) {
           break;
       }
    }

    list->dealloc(list->pool, list);
}
