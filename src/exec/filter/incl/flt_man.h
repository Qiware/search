#if !defined(__FLT_MAN_H__)
#define __FLT_MAN_H__

#include "log.h"
#include "comm.h"
#include "slab.h"
#include "filter.h"
#include "avl_tree.h"

/* 过滤代理 */
typedef struct
{
    int fd;                             /* 侦听套接字 */
    fd_set rdset;                       /* 可读集合 */
    fd_set wrset;                       /* 可写集合 */

    list_t *mesg_list;                  /* 发送链表 */

    avl_tree_t *reg;                    /* 回调注册 */
    log_cycle_t *log;                   /* 日志对象 */
    slab_pool_t *slab;                  /* 内存池对象 */
} flt_man_t;

/* 回调函数类型 */
typedef int (*flt_man_reg_cb_t)(flt_cntx_t *ctx,
        flt_man_t *man, uint32_t type, char *buff, struct sockaddr_un *from, void *args);

/* 注册信息 */
typedef struct
{
    uint32_t type;                      /* 消息类型 */
    flt_man_reg_cb_t proc;             /* 回调函数指针 */
    void *args;                         /* 附加参数 */
} flt_man_reg_t;

void *flt_manager_routine(void *_ctx);

#endif /*__FLT_MAN_H__*/
