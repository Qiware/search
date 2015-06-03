/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: search.c
 ** 版本号: 1.0
 ** 描  述: 搜索引擎
 **         负责接受搜索请求，并将搜索结果返回给客户端
 ** 作  者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/

#include "sck.h"
#include "comm.h"
#include "lock.h"
#include "hash.h"
#include "mesg.h"
#include "probe.h"
#include "agentd.h"
#include "syscall.h"
#include "sdtp_cli.h"
#include "agtd_conf.h"
#include "prob_mesg.h"
#include "prob_worker.h"

#define AGTD_PROC_LOCK_PATH "../temp/prob/prob.lck"

typedef struct
{
    agtd_conf_t *conf;                  /* 配置信息 */

    sdtp_cli_t *sdtp;                   /* SDTP服务 */
    prob_cntx_t *prob;                  /* 探针服务 */
    log_cycle_t *log;                   /* 日志对象 */

    int len;                            /* 业务请求树长 */
    avl_tree_t **serial_to_sck_map;     /* 序列与SCK的映射 */
} agtd_cntx_t;

static agtd_cntx_t *agtd_init(char *pname, const char *path);
static int agtd_set_reg(agtd_cntx_t *agtd);

/******************************************************************************
 **函数名称: main 
 **功    能: 代理服务
 **输入参数: 
 **     argc: 参数个数
 **     argv: 参数列表
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 加载配置，再通过配置启动各模块
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
int main(int argc, char *argv[])
{
    agtd_opt_t opt;
    agtd_cntx_t *agtd;

    memset(&opt, 0, sizeof(opt));

    /* > 解析输入参数 */
    if (agtd_getopt(argc, argv, &opt))
    {
        return agtd_usage(argv[0]);
    }

    if (opt.isdaemon)
    {
        /* int daemon(int nochdir, int noclose);
         *  1． daemon()函数主要用于希望脱离控制台,以守护进程形式在后台运行的程序.
         *  2． 当nochdir为0时,daemon将更改进城的根目录为root(“/”).
         *  3． 当noclose为0是,daemon将进城的STDIN, STDOUT, STDERR都重定向到/dev/null */
        daemon(1, 1);
    }

    /* > 进程初始化 */
    agtd = agtd_init(argv[0], opt.conf_path);
    if (NULL == agtd)
    {
        fprintf(stderr, "Initialize agtd failed!");
        return -1;
    }
 
    /* 注册回调函数 */
    if (agtd_set_reg(agtd))
    {
        fprintf(stderr, "Set register callback failed!");
        return -1;
    }

    /* 3. 启动爬虫服务 */
    if (prob_startup(agtd->prob))
    {
        fprintf(stderr, "Startup search-engine failed!");
        goto ERROR;
    }

    while (1) { pause(); }

ERROR:
    /* 4. 销毁全局信息 */
    prob_cntx_destroy(agtd->prob);

    return -1;
}

/******************************************************************************
 **函数名称: agtd_sdtp_set_conf
 **功    能: 设置SDTP配置信息
 **输入参数: NONE
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 设置SDTP的配置信息
 **注意事项: TODO: 可改为使用配置文件的方式加载配置信息
 **作    者: # Qifeng.zou # 2015.05.28 22:58:17 #
 ******************************************************************************/
static int agtd_sdtp_set_conf(sdtp_ssvr_conf_t *conf)
{
    memset(conf, 0, sizeof(sdtp_ssvr_conf_t));

    snprintf(conf->name, sizeof(conf->name), "SDTP-SEND");

    snprintf(conf->auth.usr, sizeof(conf->auth.usr), "qifeng");
    snprintf(conf->auth.passwd, sizeof(conf->auth.passwd), "111111");

    snprintf(conf->ipaddr, sizeof(conf->ipaddr), "127.0.0.1");

    conf->send_thd_num = 1;
    conf->send_buff_size = 5 * MB;
    conf->recv_buff_size = 2 * MB;

    snprintf(conf->sendq.name, sizeof(conf->sendq.name), "../temp/sdtp/sdtp-ssvr.key");
    conf->sendq.size = 4096;
    conf->sendq.count = 2048;

    return 0;
}

/******************************************************************************
 **函数名称: agtd_sdtp_init
 **功    能: 初始化SDTP对象
 **输入参数: NONE
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 设置SDTP的配置信息
 **注意事项: TODO: 可改为使用配置文件的方式加载配置信息
 **作    者: # Qifeng.zou # 2015.05.28 22:58:17 #
 ******************************************************************************/
static sdtp_cli_t *agtd_sdtp_init(log_cycle_t *log)
{
    sdtp_ssvr_conf_t conf;

    agtd_sdtp_set_conf(&conf);

    return sdtp_cli_init(&conf, 0, log);
}

/******************************************************************************
 **函数名称: prob_init_req_list
 **功    能: 初始化请求列表
 **输入参数:
 **     agtd: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 构建平衡二叉树
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.28 23:41:39 #
 ******************************************************************************/
