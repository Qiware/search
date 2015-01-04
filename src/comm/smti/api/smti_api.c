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
#include "smti_cmd.h"
#include "smti_comm.h"
#include "smti_snd_cli.h"
#include "smti_snd_svr.h"
#include "orm_atomic.h"
#include "msger_dts.h"
#define SMTI_BODY_MAX_LEN        (10*1024)   /* 发送端接收的报文体最大长度 */

/* 静态函数声明 */
static int smti_snd_init(smti_snd_ctx_t *ctx);

/******************************************************************************
 **函数名称: smti_snd_startup
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
smti_snd_ctx_t *smti_snd_startup(const smti_snd_conf_t *conf)
{
    int ret = 0;
    smti_snd_ctx_t *ctx = NULL;

    /* 1. 创建上下文对象 */
    ctx = (smti_snd_ctx_t *)calloc(1, sizeof(smti_snd_ctx_t));
    if (NULL == ctx)
    {
        printf("errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    /* 2. 加载配置信息 */
    memcpy(&ctx->conf, conf, sizeof(smti_snd_conf_t));

    /* 3. 根据配置进行初始化处理 */
    ret = smti_snd_init(ctx);
    if (0 != ret)
    {
        printf("Init send thread failed!");
        return NULL;
    }

    return ctx;
}

/******************************************************************************
 **函数名称: smti_snd_init
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
static int smti_snd_init(smti_snd_ctx_t *ctx)
{
    /* 1. 创建Send线程池 */
    if (smti_snd_creat_sendtp(ctx))
    {
        LogError("Create send thread pool failed!");
        return SMTI_ERR;
    }

    return 0;
}
