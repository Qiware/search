#if !defined(__QUEUE_H__)
#define __QUEUE_H__

/* 循环队列结点 */
typedef struct _Qnode_t
{
    struct _Qnode_t *next;
    void *args;
} Qnode_t;

/* 循环队列 */
typedef struct
{
    int max;        /* 队列容量 */
    int num;        /* 队列成员个数 */
    int size;       /* 队列各节点的最大存储空间 */

    Qnode_t *base;  /* 队列基址 */
    Qnode_t *head;  /* 队列头 */
    Qnode_t *tail;  /* 队列尾 */
} Queue_t;

extern int queue_init(Queue_t *q, int max, int size);
extern int queue_push(Queue_t *q, void *addr, int size);
extern void* queue_pop(Queue_t *q);
extern void queue_destroy(Queue_t *q);
#endif /*__QUEUE_H__*/
