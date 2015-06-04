/******************************************************************************
 ** Coypright(C) 2013-2014 Xundao technology Co., Ltd
 **
 ** 文件名: shm_queue.c
 ** 版本号: 1.0
 ** 描  述: 共享内存版队列
 **         使用SHM_SLAB机制管理共享内存.
 ** 作  者: # Qifeng.zou # 2015.05.06 #
 ******************************************************************************/
#include "shm_opt.h"
#include "shm_ring.h"
#include "shm_slot.h"
#include "shm_queue.h"

/* 计算队列总空间 */
size_t shm_queue_total(int max, size_t size)
{
    size_t total, sz;

    sz = shm_slot_total(max, size);
    if ((size_t)-1 == sz)
    {
        return (size_t)-1;
    }

    total = sz;

    sz = shm_ring_total(max);
    if ((size_t)-1 == sz)
    {
        return (size_t)-1;
    }

    return total;
}

/******************************************************************************
 **函数名称: shm_queue_creat
 **功    能: 创建共享内存队列
 **输入参数:
 **     path: KEY路径
 **     max: 队列单元数
 **     size: 队列单元SIZE
 **输出参数: NONE
 **返    回: 共享内存队列
 **实现描述: 通过路径生成KEY，再根据KEY创建共享内存队列
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.04 #
 ******************************************************************************/
shm_queue_t *shm_queue_creat(const char *path, int max, int size)
{
    key_t key;

    key = shm_ftok(path, 0);
    if ((key_t)-1 == key)
    {
        return NULL;
    }

    /* > 通过KEY创建共享内存队列 */
    return shm_queue_creat_ex(key, max, size);
}

/******************************************************************************
 **函数名称: shm_queue_creat_ex
 **功    能: 创建共享内存队列
 **输入参数:
 **     key: 共享内存KEY
 **     max: 队列单元数
 **     size: 队列单元SIZE
 **输出参数: NONE
 **返    回: 共享内存队列
 **实现描述: 接合共享内存环形队列和内存池实现
 **      -------------- --------------
 **     |              |              |
 **     |     ring     |     slot     |
 **     |  (环形队列)  |   (内存池)   |
 **      -------------- --------------
 **     环形队列: 负责数据的弹出和插入等操作
 **     内存池  : 负责内存空间的申请和回收等操作
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.06 #
 ******************************************************************************/
shm_queue_t *shm_queue_creat_ex(int key, int max, int size)
{
    void *addr;
    size_t total;
    shm_queue_t *shmq;

    /* > 计算内存空间 */
    total = shm_queue_total(max, size);
    if ((size_t)-1 == total)
    {
        return NULL;
    }

    /* > 新建队列对象 */
    shmq = (shm_queue_t *)calloc(1, sizeof(shm_queue_t));
    if (NULL == shmq)
    {
        return NULL;
    }

    /* > 创建共享内存 */
    addr = shm_creat(key, total);
    if (NULL == addr)
    {
        free(shmq);
        return NULL;
    }

    /* > 初始化环形队列 */
    shmq->ring = shm_ring_init(addr, max);
    if (NULL == shmq->ring)
    {
        free(shmq);
        return NULL;
    }

    /* > 初始化内存池 */
    shmq->slot = shm_slot_init(addr + shm_ring_total(max), max, size);
    if (NULL == shmq->slot)
    {
        free(shmq);
        return NULL;
    }

    return shmq;
}

/******************************************************************************
 **函数名称: shm_queue_attach
 **功    能: 附着共享内存队列
 **输入参数:
 **     path: KEY值路径
 **输出参数:
 **返    回: 共享内存队列
 **实现描述: 
 **     1. 计算内存空间
 **     2. 创建共享内存
 **     3. 初始化标志量
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.25 #
 ******************************************************************************/
shm_queue_t *shm_queue_attach(const char *path)
{
    key_t key;

    /* > 通过路径生成KEY */
    key = shm_ftok(path, 0);
    if ((key_t)-1 == key)
    {
        return NULL;
    }

    /* > 附着共享内存 */
    return shm_queue_attach_ex(key);
}

/******************************************************************************
 **函数名称: shm_queue_attach_ex
 **功    能: 附着共享内存队列
 **输入参数:
 **     key: 共享内存KEY
 **     max: 队列单元数
 **     size: 队列单元SIZE
 **输出参数:
 **返    回: 共享内存队列
 **实现描述: 
 **     1. 计算内存空间
 **     2. 创建共享内存
 **     3. 初始化标志量
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.25 #
 ******************************************************************************/
shm_queue_t *shm_queue_attach_ex(int key)
{
    void *addr;
    shm_queue_t *shmq;

    /* > 新建队列对象 */
    shmq = (shm_queue_t *)calloc(1, sizeof(shm_queue_t));
    if (NULL == shmq)
    {
        return NULL;
    }

    /* > 附着共享内存 */
    addr = (void *)shm_attach(key);
    if (NULL == addr)
    {
        free(shmq);
        return NULL;
    }

    /* > 设置指针对象 */
    shmq->ring = (shm_ring_t *)addr;
    shmq->slot = (shm_slot_t *)(addr + shm_ring_total(shmq->ring->max));

    return shmq;
}

/******************************************************************************
 **函数名称: shm_queue_push
 **功    能: 压入队列
 **输入参数:
 **     shmq: 共享内存队列
 **     p: 内存地址
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 内存地址p必须是从shm_queue_malloc()分配的，否则会出严重错误！
 **作    者: # Qifeng.zou # 2014.05.06 #
 ******************************************************************************/
int shm_queue_push(shm_queue_t *shmq, void *p)
{
    if (NULL == p) { return -1; }

    return shm_ring_push(shmq->ring, p - (void *)shmq->ring);
}

/******************************************************************************
 **函数名称: shm_queue_pop
 **功    能: 出队列
 **输入参数:
 **     shmq: 共享内存队列
 **输出参数: NONE
 **返    回: 数据地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.06 #
 ******************************************************************************/
void *shm_queue_pop(shm_queue_t *shmq)
{
    off_t off;

    off = shm_ring_pop(shmq->ring);
    if ((off_t)-1 == off)
    {
        return NULL;
    }

    return (void *)shmq->ring + off;
}