static int prob_init_req_list(agtd_cntx_t *agtd)
{
    int i;
    avl_opt_t opt;

    memset(&opt, 0, sizeof(opt));

    agtd->len = 10;
    agtd->serial_to_sck_map = (avl_tree_t **)calloc(agtd->len, sizeof(avl_tree_t *));
    if (NULL == agtd->serial_to_sck_map)
    {
        return PROB_ERR;
    }

    for (i=0; i<agtd->len; ++i)
    {
        opt.pool = (void *)NULL;
        opt.alloc = (mem_alloc_cb_t)mem_alloc;
        opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

        agtd->serial_to_sck_map[i] = avl_creat(&opt, (key_cb_t)avl_key_cb_int64, (avl_cmp_cb_t)avl_cmp_cb_int64);
        if (NULL == agtd->serial_to_sck_map[i])
        {
            return PROB_ERR;
        }
    }

    return PROB_OK;
}

/******************************************************************************
 **函数名称: prob_search_req_hdl
 **功    能: 搜索请求的处理函数
 **输入参数:
 **     type: 全局对象
 **     sdtp: SDTP客户端对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.28 23:11:54 #
 ******************************************************************************/
static int prob_search_req_hdl(unsigned int type, void *data, int length, void *args, log_cycle_t *log)
{
    int idx;
    mesg_search_req_t req;
    prob_flow_t *flow, *f;
    srch_mesg_body_t *body;
    prob_mesg_header_t *head;
    agtd_cntx_t *agtd = (agtd_cntx_t *)args;

    flow = (prob_flow_t *)data;
    head = (prob_mesg_header_t *)(flow + 1);
    body = (srch_mesg_body_t *)(head + 1);

    /* > 将流水信息插入请求列表 */
    f = (prob_flow_t *)malloc(sizeof(prob_flow_t));
    if (NULL == f)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return PROB_ERR;
    }

    memcpy(f, flow, sizeof(prob_flow_t));

    idx = flow->serial % agtd->len;

    if (avl_insert(agtd->serial_to_sck_map[idx], &flow->serial, sizeof(flow->serial), f))
    {
        free(f);
        log_error(log, "Insert into avl failed! idx:%d serial:%lu sck_serial:%lu prob_agt_idx:%d",
                idx, flow->serial, flow->sck_serial, flow->prob_agt_idx);
        return PROB_ERR;
    }

    /* > 转发搜索请求 */
    req.serial = flow->serial;
    memcpy(&req.body, body, sizeof(srch_mesg_body_t));

    return sdtp_cli_send(agtd->sdtp, type, &req, sizeof(req));
}

/******************************************************************************
 **函数名称: agtd_set_reg
 **功    能: 设置注册函数
 **输入参数:
 **     agtd: 全局信息
 **输出参数:
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.28 23:11:54 #
 ******************************************************************************/
static int agtd_set_reg(agtd_cntx_t *agtd)
{
    if (prob_register(agtd->prob, MSG_SEARCH_REQ, (prob_reg_cb_t)prob_search_req_hdl, (void *)agtd))
    {
        return PROB_ERR;
    }

    return PROB_OK;
}

/******************************************************************************
 **函数名称: agtd_init
 **功    能: 初始化进程
 **输入参数:
 **     pname: 进程名
 **     path: 配置路径
 **输出参数: NONE
 **返    回: 全局对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.28 23:11:54 #
 ******************************************************************************/
static agtd_cntx_t *agtd_init(char *pname, const char *path)
{
    sdtp_cli_t *sdtp;
    prob_cntx_t *prob;
    agtd_cntx_t *agtd;
    log_cycle_t *log;

    agtd = (agtd_cntx_t *)calloc(1, sizeof(agtd_cntx_t));
    if (NULL == agtd)
    {
        return NULL;
    }

    /* > 初始化日志 */
    log = agtd_init_log(pname);
    if (NULL == log)
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return NULL;
    }

    /* > 加载配置信息 */
    agtd->conf = agtd_conf_load(path, log);
    if (NULL == agtd->conf)
    {
        FREE(agtd);
        fprintf(stderr, "Load configuration failed!\n");
        return NULL;
    }

    /* > 初始化全局信息 */
    prob = prob_cntx_init(&agtd->conf->prob, log);
    if (NULL == prob)
    {
        fprintf(stderr, "Initialize search-engine failed!");
        return NULL;
    }

    /* > 初始化SDTP信息 */
    sdtp = agtd_sdtp_init(prob->log);
    if (NULL == sdtp)
    {
        fprintf(stderr, "Initialize sdtp failed!");
        return NULL;
    }

    /* > 初始化请求列表 */
    if (prob_init_req_list(agtd))
    {
        fprintf(stderr, "Initialize sdtp failed!");
        return NULL;
    }

    agtd->prob = prob;
    agtd->sdtp = sdtp;

    return agtd;
}
