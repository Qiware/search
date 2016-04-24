/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: shm_hash.c
 ** 版本号: 1.0
 ** 描  述:
 ** 作  者: # Qifeng.zou # Sat 25 Jul 2015 10:54:37 AM CST #
 ******************************************************************************/

#include "shm_opt.h"
#include "shm_hash.h"
#include "hash_alg.h"

#define SHM_HASH_TOTAL_SIZE(len, max, size) /* 计算哈希空间 */\
    (sizeof(shm_hash_head_t) + 2 * shm_ring_total(max) \
     + (len) * sizeof(shm_hash_slot_t) \
     + (max) * (sizeof(shm_list_node_t) + (size)))

/* 获取各段偏移 */
#define SHM_HASH_HEAD_OFFSET(len, max, size) (0)
#define SHM_HASH_SLOT_OFFSET(len, max, size) (sizeof(shm_hash_head_t))
#define SHM_HASH_NODEQ_OFFSET(len, max, size)   /* 链表结点队列 */\
    (SHM_HASH_SLOT_OFFSET(len, max, size) + (len) * sizeof(shm_hash_slot_t))
#define SHM_HASH_DATAQ_OFFSET(len, max, size)   /* 数据结点队列 */\
    (SHM_HASH_NODEQ_OFFSET(len, max, size) + shm_ring_total(max))
#define SHM_HASH_NODE_OFFSET(len, max, size) \
    (SHM_HASH_DATAQ_OFFSET(len, max, size) + shm_ring_total(max))
#define SHM_HASH_DATA_OFFSET(len, max, size) \
    (SHM_HASH_NODE_OFFSET(len, max, size) + (max) * sizeof(shm_list_node_t))

/* 静态函数 */
static shm_hash_t *shm_hash_init(void *addr, int len, int max, size_t size);

/******************************************************************************
 **函数名称: shm_hash_creat
 **功    能: 创建哈希表
 **输入参数:
 **     path: 路径
 **     len: 哈希表长
 **     max: 最大单元数
 **     size: 单元大小
 **输出参数: NONE
 **返    回: 哈希表
 **实现描述:
 **   ---------- ------------ --------- ---------- ---------- -----------------
 **  |          |            |         |          |          |                 |                    |
 **  | 头部信息 | 哈  希  槽 | 结点队列| 数据队列 | 结点单元 |   数 据 单 元   |
 **  |          |            |         |          |          |                 |
 **   ---------- ------------ --------- ---------- ---------- -----------------
 **  ^          ^            ^         ^          ^          ^
 **  |          |            |         |          |          |
 ** addr       slot        node_pool     data_pool       node       data
 **注意事项: 创建共享内存, 并进行相关资源进行初始化.
 **作    者: # Qifeng.zou # 2015.07.26 01:00:00 #
 ******************************************************************************/
shm_hash_t *shm_hash_creat(const char *path, int len, int max, size_t size)
{
    void *addr;
    size_t total;

    total = SHM_HASH_TOTAL_SIZE(len, max, size);

    /* > 创建共享内存 */
    addr = shm_creat(path, total);
    if (NULL == addr) {
        return NULL;
    }

    /* > 初始化哈希表 */
    return shm_hash_init(addr, len, max, size);
}

/*****************************************************************************
 **函数名称: shm_hash_init
 **功    能: 初始化哈希表
 **输入参数:
 **     addr: 起始地址
 **     len: 哈希表长
 **     max: 最大单元数
 **     size: 单元大小
 **输出参数: NONE
 **返    回: 哈希表
 **实现描述:
 **注意事项: 初始化哈希表对象的相关资源信息
 **作    者: # Qifeng.zou # 2015.07.26 #
 ******************************************************************************/
