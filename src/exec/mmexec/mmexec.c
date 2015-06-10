/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: mmexec.c
 ** 版本号: 1.0
 ** 描  述: 负责共享内存的创建
 **         因存在2个进程相互依赖对象的共享内存, 这样将导致进程无法正常启动, 通过
 **         第3方进行统一的共享内存创建将有效的解决以上问题的存在!
 ** 作  者: # Qifeng.zou # Thu 04 Jun 2015 04:58:32 PM CST #
 ******************************************************************************/

#include "shm_queue.h"

#define LSND_SHM_SENDQ_MAX      (1024)          /* 发送队列容量 */
#define LSND_SHM_SENDQ_SIZE     (4096)          /* 发送队列尺寸 */
#define LSND_SHM_SENDQ_PATH     "../temp/listend/send.shmq"  /* 发送队列路径 */

static int mm_lsnd_creat_sendq(void);

/* 主函数 */
int main(int argc, char *argv[])
{
    daemon(1, 1); /* 切后台运行 */

    /* > 创建Agentd发送队列 */
    if (mm_lsnd_creat_sendq())
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    while (1) { pause(); }

    return 0;
}

/******************************************************************************
 **函数名称: mm_lsnd_creat_sendq
 **功    能: 为侦听服务创建发送队列
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 通过路径创建共享内存队列
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-05 10:09:34 #
 ******************************************************************************/
static int mm_lsnd_creat_sendq(void)
{
    shm_queue_t *shmq;

    shmq = shm_queue_creat(LSND_SHM_SENDQ_PATH, LSND_SHM_SENDQ_MAX, LSND_SHM_SENDQ_SIZE);
    if (NULL == shmq)
    {
        return -1;
    }

    return 0;
}
