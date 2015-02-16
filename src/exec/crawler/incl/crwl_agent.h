#if !defined(__CRWL_AGENT_H__)
#define __CRWL_AGENT_H__

#include "log.h"
#include "slab.h"
#include "common.h"
#include "avl_tree.h"

#define CRWL_AGT_PORT   (8888)          /* 代理端口号 */

/* 回调函数类型 */
typedef int (*crwl_agt_reg_cb_t)(uint32_t type, char *buff, void *args);

/* 注册信息 */
typedef struct
{
    uint32_t type;                      /* 消息类型 */
    crwl_agt_reg_cb_t proc;             /* 回调函数指针 */
    void *args;                         /* 附加参数 */
} crwl_agt_reg_t;

/* 爬虫代理 */
typedef struct
{
    int fd;                             /* 侦听套接字 */
    fd_set rdset;                       /* 可读集合 */
    fd_set wrset;                       /* 可写集合 */

    list_t *mesg_list;                  /* 发送链表 */

    avl_tree_t *reg;                    /* 回调注册 */
    log_cycle_t *log;                   /* 日志对象 */
    slab_pool_t *slab;                  /* 内存池对象 */
} crwl_agent_t;

void *crwl_agt_routine(void *_ctx);

#endif /*__CRWL_AGENT_H__*/
