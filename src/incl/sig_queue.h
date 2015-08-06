#if !defined(__SIG_QUEUE_H__)
#define __SIG_QUEUE_H__

#include "comm.h"
#include "queue.h"

typedef struct
{
    pthread_mutex_t lock;           /* 队列锁 */
    pthread_cond_t ready;           /* 临界变量 */
    queue_t *queue;                 /* 队列(无锁队列) */
} sig_queue_t;

sig_queue_t *sig_queue_creat(int max, int size);
#define sig_queue_malloc(sq, size) queue_malloc((sq)->queue, size)
#define sig_queue_dealloc(sq, p) queue_dealloc((sq)->queue, p)
int sig_queue_push(sig_queue_t *sq, void *addr);
void *sig_queue_pop(sig_queue_t *sq);

#define sig_queue_used(q) queue_used((q)->queue)    /* 获取队列数据个数 */
#define sig_queue_size(q) queue_size((q)->queue)    /* 获取队列单元大小 */
#define sig_queue_print(sq) queue_print((sq)->queue)/* 打印队列使用情况 */
void sig_queue_destroy(sig_queue_t *sq);

#endif /*__SIG_QUEUE_H__*/
