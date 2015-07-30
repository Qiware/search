/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: shm_hash.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # Sat 25 Jul 2015 10:54:37 AM CST #
 ******************************************************************************/


#define SHM_HASH_TOTAL_SIZE(max, size) /* 计算哈希空间 */\
    (sizeof(shm_hash_head_t) + 2 * shm_ring_total(max) \
     + (max) * (sizeof(shm_hash_slot_t) + sizeof(shm_list_node_t) + (size)))

/* 获取各段偏移 */
#define SHM_HASH_HEAD_OFFSET(len, max, size) (0)
#define SHM_HASH_SLOT_OFFSET(len, max, size) (sizeof(shm_hash_head_t))
#define SHM_HASH_NODEQ_OFFSET(len, max, size)  \
    (SHM_HASH_SLOT_OFFSET(len, max, size) + (len) * sizeof(shm_hash_slot_t))
#define SHM_HASH_DATAQ_OFFSET(len, max, size) \
    (SHM_HASH_NODEQ_OFFSET(len, max, size) + shm_ring_total(max))
#define SHM_HASH_NODE_OFFSET(len, max, size) \
    (SHM_HASH_DATAQ_OFFSET(len, max, size) + shm_ring_total(max))
#define SHM_HASH_DATA_OFFSET(len, max, size) \
    (SHM_HASH_NODE_OFFSET(len, max, size) + (max) * sizeof(shm_hash_node_t))

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
 ** addr       slot        nodeq     dataq       node       data
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
    if (NULL == addr)
    {
        return NULL;
    }

    /* > 初始化哈希表 */
    return shm_hash_init(addr, len, max, size);
}

/******************************************************************************
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
    off_t off;
    shm_hash_t *sh;
    shm_ring_t *ring;
    shm_hash_head_t *head;
    shm_hash_slot_t *slot;

    /* > 创建对象 */
    sh = (shm_hash_t *)calloc(1, sizeof(shm_hash_t));
    if (NULL == sh)
    {
        return NULL;
    }

    sh->addr = addr;
    sh->head = (shm_hash_head_t *)(addr + SHM_HASH_HEAD_OFFSET(len, max, size));
    sh->slot = (shm_hash_slot_t *)(addr + SHM_HASH_SLOT_OFFSET(len, max, size));
    sh->nodeq = (shm_ring_t *)(addr + SHM_HASH_NODEQ_OFFSET(len, max, size));
    sh->dataq = (shm_ring_t *)(addr + SHM_HASH_DATAQ_OFFSET(len, max, size));

    /* > 初始化头部信息 */
    head = sh->head;
    head->len = len;
    head->max = max;
    head->size = size;
    head->slot_off = SHM_HASH_SLOT_OFFSET(len, max, size);
    head->node_off = SHM_HASH_NODEQ_OFFSET(len, max, size);
    head->data_off = SHM_HASH_DATAQ_OFFSET(len, max, size);

    /* > 链表结点队列 */
    ring = shm_ring_init((void *)sh->nodeq, max);
    if (NULL == ring)
    {
        return -1;
    }

    off = SHM_HASH_NODE_OFFSET(len, max, size);
    for (idx=0; idx<max; ++idx, off+=sizeof(shm_list_node_t))
    {
        if (shm_ring_push(sh->nodeq, off))
        {
            return -1;
        }
    }

    /* > 数据结点队列 */
    ring = shm_ring_init((void *)sh->dataq, max);
    if (NULL == ring)
    {
        return -1;
    }

    off = SHM_HASH_DATA_OFFSET(len, max, size);
    for (idx=0; idx<max; ++idx, off+=size)
    {
        if (shm_ring_push(sh->dataq, off))
        {
            return -1;
        }
    }

    /* > 哈希槽数组 */
    slot = sh->slot;
    for (idx=0; idx<len; ++idx, ++slot)
    {
        memset(slot, 0, sizeof(shm_hash_slot_t));
    }

    return 0;
}

/******************************************************************************
 **函数名称: shm_hash_alloc
 **功    能: 申请数据单元空间
 **输入参数: 
 **     sh: 哈希表对象
 **输出参数: NONE
 **返    回: 数据单元地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.07.26 14:26:17 #
 ******************************************************************************/
void *shm_hash_alloc(shm_hash_t *sh)
{
    off_t off;
    void *addr;

    off = shm_ring_pop(sh->dataq);
    if (-1 == off)
    {
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

    shm_ring_push(sh->dataq, off);

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
    shm_list_node_t *node, *item;
    off_t data_off, node_off, off, next;

    idx = hash_time33_ex(key, len) % sh->len;
    data_off = (off_t)(data - sh->addr);

    /* > 申请链表结点 */
    node_off = shm_ring_pop(sh->nodeq);
    if (-1 == node_off)
    {
        return -1;
    }

    /* > 插入链表头 */
    node = (shm_list_node_t *)(sh->addr + node_off);
    node->data = data_off;
    node->next = sh->slot[idx].list.head;
    sh->slot[idx].list.head = node_off;

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
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.07.26 14:32:24 #
 ******************************************************************************/
void *shm_hash_pop(shm_hash_t *sh, void *key, int len)
{
    off_t off, data;
    uint64_t idx;
    shm_list_node_t *node, *prev = NULL;

    idx = hash_time33_ex(key, len) % sh->len;
    off = sh->slot[idx].list.head;
    while (0 != off)
    {
        node = (shm_list_node_t *)(sh->addr + off);
        if (!cmp_cb(key, sh->addr + node->data_off))
        {
            data = node->data_off;
            shm_ring_push(sh->nodeq, off);

            return (void *)(sh->addr + data);
        }
    }

    return NULL;
}
