#if !defined(__CRWL_MAN_H__)
#define __CRWL_MAN_H__

#include "log.h"
#include "comm.h"
#include "crawler.h"
#include "avl_tree.h"

#define CRWL_MAN_DATA_DIR "../data/crwl/man" /* 数据存储目录 */

/* 爬虫代理 */
typedef struct
{
    int fd;                             /* 侦听套接字 */
    fd_set rdset;                       /* 可读集合 */
    fd_set wrset;                       /* 可写集合 */

    list_t *mesg_list;                  /* 发送链表 */

    avl_tree_t *reg;                    /* 回调注册 */
    log_cycle_t *log;                   /* 日志对象 */
} crwl_man_t;

/* 回调函数类型 */
typedef int (*crwl_man_reg_cb_t)(crwl_cntx_t *ctx,
        crwl_man_t *man, unsigned int type, char *buff, struct sockaddr_un *from, void *args);

/* 注册信息 */
typedef struct
{
    unsigned int type;                  /* 消息类型 */
    crwl_man_reg_cb_t proc;             /* 回调函数指针 */
    void *args;                         /* 附加参数 */
} crwl_man_reg_t;

void *crwl_manager_routine(void *_ctx);

#endif /*__CRWL_MAN_H__*/
