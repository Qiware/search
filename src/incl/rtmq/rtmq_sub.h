#if !defined(__RTMQ_SUB_H__)
#define __RTMQ_SUB_H__

#include "comm.h"
#include "mesg.h"
#include "vector.h"
#include "hash_tab.h"

/* 订阅连接 */
typedef struct
{
    uint64_t sid;               /* 会话ID */
    int nid;                    /* 订阅结点ID */
} rtmq_sub_node_t;

/* 订阅列表 */
typedef struct
{
    mesg_type_e type;           /* 订阅类型 */
    list2_t *nodes;             /* 订阅结点列表(数组管理) */
} rtmq_sub_list_t;

/* 订阅管理 */
typedef struct
{
    hash_tab_t *sub_one_tab;        /* 订阅表(注:以type为主键, 存储rtmq_sub_list_t类型) */
    hash_tab_t *sub_all_tab;        /* 订阅表(注:以type为主键, 存储rtmq_sub_list_t类型) */
} rtmq_sub_mgr_t;

#endif /*__RTMQ_SUB_H__*/
