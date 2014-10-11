#if !defined(__QUEUE_H__)
#define __QUEUE_H__

/* 队列配置 */
typedef struct
{
    size_t count;   /* 单元数 */
} queue_conf_t;

/* 循环队列结点 */
typedef struct _Qnode_t
{
    struct _Qnode_t *next;
    void *data;
} Qnode_t;

/* 循环队列 */
typedef struct
{
    int max;        /* 队列容量 */
    int num;        /* 队列成员个数 */

    Qnode_t *base;  /* 队列基址 */
    Qnode_t *head;  /* 队列头 */
    Qnode_t *tail;  /* 队列尾 */
} Queue_t;

int queue_init(Queue_t *q, int max);
int queue_push(Queue_t *q, void *addr);
void* queue_pop(Queue_t *q);
void queue_destroy(Queue_t *q);
#endif /*__QUEUE_H__*/
