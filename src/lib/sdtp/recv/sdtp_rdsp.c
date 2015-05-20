/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: sdtp_rdsp.c
 ** 版本号: 1.0
 ** 描  述: 负责接收端发送数据的分发处理
 ** 作  者: # Qifeng.zou # Fri 15 May 2015 11:45:31 PM CST #
 ******************************************************************************/
#include "sdtp.h"
#include "sdtp_cmd.h"
#include "sdtp_priv.h"

/******************************************************************************
 **函数名称: sdtp_rdsp_init
 **功    能: 初始化分发线程
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 分发对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.15 #
 ******************************************************************************/
sdtp_rdsp_t *sdtp_rdsp_init(sdtp_rctx_t *ctx)
{
    sdtp_rdsp_t *dsp;

    dsp = (sdtp_rdsp_t *)calloc(1, sizeof(sdtp_rdsp_t));
    if (NULL == dsp)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    return dsp
}
