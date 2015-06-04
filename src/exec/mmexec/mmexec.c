/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: mmexec.c
 ** 版本号: 1.0
 ** 描  述: 负责共享内存的创建
 **         因存在2个进程相互依赖对象的共享内存, 这样将导致进程无法正常启动, 通过第3方进行
 **         统一的共享内存创建将有效的解决以上问题的存在!
 ** 作  者: # Qifeng.zou # Thu 04 Jun 2015 04:58:32 PM CST #
 ******************************************************************************/

/* 主函数 */
int main(int argc, char *argv[])
{
    shm_queue_t *shmq;
    char path[FILE_PATH_MAX_LEN];

    /* > 创建共享内存 */
    snprintf(path, sizeof(path), "../temp/");

    shmq = shm_queue_creat_ex(path, max, size);
    if (NULL == shmq)
    {
        return -1;
    }
    return 0;
}
