#if !defined(__CRWL_COMM_H__)
#define __CRWL_COMM_H__

typedef enum
{
    CRWL_OK
    , CRWL_SHOW_HELP            /* 显示帮助信息 */
    , CRWL_FAIL = ~0x7fffffff   /* 失败、错误 */
}crwl_err_code_e;

int crwl_load_conf(crawler_conf_t *conf, const char *path);
int crwl_start_server(crawler_conf_t *conf);

int crwl_start_work(crawler_conf_t *conf);

#endif /*__CRWL_COMM_H__*/
