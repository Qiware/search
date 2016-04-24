/******************************************************************************
 ** Copyright(C) 2013-2014 Qiware technology Co., Ltd
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

/******************************************************************************
 **函数名称: shm_queue_total
 **功    能: 计算队列总空间
 **输入参数:
 **     max: 队列单元数
 **     size: 队列单元SIZE
 **输出参数: NONE
 **返    回: SHMQ队列需要的总空间大小
 **实现描述: 接合共享内存环形队列和内存池实现
 **      -------------- --------------
 **     |              |              |
 **     |     ring     |     slot     |
 **     |  (环形队列)  |   (内存池)   |
 **      -------------- --------------
 **     环形队列: 负责数据的PUSH和POP等操作
 **     内存池  : 负责内存空间的申请和回收等操作
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.06 #
 ******************************************************************************/
size_t shm_queue_total(int max, size_t size)
{
    size_t sum = 0, sz;

    sz = shm_ring_total(max);
    if ((size_t)-1 == sz) {
        return (size_t)-1;
    }

    sum += sz;

    sz = shm_slot_total(max, size);
    if ((size_t)-1 == sz) {
        return (size_t)-1;
    }

    sum += sz;

    return sum;
}

/******************************************************************************
 **函数名称: shm_queue_creat
 **功    能: 创建共享内存队列
 **输入参数:
 **     path: KEY值路径
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
shm_queue_t *shm_queue_creat(const char *path, int max, int size)
{
    void *addr;
    size_t total;
    shm_queue_t *shmq;

    /* > 计算内存空间 */
    total = shm_queue_total(max, size);
    if ((size_t)-1 == total) {
        return NULL;
    }

    /* > 新建队列对象 */
    shmq = (shm_queue_t *)calloc(1, sizeof(shm_queue_t));
    if (NULL == shmq) {
        return NULL;
    }

    /* > 创建共享内存 */
    addr = shm_creat(path, total);
    if (NULL == addr) {
        free(shmq);
        return NULL;
    }

    /* > 初始化环形队列 */
    shmq->ring = shm_ring_init(addr, max);
    if (NULL == shmq->ring) {
        free(shmq);
        return NULL;
    }

    /* > 初始化内存池 */
    shmq->slot = shm_slot_init(addr + shm_ring_total(max), max, size);
    if (NULL == shmq->slot) {
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
shm_queue_t *shm_queue_attach(const char *path)
{
    void *addr;
    shm_queue_t *shmq;

    /* > 新建队列对象 */
    shmq = (shm_queue_t *)calloc(1, sizeof(shm_queue_t));
    if (NULL == shmq) {
        return NULL;
    }

    /* > 附着共享内存 */
    addr = (void *)shm_attach(path, 0);
    if (NULL == addr) {
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
 **函数名称: shm_queue_mpush
 **功    能: 压入多条数据
 **输入参数:
 **     shmq: 共享内存队列
 **     p: 内存地址数组
 **     num: 数组长度
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 1. 内存地址p必须是从shm_queue_malloc()分配的，否则会出严重错误！
 **          2. 一次最大压入数据不能超过SHMQ_MPUSH_MAX_NUM个数
 **作    者: # Qifeng.zou # 2014.06.08 10:58:11 #
 ******************************************************************************/
int shm_queue_mpush(shm_queue_t *shmq, void **p, int num)
{
    int idx;
    off_t off[SHMQ_MPUSH_MAX_NUM];

    memset(off, 0, sizeof(off));

    if (NULL == p) { return -1; }
    if (num > SHMQ_MPUSH_MAX_NUM) {
        return -1;
    }

    for (idx=0; idx<num; ++idx) {
        off[idx] = (off_t)(p[idx] - (void *)shmq->ring);
    }

    return shm_ring_mpush(shmq->ring, off, num);
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
    if ((off_t)-1 == off) {
        return NULL;
    }

    return (void *)shmq->ring + off;
}

/******************************************************************************
 **函数名称: shm_queue_mpop
 **功    能: 弹出多个数据
 **输入参数:
 **     shmq: 共享内存队列
 **输出参数: NONE
 **返    回: 实际数据条数
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.08 11:07:45 #
 ******************************************************************************/
int shm_queue_mpop(shm_queue_t *shmq, void **p, int num)
{
    int idx;

    num = shm_ring_mpop(shmq->ring, (off_t *)p, num);
    for (idx=0; idx<num; ++idx) {
        p[idx] = (void *)((void *)shmq->ring + (off_t)p[idx]);
    }

    return num;
}
