#if !defined(__QUEUE_H__)
#define __QUEUE_H__

/* 循环队列结点 */
typedef struct _Qnode_t
{
    struct _Qnode_t *next;
    void *args;
}Qnode_t;

/* 循环队列 */
typedef struct
{
    int32_t max;        /* 队列容量 */
    int32_t num;        /* 队列成员个数 */
    int32_t size;       /* 队列各节点的最大存储空间 */

    Qnode_t *base;  /* 队列基址 */
    Qnode_t *head;  /* 队列头 */
    Qnode_t *tail;  /* 队列尾 */
}Queue_t;

extern int32_t queue_init(Queue_t *q, int32_t max, int32_t size);
extern int32_t queue_push(Queue_t *q, void *addr, int32_t size);
extern void* queue_pop(Queue_t *q);
extern void queue_destroy(Queue_t *q);
#endif /*__QUEUE_H__*/
