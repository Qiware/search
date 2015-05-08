/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: invert.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # Wed 06 May 2015 10:22:56 PM CST #
 ******************************************************************************/

#include "invertd.h"
#include "invtd_conf.h"

#define INVTD_LOG_PATH      "../log/invertd.log"
#define INVTD_PLOG_PATH     "../log/invertd.plog"

invtd_cntx_t *invtd_cntx_init(void);

/******************************************************************************
 **函数名称: main 
 **功    能: 倒排服务主程序
 **输入参数:
 **     argc: 参数个数
 **     argv: 参数列表
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.07 #
 ******************************************************************************/
int main(int argc, char *argv[])
{
    invtd_cntx_t *ctx;

    ctx = invtd_cntx_init();
    if (NULL == ctx)
    {
        fprintf(stderr, "Init invertd failed!\n");
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: invtd_init_log
 **功    能: 初始化日志模块
 **输入参数:
 **     level: 日志级别
 **输出参数: NONE
 **返    回: 日志对象
 **实现描述: 初始化应用日志和平台日志
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.07 #
 ******************************************************************************/
static log_cycle_t *invtd_init_log(int level)
{
    log_cycle_t *log;

    log = log_init(level, INVTD_LOG_PATH);
    if (NULL == log)
    {
        return NULL;
    }

    if (plog_init(level, INVTD_PLOG_PATH))
    {
        return NULL;
    }

    return log;
}

/******************************************************************************
 **函数名称: invtd_init_sdtp
 **功    能: 初始化SDTP模块
 **输入参数:
 **     log: 日志对象
 **输出参数: NONE
 **返    回: SDTP对象
 **实现描述: 设置配置参数, 并传入初始化函数
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.07 #
 ******************************************************************************/
static sdtp_cntx_t *invtd_init_sdtp(log_cycle_t *log)
{
    sdtp_conf_t conf;

    snprintf(conf.name, sizeof(conf.name), "../temp/sdtp/invertd");
    conf.port = 9999;
    conf.recv_thd_num = 2;
    conf.work_thd_num = 4;
    conf.rqnum = 4;
    conf.recvq.max = 8092;
    conf.recvq.size = 1024;

    return sdtp_init(&conf, log);
}

/******************************************************************************
 **函数名称: invtd_cntx_init 
 **功    能: 初始化倒排服务
 **输入参数:
 **     argc: 参数个数
 **     argv: 参数列表
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 依次创建所需要的资源(日志 SDTP服务 倒排表等)
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.07 #
 ******************************************************************************/
invtd_cntx_t *invtd_cntx_init(void)
{
    log_cycle_t *log;
    invtd_cntx_t *ctx;

    /* > 初始化日志 */
    log = invtd_init_log(LOG_LEVEL_DEBUG);
    if (NULL == log)
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return NULL;
    }

    /* > 创建倒排对象 */
    ctx = (invtd_cntx_t *)calloc(1, sizeof(invtd_cntx_t));
    if (NULL == ctx)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->log = log;

    /* > 创建倒排表 */
    ctx->tab = invert_tab_creat(1024, log);
    if (NULL == ctx->tab)
    {
        free(ctx);
        log_error(log, "Create invert table failed!");
        return NULL;
    }

    /* > SDTP服务 */
    ctx->sdtp = invtd_init_sdtp(log);
    if (NULL == ctx->sdtp)
    {
        free(ctx);
        log_error(log, "Init sdtp failed!");
        return NULL;
    }

    return ctx;
}
