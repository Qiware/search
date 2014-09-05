#if !defined(__CRWL_COMM_H__)
#define __CRWL_COMM_H__

#include "log.h"

typedef enum
{
    CRWL_OK
    , CRWL_SHOW_HELP            /* 显示帮助信息 */
    , CRWL_ERR = ~0x7fffffff    /* 失败、错误 */
}crwl_err_code_e;

int crwl_load_conf(crawler_conf_t *conf, const char *path, log_cycle_t *log);
int crwl_start_work(crawler_conf_t *conf, log_cycle_t *log);

#endif /*__CRWL_COMM_H__*/
