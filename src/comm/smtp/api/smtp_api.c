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
#include "smtp_cmd.h"
#include "smtp_comm.h"
#include "smtp_snd_cli.h"
#include "smtp_snd_svr.h"
#include "orm_atomic.h"
#include "msger_dts.h"
#define SMTP_BODY_MAX_LEN        (10*1024)   /* 发送端接收的报文体最大长度 */

/* 静态函数声明 */
static int smtp_snd_init(smtp_snd_ctx_t *ctx);

/******************************************************************************
 **函数名称: smtp_snd_startup
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
smtp_snd_ctx_t *smtp_snd_startup(const smtp_snd_conf_t *conf)
{
    int ret = 0;
    smtp_snd_ctx_t *ctx = NULL;

    /* 1. 创建上下文对象 */
    ctx = (smtp_snd_ctx_t *)calloc(1, sizeof(smtp_snd_ctx_t));
    if (NULL == ctx)
    {
        printf("errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    /* 2. 加载配置信息 */
    memcpy(&ctx->conf, conf, sizeof(smtp_snd_conf_t));

    /* 3. 根据配置进行初始化处理 */
    ret = smtp_snd_init(ctx);
    if (0 != ret)
    {
        printf("Init send thread failed!");
        return NULL;
    }

    return ctx;
}

/******************************************************************************
 **函数名称: smtp_snd_init
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
static int smtp_snd_init(smtp_snd_ctx_t *ctx)
{
    /* 1. 创建Send线程池 */
    if (smtp_snd_creat_sendtp(ctx))
    {
        LogError("Create send thread pool failed!");
        return SMTP_ERR;
    }

    return 0;
}
