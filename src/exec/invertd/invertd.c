/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: invert.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # Wed 06 May 2015 10:22:56 PM CST #
 ******************************************************************************/

#include "invertd.h"

int main(int argc, char *argv[])
{
    invtd_cntx_t *ctx;

    ctx = 

    return 0;
}

invtd_cntx_t *invtd_cntx_init(void)
{
    log_cycle_t *log;
    invtd_cntx_t *ctx;

    ctx = (invtd_cntx_t *)calloc(1, sizeof(invtd_cntx_t));
    if (NULL == ctx)
    {
        return NULL;
    }

    ctx->tab = invert_tab_creat(1024, log);
    if (NULL == ctx->tab)
    {
        free(ctx);
        return NULL;
    }

    return ctx;
}
