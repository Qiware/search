#include <fcntl.h>
#include <memory.h>
#include <stdarg.h>
#include <sys/un.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "xml_tree.h"
#include "smtc_cmd.h"
#include "smtc_cli.h"
#include "smtc_ssvr.h"
#include "smtc_comm.h"
#include "orm_atomic.h"

#define SMTC_BODY_MAX_LEN        (10*1024)   /* 发送端接收的报文体最大长度 */

/* 静态函数声明 */
static int smtc_snd_init(smtc_ssvr_cntx_t *ctx);

/******************************************************************************
 **函数名称: smtc_snd_startup
 **功    能: 启动发送端
 **输入参数: 
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 0:Success !0:Failed
 **实现描述: 
 **     1. 创建上下文对象
 **     2. 加载配置文件
 **     3. 根据配置进行初始化处理
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.21 #
 ******************************************************************************/
smtc_ssvr_cntx_t *smtc_snd_startup(const smtc_ssvr_conf_t *conf)
{
    int ret;
    smtc_ssvr_cntx_t *ctx;

    /* 1. 创建上下文对象 */
    ctx = (smtc_ssvr_cntx_t *)calloc(1, sizeof(smtc_ssvr_cntx_t));
    if (NULL == ctx)
    {
        printf("errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    /* 2. 加载配置信息 */
    memcpy(&ctx->conf, conf, sizeof(smtc_ssvr_conf_t));

    /* 3. 根据配置进行初始化处理 */
    if (!smtc_snd_init(ctx))
    {
        printf("Init send thread failed!");
        return NULL;
    }

    return ctx;
}

/******************************************************************************
 **函数名称: smtc_snd_init
 **功    能: 根据客户端配置信息进行初始化处理
 **输入参数: 
 **     ctx: 上下文信息
 **输出参数: NONE
 **返    回: 0:Success !0:Failed
 **实现描述: 
 **     1. 创建SND线程池
 **     2. 创建KPALIVE线程
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.25 #
 ******************************************************************************/
static int smtc_snd_init(smtc_ssvr_cntx_t *ctx)
{
    /* 1. 创建Send线程池 */
    if (smtc_ssvr_creat_sendtp(ctx))
    {
        LogError("Create send thread pool failed!");
        return SMTC_ERR;
    }

    return 0;
}
