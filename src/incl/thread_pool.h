#if !defined(__THREAD_POOL_H__)
#define __THREAD_POOL_H__

#include <pthread.h>
#if defined(__THREAD_POOL_SLAB__)
#include "slab.h"
#endif /*__THREAD_POOL_SLAB__*/

/* 线程工作内容 */
typedef struct _thread_worker_t
{
    void *(*process)(void *arg);    /* 线程调用接口 */
    void *arg;                      /* 接口参数 */
    struct _thread_worker_t *next;  /* 下一个节点 */
} thread_worker_t;

/* 线程池 */
typedef struct
{
    pthread_mutex_t queue_lock;     /* 队列互斥锁 */
    pthread_cond_t queue_ready;     /* 队列临界锁 */

    thread_worker_t *head;          /* 队列头 */
    int shutdown;                   /* 是否已销毁线程 */
    pthread_t *tid;                 /* 线程ID数组 —动态分配空间 */
    int num;                        /* 实际创建的线程个数 */
    int queue_size;                 /* 工作队列当前大小 */

    void *data;                     /* 附加数据 */
#if defined(__THREAD_POOL_SLAB__)
    eslab_pool_t eslab;             /* SLAB内存管理机制 */
#endif /*__THREAD_POOL_SLAB__*/
} thread_pool_t;


thread_pool_t *thread_pool_init(int num);
int thread_pool_add_worker(thread_pool_t *tp, void *(*process)(void *arg), void *arg);
int thread_pool_keepalive(thread_pool_t *tp);
int thread_pool_keepalive_ext(thread_pool_t *tp, void *(*process)(void *arg), void *arg);
int thread_pool_get_tidx(thread_pool_t *tp);
int thread_pool_destroy(thread_pool_t *tp);
int thread_pool_destroy_ext(
        thread_pool_t *tp,
        void (*args_destroy)(void *cntx, void *args),
        void *cntx); 
#endif /*__THREAD_POOL_H__*/
