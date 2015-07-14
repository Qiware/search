/******************************************************************************
 ** Copyright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: frwd_comm.c
 ** 版本号: 1.0
 ** 描  述: 通用函数定义
 ** 作  者: # Qifeng.zou # Wed 10 Jun 2015 12:14:26 PM CST #
 ******************************************************************************/

#include "frwd.h"
#include "conf.h"
#include "mesg.h"
#include "agent.h"
#include "command.h"
#include "lsnd_conf.h"

#define FRWD_DEF_CONF_PATH  "../conf/frwder.xml"

static int frwd_init_log(frwd_cntx_t *frwd, const char *pname);
static int frwd_init_lsnd(frwd_cntx_t *frwd, const frwd_conf_t *conf);
static int frwd_attach_lsnd_distq(frwd_lsnd_t *lsnd, lsnd_conf_t *conf);

/******************************************************************************
 **函数名称: frwd_getopt 
 **功    能: 解析输入参数
 **输入参数: 
 **     argc: 参数个数
 **     argv: 参数列表
 **输出参数:
 **     opt: 参数选项
 **返    回: 0:成功 !0:失败
 **实现描述: 解析和验证输入参数
 **注意事项: 
 **     c: 配置文件路径
 **     h: 帮助手册
 **     d: 以精灵进程运行
 **作    者: # Qifeng.zou # 2015.06.10 #
 ******************************************************************************/
int frwd_getopt(int argc, char **argv, frwd_opt_t *opt)
{
    int ch;

    memset(opt, 0, sizeof(frwd_opt_t));

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
                return FRWD_SHOW_HELP;
            }
        }
    }

    optarg = NULL;
    optind = 1;

    /* 2. 验证输入参数 */
    if (!strlen(opt->conf_path))
    {
        snprintf(opt->conf_path, sizeof(opt->conf_path), "%s", FRWD_DEF_CONF_PATH);
    }

    return FRWD_OK;
}

/******************************************************************************
 **函数名称: frwd_usage
 **功    能: 显示启动参数帮助信息
 **输入参数:
 **     exec: 程序名
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.10 #
 ******************************************************************************/
int frwd_usage(const char *exec)
{
    printf("\nUsage: %s [-h] [-d] -c <config file>\n", exec);
    printf("\t-h\tShow help\n"
           "\t-c\tConfiguration path\n\n");
    return FRWD_OK;
}

/******************************************************************************
 **函数名称: frwd_init 
 **功    能: 初始化转发服务
 **输入参数: 
 **     conf: 配置信息
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.10 #
 ******************************************************************************/
frwd_cntx_t *frwd_init(const frwd_conf_t *conf)
{
    frwd_cntx_t *frwd;
    char path[FILE_PATH_MAX_LEN];

    frwd = (frwd_cntx_t *)calloc(1, sizeof(frwd_cntx_t));
    if (NULL == frwd)
    {
        fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    memcpy(&frwd->conf, conf, sizeof(frwd_conf_t));

    do
    {
        /* > 初始化日志 */
        if (frwd_init_log(frwd, "frwder"))
        {
            fprintf(stderr, "Initialize log failed!\n");
            break;
        }

        /* > 创建命令套接字 */
        snprintf(path, sizeof(path), "../temp/frwder/cmd.usck");

        frwd->cmd_sck_id = unix_udp_creat(path);
        if (frwd->cmd_sck_id < 0)
        {
            fprintf(stderr, "Create unix udp failed! path:%s\n", path);
            break;
        }

        /* > 初始化侦听相关资源 */
        if (frwd_init_lsnd(frwd, conf))
        {
            fprintf(stderr, "Initialize search engine failed!\n");
            break;
        }

        /* > 初始化发送服务 */
        frwd->rtmq = rtsd_init(&conf->conn_invtd, frwd->log);
        if (NULL == frwd->rtmq) 
        {
            log_fatal(frwd->log, "Initialize send-server failed!");
            break;
        }

        return frwd;
    } while (0);

    free(frwd);
    return NULL;
}

/******************************************************************************
 **函数名称: frwd_startup
 **功    能: 初始化转发服务
 **输入参数: 
 **     frwd: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.10 #
 ******************************************************************************/
int frwd_startup(frwd_cntx_t *frwd)
{
    if (rtsd_start(frwd->rtmq))
    {
        log_fatal(frwd->log, "Start up send-server failed!");
        return FRWD_ERR;
    }

    return FRWD_OK;
}

/******************************************************************************
 **函数名称: frwd_init_log
 **功    能: 初始化日志模块
 **输入参数:
 **     frwd: 全局对象
 **     pname: 进程名
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-10 #
 ******************************************************************************/
static int frwd_init_log(frwd_cntx_t *frwd, const char *pname)
{
    char path[FILE_PATH_MAX_LEN];
    frwd_conf_t *conf = &frwd->conf;

    snprintf(path, sizeof(path), "../log/%s.log", pname);

    frwd->log = log_init(conf->log_level, path);
    if (NULL == frwd->log)
    {
        fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
        return FRWD_ERR;
    }

    return FRWD_OK;
}

/******************************************************************************
 **函数名称: frwd_init_lsnd
 **功    能: 初始化与转发相关联的侦听服务的信息
 **输入参数:
 **     frwd: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.07.02 19:14:59 #
 ******************************************************************************/
static int frwd_init_lsnd(frwd_cntx_t *frwd, const frwd_conf_t *conf)
{
    lsnd_conf_t lcf;
    frwd_lsnd_t *lsnd = &frwd->lsnd;

    /* > 加载配置信息 */
    if (lsnd_load_conf("../conf/listend.xml", &lcf, NULL))
    {
        log_error(frwd->log, "Load listend configuration failed!");
        return FRWD_ERR;
    }

    snprintf(lsnd->name, sizeof(lsnd->name), "%s", conf->lsnd_name); /* 服务名 */
    snprintf(lsnd->dist_cmd_path, sizeof(lsnd->dist_cmd_path),       /* 分发服务命令 */
             "../temp/listend/%s/dsvr.usck", lsnd->name);

    if (frwd_attach_lsnd_distq(lsnd, &lcf))
    {
        log_error(frwd->log, "Attach distq of listend failed!");
        return FRWD_ERR;
    }

    return 0;
}

/******************************************************************************
 **函数名称: frwd_attach_lsnd_distq
 **功    能: 附着分发队列
 **输入参数: 
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-07-05 18:12:16 #
 ******************************************************************************/
static int frwd_attach_lsnd_distq(frwd_lsnd_t *lsnd, lsnd_conf_t *conf)
{
    int idx;
    char path[FILE_NAME_MAX_LEN];

    lsnd->distq_num = conf->distq.num;

    /* > 申请对象空间 */
    lsnd->distq = (shm_queue_t **)calloc(1, lsnd->distq_num * sizeof(shm_queue_t *));
    if (NULL == lsnd->distq)
    {
        return FRWD_ERR;
    }

    /* > 依次附着队列 */
    for (idx=0; idx<conf->distq.num; ++idx)
    {
        snprintf(path, sizeof(path), "%s/dist-%d.shmq", conf->wdir, idx);

        lsnd->distq[idx] = shm_queue_attach(path);
        if (NULL == lsnd->distq[idx])
        {
            free(lsnd->distq);
            return FRWD_ERR;
        }
    }
    return FRWD_OK;
}
