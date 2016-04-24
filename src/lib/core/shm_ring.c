/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: shm_ring.c
 ** 版本号: 1.0
 ** 描  述: 共享内存版环形队列
 ** 作  者: # Qifeng.zou # Tue 05 May 2015 11:40:10 PM CST #
 ******************************************************************************/
#include "atomic.h"
#include "shm_ring.h"

/******************************************************************************
 **函数名称: shm_ring_total
 **功    能: 获取环形队列对象需要的空间
 **输入参数:
 **     max: 队列容量
 **输出参数:
 **返    回: 占用空间大小
 **实现描述:
 **注意事项: max必须为2的n次方
 **作    者: # Qifeng.zou # 2014.05.05 #
 ******************************************************************************/
size_t shm_ring_total(int max)
{
    /* > max必须为2的n次方 */
    if (!ISPOWEROF2(max)) {
        return (size_t)-1;
    }

    return sizeof(shm_ring_t) + max * sizeof(off_t);
}

/******************************************************************************
 **函数名称: shm_ring_init
 **功    能: 环形队列初始化
 **输入参数:
 **     rq: 环形队列
 **     max: 队列容量
 **输出参数:
 **返    回: 占用空间大小
 **实现描述:
 **注意事项: max必须为2的n次方
 **作    者: # Qifeng.zou # 2014.05.05 #
 ******************************************************************************/
shm_ring_t *shm_ring_init(void *addr, int max)
{
    shm_ring_t *rq = (shm_ring_t *)addr;

    /* > max必须为2的n次方 */
    if (!ISPOWEROF2(max)) {
        return NULL;
    }

    /* > 设置相关标志 */
    rq->max = max;
    rq->num = 0;
    rq->mask = max - 1;
    rq->prod.head = rq->prod.tail = 0;
    rq->cons.head = rq->cons.tail = 0;

    return rq;
}

/******************************************************************************
 **函数名称: shm_ring_push
 **功    能: 放入队列
 **输入参数:
 **     rq: 环形队列
 **     off: 偏移量
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.05.06 #
 ******************************************************************************/
int shm_ring_push(shm_ring_t *rq, off_t off)
{
    return shm_ring_mpush(rq, &off, 1);
}

/******************************************************************************
 **函数名称: shm_ring_pop
 **功    能: 弹出队列
 **输入参数:
 **     rq: 队列
 **输出参数:
 **返    回: 偏移量
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.05.06 #
 ******************************************************************************/
off_t shm_ring_pop(shm_ring_t *rq)
{
    off_t off[1];

    if (0 == shm_ring_mpop(rq, off, 1)) {
        return (off_t)-1;
    }

    return off[0];
}

/******************************************************************************
 **函数名称: shm_ring_mpush
 **功    能: 同时插入多个数据
 **输入参数:
 **     rq: 环形队列
 **     addr: 数据地址数组
 **     num: 数组长度
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: addr指向的地址必须为堆地址
 **作    者: # Qifeng.zou # 2015.05.05 #
 ******************************************************************************/
int shm_ring_mpush(shm_ring_t *rq, off_t *off, unsigned int num)
{
    int succ;
    unsigned int prod_head, prod_next, cons_tail, i;

    /* > 申请队列空间 */
    do {
        prod_head = rq->prod.head;
        cons_tail = rq->cons.tail;
        if (num > (rq->max + cons_tail - prod_head)) {
            return -1; /* 空间不足 */
        }

        prod_next = prod_head + num;

        succ = atomic32_cmp_and_set(&rq->prod.head, prod_head, prod_next);
    } while (0 == succ);

    /* > 放入队列空间 */
    for (i=0; i<num; ++i) {
        rq->off[(prod_head+i) & rq->mask] = off[i];
    }
    while (rq->prod.tail != prod_head) { NULL; }
    rq->prod.tail = prod_next;

    atomic32_add(&rq->num, num); /* 计数 */

    return 0;
}

/******************************************************************************
 **函数名称: shm_ring_mpop
 **功    能: 同时弹出多个数据
 **输入参数:
 **     rq: 队列
 **     num: 弹出n个地址
 **输出参数:
 **     off: 偏移数组
 **返    回: 实际数据条数
 **实现描述: 无锁编程
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.05 #
 ******************************************************************************/
int shm_ring_mpop(shm_ring_t *rq, off_t *off, unsigned int num)
{
    int succ;
    unsigned int cons_head, cons_next, prod_tail, i;

    /* > 申请队列空间 */
    do {
        cons_head = rq->cons.head; /* 消费者头 */
        prod_tail = rq->prod.tail; /* 生产者尾 */

        if (num > prod_tail - cons_head) {
            return 0; /* 无数据 */
        }

        cons_next = cons_head + num;

        succ = atomic32_cmp_and_set(&rq->cons.head, cons_head, cons_next);
    } while(0 == succ);

    /* > 地址弹出队列 */
    for (i=0; i<num; ++i) {
        off[i] = (off_t)rq->off[(cons_head+i) & rq->mask];
    }

    /* > 判断是否其他线程也在进行处理, 是的话, 则等待对方完成处理 */
    while (rq->cons.tail != cons_head) { NULL; }

    rq->cons.tail = cons_next;

    atomic32_sub(&rq->num, num); /* 计数 */

    return num;
}

/******************************************************************************
 **函数名称: shm_ring_print
 **功    能: 打印环形队列
 **输入参数:
 **     rq: 环形队列
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 依次打印环形队列的指针值
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.05 #
 ******************************************************************************/
void shm_ring_print(shm_ring_t *rq)
{
    unsigned int i;

    for (i=0; i<rq->num; ++i) {
        fprintf(stderr, "off[%d]: %lu", rq->prod.head+i, rq->off[(rq->prod.head+i)&rq->mask]);
    }
}
