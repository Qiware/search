/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: wiov.c
 ** 版本号: 1.0
 ** 描  述:
 ** 作  者: # Qifeng.zou # 2015年12月26日 星期六 08时10分29秒 #
 ******************************************************************************/
#include "comm.h"
#include "iovec.h"
#include <limits.h>

/******************************************************************************
 **函数名称: wiov_init
 **功    能: 初始化wiov对象
 **输入参数:
 **     max: 最大写入长度
 **输出参数:
 **     wiov: IOV对象
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **     1. orig和iov成员的长度一致, 且一一对应.
 **     2. 当writev()中iov个数超过IOV_MAX时, 会出现"Invalid argument"的提示.
 **        Linux系统中IOV_MAX的值为1024.
 **作    者: # Qifeng.zou # 2015.12.26 23:42:54 #
 ******************************************************************************/
int wiov_init(wiov_t *wiov, int max)
{
    wiov->iov_cnt = 0;
    wiov->iov_idx = 0;
    wiov->iov_max = (max > IOV_MAX? IOV_MAX : max);

    wiov->orig = (wiov_orig_t *)calloc(wiov->iov_max, sizeof(wiov_orig_t));
    if (NULL == wiov->orig) {
        return -1;
    }

    wiov->iov = (struct iovec *)calloc(wiov->iov_max, sizeof(struct iovec));
    if (NULL == wiov->iov) {
        free(wiov->orig);
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: wiov_destroy
 **功    能: 销毁wiov对象
 **输入参数: NONE
 **输出参数:
 **     wiov: IOV对象
 **返    回: 0:成功 !0:失败
 **实现描述: 释放wiov对象成员的内存空间
 **注意事项: 释放已加入iov对象的各项空间
 **作    者: # Qifeng.zou # 2015.12.26 23:42:54 #
 ******************************************************************************/
void wiov_destroy(wiov_t *wiov)
{
    wiov_clean(wiov);

    free(wiov->iov);
    free(wiov->orig);

    wiov->iov_cnt = 0;
    wiov->iov_idx = 0;
    wiov->iov_max = 0;
}

/******************************************************************************
 **函数名称: wiov_clean
 **功    能: 清理wiov对象中的空间
 **输入参数:
 **     wiov: IOV对象
 **输出参数: NONE
 **返    回:
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.12.26 07:53:50 #
 ******************************************************************************/
void wiov_clean(wiov_t *wiov)
{
    int idx;

    for (idx=wiov->iov_idx; idx<wiov->iov_cnt; ++idx) {
        wiov->orig[idx].dealloc(wiov->orig[idx].pool, wiov->orig[idx].addr);
        wiov_item_reset(wiov, idx);
    }

    wiov->iov_idx = 0;
    wiov->iov_cnt = 0;
}

/******************************************************************************
 **函数名称: wiov_item_adjust
 **功    能: 校正&删除已发送数据
 **输入参数:
 **     wiov: IOV对象
 **     n: 发送长度
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 释放发送完毕数据的内存
 **注意事项:
 **作    者: # Qifeng.zou # 2015.12.26 06:18:03 #
 ******************************************************************************/
int wiov_item_adjust(wiov_t *wiov, size_t n)
{
    int idx;
    size_t len;

    len = 0;
    for (idx=wiov->iov_idx; idx<wiov->iov_cnt; ++idx, ++wiov->iov_idx) {
        if (len + wiov->iov[idx].iov_len <= n) {
            len += wiov->iov[idx].iov_len;
            wiov->orig[idx].dealloc(wiov->orig[idx].pool, wiov->orig[idx].addr);
            wiov_item_reset(wiov, idx);
        }
        else {
            wiov->iov[idx].iov_base += (n - len);
            wiov->iov[idx].iov_len = (len + wiov->iov[idx].iov_len - n);
            break;
        }
    }

    if (idx == wiov->iov_cnt) {
        wiov->iov_idx = 0;
        wiov->iov_cnt = 0;
    }

    return 0;
}
