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
#include "lsnd_conf.h"

static int lsnd_mem_creat(void);

/* 主函数 */
int main(int argc, char *argv[])
{
    daemon(1, 1); /* 切后台运行 */

    /* > 创建侦听服务内存 */
    if (lsnd_mem_creat())
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    while (1) { pause(); }

    return 0;
}

/******************************************************************************
 **函数名称: lsnd_mem_creat
 **功    能: 创建侦听服务内存
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 通过配置文件创建内存资源
 **注意事项: 
 **作    者: # Qifeng.zou # 2015-06-05 10:09:34 #
 ******************************************************************************/
static int lsnd_mem_creat(void)
{
#define LSND_SHM_DISTQ_MAX      (1024)          /* 分发队列容量 */
#define LSND_SHM_DISTQ_SIZE     (4096)          /* 分发队列尺寸 */
    lsnd_conf_t conf;
    shm_queue_t *shmq;
    char path[FILE_PATH_MAX_LEN];

    /* > 加载侦听配置 */
    if (lsnd_load_conf("../conf/listend.xml", &conf, NULL))
    {
        fprintf(stderr, "Load listen configuration failed!\n");
        return -1;
    }

    /* > 创建共享内存队列 */
    snprintf(path, sizeof(path), "%s/dist.shmq", conf.wdir);

    shmq = shm_queue_creat(path, LSND_SHM_DISTQ_MAX, LSND_SHM_DISTQ_SIZE);
    if (NULL == shmq)
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    return 0;
}
