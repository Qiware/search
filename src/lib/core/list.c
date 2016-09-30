/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: list.c
 ** 版本号: 1.0
 ** 描  述: 单向链表的处理(通用单向链表)
 **         通过此处的函数简化单向链表的插入、删除操作，加快开发进程、减少人为
 **         错误.
 ** 作  者: # Qifeng.zou # 2014.08.24 #
 ******************************************************************************/
#include "list.h"

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
    while (NULL != curr) {
        ++num;
        curr = curr->next;
    }

    if (num != list->num) {
        abort();
    }
}

/******************************************************************************
 **函数名称: list_creat
 **功    能: 创建链表
 **输入参数:
 **     opt: 选项信息
 **输出参数:
 **返    回: 链表对象
 **实现描述: 新建链表对象, 并初始化相关变量成员.
 **注意事项:
 **作    者: # Qifeng.zou # 2015.02.16 #
 ******************************************************************************/
list_t *list_creat(list_opt_t *opt)
{
    list_t *list;
    list_opt_t _opt;

    if (NULL == opt) {
        opt = &_opt;
        opt->pool = (void *)NULL;
        opt->alloc = (mem_alloc_cb_t)mem_alloc;
        opt->dealloc = (mem_dealloc_cb_t)mem_dealloc;
    }

    list = opt->alloc(opt->pool, sizeof(list_t));
    if (NULL == list) {
        return NULL;
    }

    list->num = 0;
    list->head = NULL;
    list->tail = NULL;

    list->pool = opt->pool;
    list->alloc = opt->alloc;
    list->dealloc = opt->dealloc;

    return list;
}

/******************************************************************************
 **函数名称: list_destroy
 **功    能: 销毁链表
 **输入参数:
 **     list: 链表对象
 **     dealloc: 释放回调(正对链表结点所挂的数据块)
 **     pool: 内存池(正对链表结点所挂的数据块)
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 遍历链表节点，依次释放节点和数据块内存
 **注意事项: 必须释放节点所挂数据块的空间, 否则将会出现严重的内存泄露!
 **作    者: # Qifeng.zou # 2015.02.17 #
 ******************************************************************************/
void list_destroy(list_t *list, mem_dealloc_cb_t dealloc, void *pool)
{
    list_node_t *curr, *next;

    for (curr = list->head; NULL != curr; curr = next) {
       next = curr->next;
       dealloc(pool, curr->data);
       list->dealloc(list->pool, curr);
    }

    list->dealloc(list->pool, list);
}

/******************************************************************************
 **函数名称: list_lpush
 **功    能: 链头插入
 **输入参数:
 **     list: 单向链表
 **     data: 数据
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.08.24 #
 ******************************************************************************/
