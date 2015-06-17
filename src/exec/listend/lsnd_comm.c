#include "mesg.h"
#include "listend.h"

/******************************************************************************
 **函数名称: lsnd_getopt 
 **功    能: 解析输入参数
 **输入参数: 
 **     argc: 参数个数
 **     argv: 参数列表
 **输出参数:
 **     opt: 参数选项
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 解析输入参数
 **     2. 验证输入参数
 **注意事项: 
 **     c: 配置文件路径
 **     h: 帮助手册
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
int lsnd_getopt(int argc, char **argv, lsnd_opt_t *opt)
{
    int ch;

    /* 1. 解析输入参数 */
    while (-1 != (ch = getopt(argc, argv, "c:hd")))
    {
        switch (ch)
        {
            case 'c':   /* 指定配置文件 */
            {
                snprintf(opt->conf_path, sizeof(opt->conf_path), "%s", optarg);
                break;
            }
            case 'd':
            {
                opt->isdaemon = true;
                break;
            }
            case 'h':   /* 显示帮助信息 */
            default:
            {
                return LSND_SHOW_HELP;
            }
        }
    }

    optarg = NULL;
    optind = 1;

    /* 2. 验证输入参数 */
    if (!strlen(opt->conf_path))
    {
        snprintf(opt->conf_path, sizeof(opt->conf_path), "%s", LSND_DEF_CONF_PATH);
    }

    return 0;
}

/* 显示启动参数帮助信息 */
int lsnd_usage(const char *exec)
{
    printf("\nUsage: %s [-h] [-d] -c <config file> [-l log_level]\n", exec);
    printf("\t-h\tShow help\n"
           "\t-c\tConfiguration path\n\n");
    return 0;
}

/* 初始化日志模块 */
log_cycle_t *lsnd_init_log(char *fname)
{
    char path[FILE_NAME_MAX_LEN];

    log_get_path(path, sizeof(path), basename(fname));

    return log_init(LOG_LEVEL_ERROR, path);
}

/******************************************************************************
 **函数名称: drcv_dist_routine
 **功    能: 运行分发线程
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 分发对象
 **实现描述:
 **注意事项: 消息结构: 流水信息 + 消息头 + 消息体
 **作    者: # Qifeng.zou # 2015.05.15 #
 ******************************************************************************/
void *lsnd_dist_routine(void *_ctx)
{
#define LSND_DIST_POP_NUM   (128)
    int num, idx;
    agent_flow_t *flow;
    rttp_header_t *head;
    void *addr[LSND_DIST_POP_NUM];
    lsnd_cntx_t *ctx = (lsnd_cntx_t *)_ctx;

    while (1)
    {
        /* > 获取弹出个数 */
        num = MIN(shm_queue_used(ctx->sendq), LSND_DIST_POP_NUM);
        if (0 == num)
        {
            usleep(500); /* TODO: 可使用事件通知机制减少CPU的消耗 */
            continue;
        }

        /* > 弹出发送数据 */
        num = shm_queue_mpop(ctx->sendq, addr, num);
        if (0== num)
        {
            continue;
        }

        for (idx=0; idx<num; ++idx)
        {
            /* > 获取发送队列 */
            flow = (agent_flow_t *)addr[idx]; // 流水信息
            head = (rttp_header_t *)(flow + 1); // 消息头
            if (RTTP_CHECK_SUM != head->checksum)
            {
                assert(0);
            }

            log_debug(ctx->log, "Call %s()! type:%d len:%d", __func__, head->type, head->length);

            agent_send(ctx->agent, head->type, flow->serial, head+1, head->length);

            shm_queue_dealloc(ctx->sendq, addr[idx]);
        }
    }

    return (void *)-1;
}
