/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: invtd_conf.h
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # Fri 08 May 2015 08:28:17 AM CST #
 ******************************************************************************/
#if !defined(__INVTD_CONF_H__)
#define __INVTD_CONF_H__

#include "sdtp.h"

/* 配置信息 */
typedef struct
{
    sdtp_conf_t sdtp;
} invtd_conf_t;

int invtd_conf_load(const char *path, invtd_conf_t *conf);

#endif /*__INVTD_CONF_H__*/
