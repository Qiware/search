#if !defined(__CRWL_COMM_H__)
#define __CRWL_COMM_H__

#include "log.h"

#define CRWL_DEF_THD_NUM    (1) /* 爬虫默认线程数 */

typedef enum
{
    CRWL_OK = 0
    , CRWL_SHOW_HELP            /* 显示帮助信息 */
    , CRWL_ERR = ~0x7fffffff    /* 失败、错误 */
}crwl_err_code_e;

int crwl_load_conf(crwl_conf_t *conf, const char *path, log_cycle_t *log);
int crwl_start_work(crwl_conf_t *conf, log_cycle_t *log);

#endif /*__CRWL_COMM_H__*/
