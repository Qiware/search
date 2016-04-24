/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: ring.c
 ** 版本号: 1.0
 ** 描  述: 环形无锁队列
 **     1. 先进先出队列
 **     2. 环形无锁队列
 ** 作  者: # Qifeng.zou # 2014.05.04 #
 ******************************************************************************/

#include "ring.h"
#include "atomic.h"

/******************************************************************************
 **函数名称: ring_creat
 **功    能: 环形队列初始化
 **输入参数:
 **     max: 队列容量
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: max必须为2的n次方
 **作    者: # Qifeng.zou # 2014.05.04 #
 ******************************************************************************/
ring_t *ring_creat(int max)
{
    ring_t *rq;

    /* > max必须为2的n次方 */
    if (!ISPOWEROF2(max)) { return NULL; }

    /* > 申请内存空间 */
    rq = (ring_t *)calloc(1, sizeof(ring_t));
    if (NULL == rq) {
        return NULL;
    }

    rq->data = (void **)calloc(max, sizeof(void *));
    if (NULL == rq->data) {
        free(rq);
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
 **函数名称: ring_push
 **功    能: 入队列
 **输入参数:
 **     rq: 环形队列
 **     addr: 数据地址
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: addr指向的地址必须为堆地址
 **作    者: # Qifeng.zou # 2014.05.04 #
 ******************************************************************************/
int ring_push(ring_t *rq, void *addr)
{
    return ring_mpush(rq, &addr, 1);
}

/******************************************************************************
 **函数名称: ring_pop
 **功    能: 弹出队列
 **输入参数:
 **     rq: 队列
 **输出参数:
 **返    回: 数据地址
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.05.04 #
 ******************************************************************************/
void *ring_pop(ring_t *rq)
{
    void *addr[1];

    if (0 == ring_mpop(rq, addr, 1)) {
        return NULL;
    }

    return addr[0];
}

/******************************************************************************
 **函数名称: ring_mpush
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
int ring_mpush(ring_t *rq, void **addr, unsigned int num)
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
        rq->data[(prod_head+i) & rq->mask] = addr[i];
    }
    while (rq->prod.tail != prod_head) { NULL; }
    rq->prod.tail = prod_next;

    atomic32_add(&rq->num, num); /* 计数 */

    return 0;
}

/******************************************************************************
 **函数名称: ring_mpop
 **功    能: 同时弹出多个数据
 **输入参数:
 **     rq: 队列
 **     num: 弹出n个地址
 **输出参数:
 **     addr: 指针数组
 **返    回: 数据实际条数
 **实现描述: 无锁编程
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.05 #
 ******************************************************************************/
int ring_mpop(ring_t *rq, void **addr, unsigned int num)
{
    int succ;
    unsigned int cons_head, cons_next, prod_tail, i;

    /* > 申请队列空间 */
    do {
        cons_head = rq->cons.head;
        prod_tail = rq->prod.tail;

        if (num > prod_tail - cons_head) {
            return 0; /* 无数据 */
        }

        cons_next = cons_head + num;

        succ = atomic32_cmp_and_set(&rq->cons.head, cons_head, cons_next);
    } while(0 == succ);

    /* > 地址弹出队列 */
    for (i=0; i<num; ++i) {
        addr[i] = (void *)rq->data[(cons_head+i) & rq->mask];
    }

    /* > 判断是否其他线程也在进行处理, 是的话, 则等待对方完成处理 */
    while (rq->cons.tail != cons_head) { NULL; }

    rq->cons.tail = cons_next;

    atomic32_sub(&rq->num, num); /* 计数 */

    return num;
}

/******************************************************************************
 **函数名称: ring_destroy
 **功    能: 销毁环形队列
 **输入参数:
 **     rq: 环形队列
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 释放环形队列空间
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.04 #
 ******************************************************************************/
void ring_destroy(ring_t *rq)
{
    free(rq->data);
    rq->max = 0;
    rq->num = 0;
}

/******************************************************************************
 **函数名称: ring_print
 **功    能: 打印环形队列
 **输入参数:
 **     rq: 环形队列
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 依次打印环形队列的指针值
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.05 #
 ******************************************************************************/
void ring_print(ring_t *rq)
{
    unsigned int i;

    for (i=0; i<rq->num; ++i) {
        fprintf(stderr, "ptr[%d]: %p", rq->prod.head+i, rq->data[(rq->prod.head+i)&rq->mask]);
    }
}
