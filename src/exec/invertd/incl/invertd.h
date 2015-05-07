/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: invertd.h
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # Wed 06 May 2015 10:32:12 PM CST #
 ******************************************************************************/
#if !defined(__INVERTD_H__)
#define __INVERTD_H__

#include "log.h"
#include "sdtp.h"
#include "invert_tab.h"

typedef struct
{
    log_cycle_t *log;                       /* 日志对象 */
    sdtp_cntx_t *sdtp;                      /* SDTP服务 */
    invt_tab_t *tab;                        /* 倒排表 */
} invtd_cntx_t;

#endif /*__INVERTD_H__*/
