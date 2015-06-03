/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: agentd.c
 ** 版本号: 1.0
 ** 描  述: 代理服务
 **         负责接受外界请求，并将处理结果返回给外界
 ** 作  者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/

#include "sck.h"
#include "lock.h"
#include "hash.h"
#include "mesg.h"
#include "gate.h"
#include "agentd.h"
#include "syscall.h"
#include "gate_mesg.h"

#define AGTD_PROC_LOCK_PATH "../temp/agtd/agtd.lck"

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
    if (gate_startup(agtd->gate))
    {
        fprintf(stderr, "Startup search-engine failed!");
        goto ERROR;
    }

    while (1) { pause(); }

ERROR:
    /* 4. 销毁全局信息 */
    gate_cntx_destroy(agtd->gate);

    return -1;
}

/******************************************************************************
 **函数名称: agtd_proc_lock
 **功    能: 代理服务进程锁(防止同时启动两个服务进程)
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.15 #
 ******************************************************************************/
static int agtd_proc_lock(void)
{
    int fd;
    char path[FILE_PATH_MAX_LEN];

    /* 1. 获取路径 */
    snprintf(path, sizeof(path), "%s", AGTD_PROC_LOCK_PATH);

    Mkdir2(path, DIR_MODE);

    /* 2. 打开文件 */
    fd = Open(path, OPEN_FLAGS, OPEN_MODE);
    if (fd < 0)
    {
        return -1;
    }

    /* 3. 尝试加锁 */
    if (proc_try_wrlock(fd) < 0)
    {
        CLOSE(fd);
        return -1;
    }

    return 0;
}

/* 初始化SDTP对象 */
static sdtp_cli_t *agtd_sdtp_init(sdtp_ssvr_conf_t *conf, log_cycle_t *log)
{
    return sdtp_cli_init(conf, 0, log);
}

/******************************************************************************
 **函数名称: agtd_serial_to_sck_map_init
 **功    能: 初始化请求列表
 **输入参数:
 **     agtd: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 构建平衡二叉树
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.28 23:41:39 #
 ******************************************************************************/
static int agtd_serial_to_sck_map_init(agtd_cntx_t *agtd)
{
    int i;
    avl_opt_t opt;

    memset(&opt, 0, sizeof(opt));

    agtd->len = 10;
    agtd->serial_to_sck_map = (avl_tree_t **)calloc(agtd->len, sizeof(avl_tree_t *));
    if (NULL == agtd->serial_to_sck_map)
    {
        return AGTD_ERR;
    }

    for (i=0; i<agtd->len; ++i)
    {
        opt.pool = (void *)NULL;
        opt.alloc = (mem_alloc_cb_t)mem_alloc;
        opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

        agtd->serial_to_sck_map[i] = avl_creat(&opt, (key_cb_t)avl_key_cb_int64, (avl_cmp_cb_t)avl_cmp_cb_int64);
        if (NULL == agtd->serial_to_sck_map[i])
        {
            return AGTD_ERR;
        }
    }

    return AGTD_OK;
}

/******************************************************************************
 **函数名称: agtd_search_req_hdl
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
static int agtd_search_req_hdl(unsigned int type, void *data, int length, void *args, log_cycle_t *log)
{
    int idx;
    mesg_search_req_t req;
    gate_flow_t *flow, *f;
    srch_mesg_body_t *body;
    gate_mesg_header_t *head;
    agtd_cntx_t *agtd = (agtd_cntx_t *)args;

    flow = (gate_flow_t *)data;
    head = (gate_mesg_header_t *)(flow + 1);
    body = (srch_mesg_body_t *)(head + 1);

    /* > 将流水信息插入请求列表 */
    f = (gate_flow_t *)calloc(1, sizeof(gate_flow_t));
    if (NULL == f)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return AGTD_ERR;
    }

    memcpy(f, flow, sizeof(gate_flow_t));

    idx = flow->serial % agtd->len;

    if (avl_insert(agtd->serial_to_sck_map[idx], &flow->serial, sizeof(flow->serial), f))
    {
        free(f);
        log_error(log, "Insert into avl failed! idx:%d serial:%lu sck_serial:%lu gate_agt_idx:%d",
                idx, flow->serial, flow->sck_serial, flow->gate_agt_idx);
        return AGTD_ERR;
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
    if (gate_register(agtd->gate, MSG_SEARCH_REQ, (gate_reg_cb_t)agtd_search_req_hdl, (void *)agtd))
    {
        return AGTD_ERR;
    }

    return AGTD_OK;
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
    log_cycle_t *log;
    sdtp_cli_t *sdtp;
    gate_cntx_t *gate;
    agtd_cntx_t *agtd;

    /* > 加进程锁 */
    if (agtd_proc_lock())
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return NULL;
    }

    /* > 创建全局对象 */
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

    agtd->log = log;

    /* > 加载配置信息 */
    agtd->conf = agtd_conf_load(path, log);
    if (NULL == agtd->conf)
    {
        FREE(agtd);
        fprintf(stderr, "Load configuration failed!\n");
        return NULL;
    }

    /* > 初始化全局信息 */
    gate = gate_cntx_init(&agtd->conf->gate, log);
    if (NULL == gate)
    {
        fprintf(stderr, "Initialize search-engine failed!");
        return NULL;
    }

    /* > 初始化SDTP信息 */
    sdtp = agtd_sdtp_init(&agtd->conf->sdtp, log);
    if (NULL == sdtp)
    {
        fprintf(stderr, "Initialize sdtp failed!");
        return NULL;
    }

    /* > 初始化序列号->SCK映射表 */
    if (agtd_serial_to_sck_map_init(agtd))
    {
        fprintf(stderr, "Initialize sdtp failed!");
        return NULL;
    }

    agtd->gate = gate;
    agtd->sdtp = sdtp;

    return agtd;
}
