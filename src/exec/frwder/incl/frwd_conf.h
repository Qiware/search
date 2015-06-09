#if !defined(__FRWD_CONF_H__)
#define __FRWD_CONF_H__

#include "sck.h"
#include "comm.h"
#if defined(__RTTP_SUPPORT__)
#include "rtsd_send.h"
#else /*__RTTP_SUPPORT__*/
#include "sdsd_send.h"
#endif /*__RTTP_SUPPORT__*/

/* 配置信息 */
typedef struct
{
#if defined(__RTTP_SUPPORT__)
    rtsd_conf_t conn_invtd;               /* RTTP配置 */
#else /*__RTTP_SUPPORT__*/
    sdsd_conf_t conn_invtd;               /* SDTP配置 */
#endif /*__RTTP_SUPPORT__*/
} frwd_conf_t;

#endif /*__FRWD_CONF_H__*/
