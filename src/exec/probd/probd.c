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
#include "syscall.h"
#include "sdtp_cli.h"
#include "prob_mesg.h"
#include "prob_worker.h"

#define PROB_PROC_LOCK_PATH "../temp/srch/srch.lck"

typedef struct
{
    prob_cntx_t *srch;                  /* 搜索服务 */
    sdtp_cli_t *sdtp;                   /* SDTP服务 */

    int len;                            /* 业务请求树长 */
    avl_tree_t **req_list;              /* 业务请求信息表 */
} prob_proc_t;

static prob_proc_t *prob_proc_init(char *pname, const char *path);
static int prob_set_reg(prob_proc_t *proc);

/******************************************************************************
 **函数名称: main 
 **功    能: 搜索引擎主程序
 **输入参数: 
 **     argc: 参数个数
 **     argv: 参数列表
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 加载搜索引擎配置
 **     1. 初始化搜索引擎信息
 **     2. 启动搜索引擎功能
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
int main(int argc, char *argv[])
{
    prob_opt_t opt;
    prob_proc_t *proc;

    memset(&opt, 0, sizeof(opt));

    /* > 解析输入参数 */
    if (prob_getopt(argc, argv, &opt))
    {
        return prob_usage(argv[0]);
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
    proc = prob_proc_init(argv[0], opt.conf_path);
    if (NULL == proc)
    {
        fprintf(stderr, "Initialize proc failed!");
        return PROB_ERR;
    }
 
    /* 注册回调函数 */
    if (prob_set_reg(proc))
    {
        fprintf(stderr, "Set register callback failed!");
        return PROB_ERR;
    }

    /* 3. 启动爬虫服务 */
    if (prob_startup(proc->srch))
    {
        fprintf(stderr, "Startup search-engine failed!");
        goto ERROR;
    }

    while (1) { pause(); }

ERROR:
    /* 4. 销毁全局信息 */
    prob_cntx_destroy(proc->srch);

    return PROB_ERR;
}

/******************************************************************************
 **函数名称: prob_sdtp_set_conf
 **功    能: 设置SDTP配置信息
 **输入参数: NONE
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 设置SDTP的配置信息
 **注意事项: TODO: 可改为使用配置文件的方式加载配置信息
 **作    者: # Qifeng.zou # 2015.05.28 22:58:17 #
 ******************************************************************************/
static int prob_sdtp_set_conf(sdtp_ssvr_conf_t *conf)
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

    return PROB_OK;
}

/******************************************************************************
 **函数名称: prob_sdtp_init
 **功    能: 初始化SDTP对象
 **输入参数: NONE
 **输出参数:
 **     conf: 配置信息
 **返    回: 0:成功 !0:失败
 **实现描述: 设置SDTP的配置信息
 **注意事项: TODO: 可改为使用配置文件的方式加载配置信息
 **作    者: # Qifeng.zou # 2015.05.28 22:58:17 #
 ******************************************************************************/
static sdtp_cli_t *prob_sdtp_init(log_cycle_t *log)
{
    sdtp_ssvr_conf_t conf;

    prob_sdtp_set_conf(&conf);

    return sdtp_cli_init(&conf, 0, log);
}

/******************************************************************************
 **函数名称: prob_init_req_list
 **功    能: 初始化请求列表
 **输入参数:
 **     proc: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 构建平衡二叉树
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.28 23:41:39 #
 ******************************************************************************/
static int prob_init_req_list(prob_proc_t *proc)
{
    int i;
    avl_opt_t opt;

    memset(&opt, 0, sizeof(opt));

    proc->len = 10;
    proc->req_list = (avl_tree_t **)calloc(proc->len, sizeof(avl_tree_t *));
    if (NULL == proc->req_list)
    {
        return PROB_ERR;
    }

    for (i=0; i<proc->len; ++i)
    {
        opt.pool = (void *)NULL;
        opt.alloc = (mem_alloc_cb_t)mem_alloc;
        opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

        proc->req_list[i] = avl_creat(&opt, (key_cb_t)avl_key_cb_int64, (avl_cmp_cb_t)avl_cmp_cb_int64);
        if (NULL == proc->req_list[i])
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
    prob_proc_t *proc = (prob_proc_t *)args;

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

    idx = flow->serial % proc->len;

    if (avl_insert(proc->req_list[idx], &flow->serial, sizeof(flow->serial), f))
    {
        free(f);
        log_error(log, "Insert into avl failed! idx:%d serial:%lu sck_serial:%lu prob_agt_idx:%d",
                idx, flow->serial, flow->sck_serial, flow->prob_agt_idx);
        return PROB_ERR;
    }

    /* > 转发搜索请求 */
    req.serial = flow->serial;
    memcpy(&req.body, body, sizeof(srch_mesg_body_t));

    return sdtp_cli_send(proc->sdtp, type, &req, sizeof(req));
}

/******************************************************************************
 **函数名称: prob_set_reg
 **功    能: 设置注册函数
 **输入参数:
 **     proc: 全局信息
 **输出参数:
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.28 23:11:54 #
 ******************************************************************************/
static int prob_set_reg(prob_proc_t *proc)
{
    if (prob_register(proc->srch, MSG_SEARCH_REQ, (prob_reg_cb_t)prob_search_req_hdl, (void *)proc))
    {
        return PROB_ERR;
    }

    return PROB_OK;
}

/******************************************************************************
 **函数名称: prob_proc_init
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
static prob_proc_t *prob_proc_init(char *pname, const char *path)
{
    sdtp_cli_t *sdtp;
    prob_cntx_t *srch;
    prob_proc_t *proc;

    proc = (prob_proc_t *)calloc(1, sizeof(prob_proc_t));
    if (NULL == proc)
    {
        return NULL;
    }

    /* > 初始化全局信息 */
    srch = prob_cntx_init(pname, path);
    if (NULL == srch)
    {
        fprintf(stderr, "Initialize search-engine failed!");
        return NULL;
    }

    /* > 初始化SDTP信息 */
    sdtp = prob_sdtp_init(srch->log);
    if (NULL == sdtp)
    {
        fprintf(stderr, "Initialize sdtp failed!");
        return NULL;
    }

    /* > 初始化请求列表 */
    if (prob_init_req_list(proc))
    {
        fprintf(stderr, "Initialize sdtp failed!");
        return NULL;
    }

    proc->srch = srch;
    proc->sdtp = sdtp;

    return proc;

}
