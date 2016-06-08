/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: invtd_mesg.c
 ** 版本号: 1.0
 ** 描  述: 倒排服务与消息处理相关内容
 ** 作  者: # Qifeng.zou # Fri 08 May 2015 10:37:21 PM CST #
 ******************************************************************************/
#include "mesg.h"
#include "invertd.h"
#include "xml_tree.h"
#include "rtmq_recv.h"
#include "invtd_mesg.h"

/******************************************************************************
 **函数名称: invtd_print_invt_tab_req_hdl
 **功    能: 处理打印倒排表的请求
 **输入参数:
 **     type: 消息类型
 **     orig: 源设备ID
 **     buff: 搜索请求的数据
 **     len: 数据长度
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
static int invtd_print_invt_tab_req_hdl(int type, int orig, char *buff, size_t len, void *args)
{
    return INVT_OK;
}

/******************************************************************************
 **函数名称: invtd_rtmq_reg
 **功    能: 注册RTMQ回调
 **输入参数:
 **     ctx: SDTP对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 进行回调函数的注册
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
static int invtd_rtmq_reg(invtd_cntx_t *ctx)
{
#define INVTD_RTMQ_REG(ctx, type, proc, args) \
    if (rtmq_proxy_reg_add((ctx)->frwder, type, proc, args)) { \
        log_error(ctx->log, "Register callback failed!"); \
        return INVT_ERR; \
    }

   INVTD_RTMQ_REG(ctx, MSG_SEARCH_REQ, invtd_search_req_hdl, ctx);
   INVTD_RTMQ_REG(ctx, MSG_INSERT_WORD_REQ, invtd_insert_word_req_hdl, ctx);
   INVTD_RTMQ_REG(ctx, MSG_PRINT_INVT_TAB_REQ, invtd_print_invt_tab_req_hdl, ctx);

    return INVT_OK;
}

/******************************************************************************
 **函数名称: invtd_start_frwder
 **功    能: 启动DownStream服务
 **输入参数:
 **     ctx: SDTP对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 进行回调函数的注册, 并启动SDTP服务
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.08 #
 ******************************************************************************/
int invtd_start_frwder(invtd_cntx_t *ctx)
{
    if (invtd_rtmq_reg(ctx)) {
        return INVT_ERR;
    }

    return rtmq_proxy_launch(ctx->frwder);
}
