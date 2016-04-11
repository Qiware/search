#if !defined(__FRWD_H__)
#define __FRWD_H__

#include "mesg.h"
#include "vector.h"
#include "shm_queue.h"
#include "rtsd_send.h"
#include "rtsd_ssvr.h"
#include "frwd_conf.h"

#define FRWD_CMD_PATH "../temp/frwder/cmd.usck"

typedef enum
{
    FRWD_OK                                 /* 正常 */
    , FRWD_SHOW_HELP                        /* 显示帮助 */

    , FRWD_ERR = ~0x7fffffff                /* 异常 */
} frwd_err_code_e;

/* 输入参数 */
typedef struct
{
    int log_level;                          /* 日志级别 */
    bool isdaemon;                          /* 是否后台运行 */
    char *conf_path;                        /* 配置路径 */
} frwd_opt_t;

/* 订阅结点 */
typedef struct
{
    int nodeid;                             /* 订阅结点ID */
} frwd_sub_item_t;

/* 订阅列表 */
typedef struct
{
    mesg_type_e type;                       /* 订阅类型 */
#define FRWD_SUB_VEC_CAP   (32)
#define FRWD_SUB_VEC_INCR  (32)
    vector_t *list;                         /* 订阅结点列表(数组管理) */
} frwd_sub_list_t;

/* 订阅管理 */
typedef struct
{
    pthread_rwlock_t lock;                  /* 读写锁 */
    avl_tree_t *list;                       /* 订阅列表(注:以type为主键, 存储frwd_sub_list_t类型) */
} frwd_sub_mgr_t;

/* 全局对象 */
typedef struct
{
    int cmd_sck_id;                         /* 命令套接字 */
    frwd_conf_t conf;                       /* 配置信息 */
    log_cycle_t *log;                       /* 日志对象 */
    rtrd_cntx_t *upstrm;                    /* UpStream对象 */
    rtrd_cntx_t *downstrm;                  /* DownStream对象(用于接收来自下游的数据) */
    frwd_sub_mgr_t upstrm_sub_mgr;          /* 上游订阅管理 */
    frwd_sub_mgr_t downstrm_sub_mgr;        /* 下游订阅管理 */
} frwd_cntx_t;

int frwd_getopt(int argc, char **argv, frwd_opt_t *opt);
int frwd_usage(const char *exec);
log_cycle_t *frwd_init_log(const char *pname, int log_level);
frwd_cntx_t *frwd_init(const frwd_conf_t *conf, log_cycle_t *log);
int frwd_launch(frwd_cntx_t *frwd);
int frwd_set_reg(frwd_cntx_t *frwd);

#endif /*__FRWD_H__*/
