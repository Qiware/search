#if !defined(__INVTD_CONF_H__)
#define __INVTD_CONF_H__

#if defined(__RTTP_SUPPORT__)
#include "rtrd_recv.h"
#else /*__RTTP_SUPPORT__*/
#include "sdrd_recv.h"
#endif /*__RTTP_SUPPORT__*/

/* 配置信息 */
typedef struct
{
    int invt_tab_max;                   /* 倒排表长度 */
#if defined(__RTTP_SUPPORT__)
    rtrd_conf_t sdrd;                   /* SDTP配置 */
#else /*__RTTP_SUPPORT__*/
    sdrd_conf_t sdrd;                   /* SDTP配置 */
#endif /*__RTTP_SUPPORT__*/
} invtd_conf_t;

int invtd_conf_load(const char *path, invtd_conf_t *conf);

#endif /*__INVTD_CONF_H__*/
