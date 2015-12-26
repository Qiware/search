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

/******************************************************************************
 **函数名称: wiov_item_clear
 **功    能: 清理wiov对象中的空间
 **输入参数:
 **     wiov: IOV对象
 **输出参数: NONE
 **返    回: 
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.12.26 07:53:50 #
 ******************************************************************************/
void wiov_item_clear(wiov_t *wiov)
{
    int idx;

    for (idx=wiov->iov_idx; idx<wiov->iov_cnt; ++idx) {
        wiov->orig[idx].dealloc(wiov->orig[idx].pool, wiov->orig[idx].addr);
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
 **实现描述: 释放数据发送完毕后的内存!
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
            wiov->orig[idx].off = (n - len);
            break;
        }
    }

    if (idx == wiov->iov_cnt) {
        wiov->iov_idx = 0;
        wiov->iov_cnt = 0;
    }

    return 0;
}
