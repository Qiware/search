#if !defined(__RTMQ_SUB_H__)
#define __RTMQ_SUB_H__

#include "comm.h"
#include "mesg.h"
#include "vector.h"
#include "avl_tree.h"

/* 订阅连接 */
typedef struct
{
    uint64_t sid;               /* 会话ID */
    int nodeid;                 /* 订阅结点ID */
} rtmq_sub_node_t;

/* 订阅列表 */
typedef struct
{
    mesg_type_e type;           /* 订阅类型 */
#define RTMQ_SUB_VEC_LEN   (32)
#define RTMQ_SUB_VEC_INCR  (32)
    vector_t *nodes;            /* 订阅结点列表(数组管理) */
} rtmq_sub_list_t;

/* 订阅管理 */
typedef struct
{
    pthread_rwlock_t lock;      /* 读写锁 */
    avl_tree_t *tab;            /* 订阅表(注:以type为主键, 存储rtmq_sub_list_t类型) */
} rtmq_sub_mgr_t;

#endif /*__RTMQ_SUB_H__*/
