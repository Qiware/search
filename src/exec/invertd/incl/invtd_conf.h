#if !defined(__INVTD_CONF_H__)
#define __INVTD_CONF_H__

#include "sdtp_recv.h"

/* 配置信息 */
typedef struct
{
    int invt_tab_max;                   /* 倒排表长度 */
    sdtp_conf_t sdtp;                   /* SDTP配置 */
} invtd_conf_t;

int invtd_conf_load(const char *path, invtd_conf_t *conf);

#endif /*__INVTD_CONF_H__*/
