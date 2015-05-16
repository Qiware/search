#include "sdtp.h"
#include "sdtp_cmd.h"
#include "sdtp_priv.h"

/******************************************************************************
 **函数名称: sdtp_cmd_to_rsvr
 **功    能: 发送命令到指定的接收线程
 **输入参数: 
 **     ctx: 全局对象
 **     cmd_sck_id: 命令套接字
 **     cmd: 处理命令
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 随机选择接收线程
 **     2. 发送命令至接收线程
 **注意事项: 如果发送失败，最多重复3次发送!
 **作    者: # Qifeng.zou # 2015.01.09 #
 ******************************************************************************/
int sdtp_cmd_to_rsvr(sdtp_rctx_t *ctx, int cmd_sck_id, const sdtp_cmd_t *cmd, int idx)
{
    char path[FILE_PATH_MAX_LEN];
    sdtp_conf_t *conf = &ctx->conf;

    sdtp_rsvr_usck_path(conf, path, idx);

    /* 发送命令至接收线程 */
    if (unix_udp_send(cmd_sck_id, path, cmd, sizeof(sdtp_cmd_t)) < 0)
    {
        log_error(ctx->log, "errmsg:[%d] %s! path:%s type:%d",
                errno, strerror(errno), path, cmd->type);
        return SDTP_ERR;
    }

    return SDTP_OK;
}
