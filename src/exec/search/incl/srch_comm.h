#if !defined(__SRCH_COMM_H__)
#define __SRCH_COMM_H__

#include <stdint.h>
#include "common.h"

/* 错误码 */
typedef enum
{
    SRCH_OK = 0                             /* 正常 */
    , SRCH_SHOW_HELP                        /* 显示帮助信息 */
    , SRCH_DONE                             /* 完成 */
    , SRCH_SCK_AGAIN                        /* 出现EAGAIN提示 */
    , SRCH_SCK_CLOSE                        /* 套接字关闭 */

    , SRCH_ERR = ~0x7FFFFFFF                /* 失败、错误 */
} srch_err_code_e;

/* 输入参数 */
typedef struct
{
    char conf_path[FILE_NAME_MAX_LEN];      /* 配置文件路径 */
    bool isdaemon;                          /* 是否后台运行 */
} srch_opt_t;

/* 注册回调类型 */
typedef int (*srch_reg_cb_t)(uint8_t type, char *buff, size_t len, void *args);

/* 注册对象 */
typedef struct
{
    uint8_t type;                           /* 数据类型 范围:(0 ~ SRCH_MSG_TYPE_MAX) */
#define SRCH_REG_FLAG_UNREG     (0)         /* 0: 未注册 */
#define SRCH_REG_FLAG_REGED     (1)         /* 1: 已注册 */
    uint8_t flag;                           /* 注册标志 范围:(0: 未注册 1: 已注册) */
    srch_reg_cb_t cb;                       /* 对应数据类型的处理函数 */
    void *args;                             /* 附加参数 */
} srch_reg_t;

/* 新增套接字对象 */
typedef struct
{
    int fd;                                 /* 套接字 */
    uint64_t sck_serial;                    /* SCK流水号 */
} srch_add_sck_t;

#endif /*__SRCH_COMM_H__*/
