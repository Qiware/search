#if !defined(__SHM_QUEUE_H__)
#define __SHM_QUEUE_H__

#include "spinlock.h"
#include "shm_pool.h"

/* 结点当前状态 */
#define SHMQ_NODE_STAT_IDLE     (0)  /* 未被占用 */
#define SHMQ_NODE_STAT_USED     (1)  /* 已被占用 */

/* 队列结点 */
typedef struct _shm_queue_node_t
{
    uint32_t id;            /* 结点ID(0~max-1) */
    uint32_t status;        /* 被占用标记(SHMQ_NODE_STAT_IDLE ~ SHMQ_NODE_STAT_USED) */
    uint32_t next;          /* 下一个结点偏移(shm_queue_node_t) */
    uint32_t data;          /* 数据偏移 */
} shm_queue_node_t;

/* 队列信息 */
typedef struct
{
    spinlock_t lock;        /* 锁 */

    uint32_t max;           /* 队列容量 */
    uint32_t num;           /* 占用个数 */
    uint32_t size;          /* 队列各节点的最大存储空间 */

    uint32_t base;          /* 队列基址(shm_queue_node_t) */
    uint32_t head;          /* 队列头(shm_queue_node_t) */
    uint32_t tail;          /* 队列尾(shm_queue_node_t) */

    uint32_t pool_off;      /* 内存对象(shm_pool_t) */
} shm_queue_info_t;

/* 队列对象 */
typedef struct
{
    shm_queue_info_t *info; /* 基本信息 */
    shm_pool_t *pool;       /* 内存池对象 */
} shm_queue_t;

shm_queue_t *shm_queue_creat(int key, int max, int size);
shm_queue_t *shm_queue_attach(int key);
#define shm_queue_malloc(shmq) shm_pool_alloc((shmq)->pool)
#define shm_queue_dealloc(shmq, p) shm_pool_dealloc((shmq)->pool, p)
int shm_queue_push(shm_queue_t *shmq, void *p);
void *shm_queue_pop(shm_queue_t *shmq);

#define shm_queue_data_count(shmq) ((shmq)->info->max - (shmq)->info->num)

#endif /*__SHM_QUEUE_H__*/
