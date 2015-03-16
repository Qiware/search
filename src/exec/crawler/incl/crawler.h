/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: crawler.h
 ** 版本号: 1.0
 ** 描  述: 网络爬虫
 **         负责下载指定URL网页
 ** 作  者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
#if !defined(__CRAWLER_H__)
#define __CRAWLER_H__

#include <stdint.h>

#include "log.h"
#include "slab.h"
#include "list.h"
#include "queue.h"
#include "common.h"
#include "sck_api.h"
#include "hash_tab.h"
#include "crwl_priv.h"
#include "crwl_task.h"
#include "crwl_conf.h"
#include "thread_pool.h"

/* 发送数据的信息 */
typedef struct
{
    int type;                               /* 数据类型(crwl_data_type_e) */
    int length;                             /* 数据长度(报头+报体) */
} crwl_data_info_t;

/* 爬虫全局信息 */
typedef struct
{
    time_t run_tm;                          /* 运行时间 */

    crwl_conf_t *conf;                      /* 配置信息 */
    log_cycle_t *log;                       /* 日志对象 */
    slab_pool_t *slab;                      /* 内存池 */

    thread_pool_t *scheds;                  /* Sched线程池 */
    thread_pool_t *worker_pool;             /* Worker线程池 */

    queue_t **workq;                        /* 工作队列(注: 与WORKER线程一一对象) */
} crwl_cntx_t;

log_cycle_t *crwl_init_log(char *fname);
crwl_cntx_t *crwl_cntx_init(char *pname, const char *path);
void crwl_cntx_destroy(crwl_cntx_t *ctx);
int crwl_startup(crwl_cntx_t *ctx);

#endif /*__CRAWLER_H__*/