int list_lpush(list_t *list, void *data)
{
    list_node_t *node;

    /* > 新建结点 */
    node = list->alloc(list->pool, sizeof(list_node_t));
    if (NULL == node) {
        return -1;
    }

    node->data = data;

    /* > 插入链表
     * 1. 链表为空时 */
    if (NULL == list->head) {
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
 **函数名称: list_rpush
 **功    能: 链尾插入
 **输入参数:
 **     list: 单向链表
 **     data: 数据
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 请调用者自己取释放返回结点和数据的内存空间
 **作    者: # Qifeng.zou # 2014.08.24 #
 ******************************************************************************/
int list_rpush(list_t *list, void *data)
{
    list_node_t *node;

    /* > 新建结点 */
    node = list->alloc(list->pool, sizeof(list_node_t));
    if (NULL == node) {
        return -1;
    }

    node->data = data;

    /* > 插入链尾
     * 1. 链表为空时 */
    if (!list->tail) {
        list->head = node;
        list->tail = node;
        node->next = NULL;
        list->num = 1;
        return 0;
    }

    /* 2. 链表不空时 */
    list->tail->next = node;
    list->tail = node;
    node->next = NULL;

    ++list->num;
    return 0;
}

/******************************************************************************
 **函数名称: list_insert
 **功    能: 在PREV后插入结点
 **输入参数:
 **     list: 单向链表
 **     prev: 前一结点
 **     data: 数据
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **     1. 如果结点prev为NULL, 则将结点node插入链表头
 **     2. 如果结点prev为tail, 则将结点node插入链表尾
 **     3. 如果结点prev不为NULL, 则将结点node插入prev后
 **作    者: # Qifeng.zou # 2014.09.24 #
 ******************************************************************************/
int list_insert(list_t *list, list_node_t *prev, void *data)
{
    list_node_t *node;

    /* > 插入链头或链尾 */
    if (NULL == prev) {
        return list_lpush(list, data);
    }
    else if (list->tail == prev) {
        return list_rpush(list, data);
    }

    /* > 新建结点 */
    node = list->alloc(list->pool, sizeof(list_node_t));
    if (NULL == node) {
        return -1;
    }

    node->data = data;

    /* > 插入链表 */
    node->next = prev->next;
    prev->next = node;
    ++list->num;
    return 0;
}

/******************************************************************************
 **函数名称: list_lpop
 **功    能: 弹出链头
 **输入参数:
 **     list: 单向链表
 **输出参数: NONE
 **返    回: 链头数据
 **实现描述:
 **注意事项: 请调用者自己释放node->data数据的内存空间
 **作    者: # Qifeng.zou # 2014.09.24 #
 ******************************************************************************/
void *list_lpop(list_t *list)
{
    void *data;
    list_node_t *head;

    /* 1. 无数据 */
    if (NULL == list->head) {
        return NULL;
    }
    /* 2. 只有１个结点 */
    else if (list->head == list->tail) {
        head = list->head;

        list->tail = NULL;
        list->head = NULL;
        list->num = 0;

        data = head->data;
        list->dealloc(list->pool, head);
        return data;
    }

    /* 3. 有多个结点 */
    head = list->head;
    list->head = head->next;
    --list->num;

    data = head->data;

    list->dealloc(list->pool, head);

    return data;
}

/******************************************************************************
 **函数名称: list_rpop
 **功    能: 弹出链尾
 **输入参数:
 **     list: 单向链表
 **输出参数: NONE
 **返    回: 链尾数据
 **实现描述:
 **注意事项: 请调用者自己释放node->data数据的内存空间
 **作    者: # Qifeng.zou # 2014.08.24 #
 ******************************************************************************/
void *list_rpop(list_t *list)
{
    void *data;
    list_node_t *prev, *tail;

    /* 1. 无数据 */
    if (NULL == list->head) {
        return NULL;
    }
    /* 2. 只有１个结点 */
    else if (list->head == list->tail) {
        tail = list->tail;

        list->tail = NULL;
        list->head = NULL;
        list->num = 0;

        data = tail->data;
        list->dealloc(list->pool, tail);
        return data;
    }

    /* 3. 有多个结点 */
    prev = list->head;
    tail = list->tail;
    while (prev->next != list->tail) {
        prev = prev->next;
    }

    prev->next = NULL;
    list->tail = prev;

    --list->num;

    data = tail->data;
    list->dealloc(list->pool, tail);
    return data;
}

/******************************************************************************
 **函数名称: list_remove
 **功    能: 删除PREV后的结点NODE
 **输入参数:
 **     list: 单向链表
 **     data: 需删数据块
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.24 #
 ******************************************************************************/
int list_remove(list_t *list, void *data)
{
    list_node_t *prev, *curr;

    prev = list->head;
    curr = list->head;
    while (NULL != curr) {
        if (curr->data == data) {
            /* 删除头结点 */
            if (list->head == curr) {
                if (list->head == list->tail) {
                    list->head = NULL;
                    list->tail = NULL;
                    list->num = 0;

                    list->dealloc(list->pool, curr);
                    return 0;
                }

                list->head = curr->next;
                --list->num;

                list->dealloc(list->pool, curr);
                return 0;
            }
            /* 删除尾结点 */
            else if (curr == list->tail) {
                prev->next = curr->next;
                list->tail = prev;
                --list->num;

                list->dealloc(list->pool, curr);
                return 0;
            }

            prev->next = curr->next;
            --list->num;

            list->dealloc(list->pool, curr);
            return 0;
        }

        prev = curr;
        curr = curr->next;
    }

    return -1;
}

/******************************************************************************
 **函数名称: list_fetch
 **功    能: 返回链表第X个结点的数据
 **输入参数:
 **     list: 单向链表
 **     idx: 结点序列号
 **输出参数: NONE
 **返    回: 第X个结点的数据
 **实现描述:
 **注意事项:
 **     1. 从0开始计数, 如果idx > list->num,则返回NULL!
 **     2. 结点的空间此时并没有被释放!
 **作    者: # Qifeng.zou # 2015.06.13 17:29:31 #
 ******************************************************************************/
void *list_fetch(list_t *list, int idx)
{
    int i = 0;
    list_node_t *node;

    if (idx >= list->num) {
        return NULL;
    }

    node  = list->head;
    for (i=0; i<idx; ++i) {
        node = node->next;
    }

    return node->data;
}

/******************************************************************************
 **函数名称: list_sort
 **功    能: 有序插入链表(降序)
 **输入参数:
 **     list: 单向链表
 **     data: 需要插入的数据
 **     cmp: 比较函数指针
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 如果想改为升序则只需修改回调函数cmp的返回值.
 **作    者: # Qifeng.zou # 2015-07-01 23:52:35 #
 ******************************************************************************/
int list_sort(list_t *list, void *data, cmp_cb_t cmp)
{
    int ret;
    list_node_t *curr, *prev;

    if (NULL == list->head) {
        return list_lpush(list, data);
    }

    prev = NULL;
    curr=list->head;
    for (; NULL!=curr; curr=curr->next) {
        ret = cmp(data, curr->data);
        if ((0 == ret) || (ret > 0)) {
            return list_insert(list, prev, data); /* 插入PREV之后 */
        }
        prev = curr;
    }

    return list_insert(list, prev, data); /* 插入链尾 */
}

/******************************************************************************
 **函数名称: list_trav
 **功    能: 遍历链表
 **输入参数:
 **     list: 单向链表
 **     proc: 对每个链表结点的处理回调
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016-01-06 22:47:55 #
 ******************************************************************************/
int list_trav(list_t *list, trav_cb_t proc, void *args)
{
    list_node_t *node;

    node = list->head;
    for (; NULL != node; node=node->next) {
        if (proc(node->data, args)) {
            return -1;
        }
    }
    return 0;
}

/******************************************************************************
 **函数名称: list_find
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
void *list_find(list_t *list, find_cb_t cb, void *args)
{
    list_node_t *node;

    node = list->head;
    for (; NULL != node; node = node->next) {
        if (cb(node->data, args)) {
            return node->data;
        }
    }

    return NULL;
}

/******************************************************************************
 **函数名称: list_find_and_del
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
void *list_find_and_del(list_t *list, find_cb_t cb, void *args)
{
    void *data;
    list_node_t *node, *prev = NULL;

    node = list->head;
    for (; NULL != node; prev = node, node = node->next) {
        if (cb(node->data, args)) {
            data = node->data;
            --list->num;
            if (0 == list->num) {
                list->head = NULL;
            }
            else {
                if (NULL != prev) {
                    prev->next = node->next;
                }

                if (node == list->head) {
                    list->head = node->next;
                }
            }
            list->dealloc(list->pool, node);
            return data;
        }
    }

    return NULL;
}
