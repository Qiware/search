#if !defined(__SHM_LIST_H__)
#define __SHM_LIST_H__

/* 链表结点 */
typedef struct
{
    off_t prev;                 /* 前结点(偏移) */
    off_t next;                 /* 后结点(偏移) */
    off_t data;                 /* 负载数据(偏移) */
} shm_list_node_t;

/* 链表对象 */
typedef struct
{
    int num;                    /* 结点数 */
    off_t head;                 /* 头结点偏移 */
} shm_list_t;

int shm_list_lpush(void *addr, shm_list_t *list, off_t node_off);

#endif /*__SHM_LIST_H__*/