static shm_hash_t *shm_hash_init(void *addr, int len, int max, size_t size)
{
    int idx;
    off_t off;
    shm_hash_t *sh;
    shm_ring_t *ring;
    shm_hash_head_t *head;
    shm_hash_slot_t *slot;

    /* > 创建对象 */
    sh = (shm_hash_t *)calloc(1, sizeof(shm_hash_t));
    if (NULL == sh) {
        return NULL;
    }

    sh->addr = addr;
    sh->head = (shm_hash_head_t *)(addr + SHM_HASH_HEAD_OFFSET(len, max, size));
    sh->slot = (shm_hash_slot_t *)(addr + SHM_HASH_SLOT_OFFSET(len, max, size));
    sh->node_pool = (shm_ring_t *)(addr + SHM_HASH_NODEQ_OFFSET(len, max, size));
    sh->data_pool = (shm_ring_t *)(addr + SHM_HASH_DATAQ_OFFSET(len, max, size));

    /* > 初始化头部信息 */
    head = sh->head;
    head->len = len;
    head->max = max;
    head->size = size;
    head->slot_off = SHM_HASH_SLOT_OFFSET(len, max, size);
    head->node_off = SHM_HASH_NODEQ_OFFSET(len, max, size);
    head->data_off = SHM_HASH_DATAQ_OFFSET(len, max, size);

    /* > 链表结点队列 */
    ring = shm_ring_init((void *)sh->node_pool, max);
    if (NULL == ring) {
        return NULL;
    }

    off = SHM_HASH_NODE_OFFSET(len, max, size);
    for (idx=0; idx<max; ++idx, off+=sizeof(shm_list_node_t)) {
        if (shm_ring_push(sh->node_pool, off)) {
            return NULL;
        }
    }

    /* > 数据结点队列 */
    ring = shm_ring_init((void *)sh->data_pool, max);
    if (NULL == ring) {
        return NULL;
    }

    off = SHM_HASH_DATA_OFFSET(len, max, size);
    for (idx=0; idx<max; ++idx, off+=size) {
        if (shm_ring_push(sh->data_pool, off)) {
            return NULL;
        }
    }

    /* > 哈希槽数组 */
    slot = sh->slot;
    for (idx=0; idx<len; ++idx, ++slot) {
        memset(slot, 0, sizeof(shm_hash_slot_t));
    }

    return sh;
}

/******************************************************************************
 **函数名称: shm_hash_alloc
 **功    能: 申请数据单元空间
 **输入参数:
 **     sh: 哈希表对象
 **输出参数: NONE
 **返    回: 数据单元地址
 **实现描述: 从数据节点池中弹出数据结点, 并根据结点偏移计算内存地址
 **注意事项:
 **作    者: # Qifeng.zou # 2015.07.26 14:26:17 #
 ******************************************************************************/
void *shm_hash_alloc(shm_hash_t *sh)
{
    off_t off;

    off = shm_ring_pop(sh->data_pool);
    if (-1 == off) {
        return NULL;
    }

    return (void *)(sh->addr + off);
}

/******************************************************************************
 **函数名称: shm_hash_dealloc
 **功    能: 回收数据单元空间
 **输入参数:
 **     sh: 哈希表对象
 **     addr: 需要被回收的数据单元地址
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.07.26 14:32:24 #
 ******************************************************************************/
void shm_hash_dealloc(shm_hash_t *sh, void *addr)
{
    off_t off;

    off = (off_t)(sh->addr - addr);

    shm_ring_push(sh->data_pool, off);

    return;
}

/******************************************************************************
 **函数名称: shm_hash_push
 **功    能: 回收数据单元空间
 **输入参数:
 **     sh: 哈希表对象
 **     key: 主键
 **     len: 长度
 **     addr: 需要被回收的数据单元地址
 **输出参数: NONE
 **返    回: 数据单元地址
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.07.26 14:32:24 #
 ******************************************************************************/
int shm_hash_push(shm_hash_t *sh, void *key, int len, void *data)
{
    uint64_t idx;
    shm_list_node_t *node;
    off_t data_off, node_off;

    idx = hash_time33_ex(key, len) % sh->head->len;
    data_off = (off_t)(data - sh->addr);

    /* > 申请链表结点 */
    node_off = shm_ring_pop(sh->node_pool);
    if (-1 == node_off) {
        return -1;
    }

    node = (shm_list_node_t *)(sh->addr + node_off);
    node->data = data_off;

    /* > 插入链表头 */
    if (shm_list_lpush(sh->addr, &sh->slot[idx].list, node_off)) {
        shm_ring_push(sh->node_pool, node_off);
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: shm_hash_pop
 **功    能: 弹出
 **输入参数:
 **     sh: 哈希表对象
 **     key: 主键
 **     len: 主键长
 **输出参数: NONE
 **返    回: 数据单元地址
 **实现描述:
 **注意事项: 需要回收链表结点的空间
 **作    者: # Qifeng.zou # 2015.07.26 14:32:24 #
 ******************************************************************************/
void *shm_hash_pop(shm_hash_t *sh, void *key, int len, cmp_cb_t cmp_cb)
{
    void *data;
    off_t off;
    uint64_t idx;
    shm_list_node_t *node;

    idx = hash_time33_ex(key, len) % sh->head->len;

    off = shm_list_query_and_delete(sh->addr, &sh->slot[idx].list, key, cmp_cb, sh->addr);
    if (0 == off) {
        return NULL;
    }

    node = (shm_list_node_t *)(sh->addr + off);
    data = (void *)(sh->addr + node->data);

    shm_ring_push(sh->node_pool, off);

    return data;
}
