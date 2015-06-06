#include "shm_opt.h"
#include "syscall.h"

static void *shm_creat_ex(int key, size_t size);
static void *shm_attach_ex(key_t key, size_t size);

/******************************************************************************
 **函数名称: shm_ftok
 **功    能: 通过文件产生共享内存KEY值
 **输入参数: 
 **     path: 文件路径
 **     id: 编号
 **输出参数: NONE
 **返    回: KEY值
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.19 #
 ******************************************************************************/
key_t shm_ftok(const char *path, int id)
{
    FILE *fp;

    if (access(path, F_OK))
    {
        Mkdir2(path, 0777);

        fp = fopen(path, "w");
        if (NULL == fp)
        {
            return -1;
        }

        FCLOSE(fp);
    }
    
    return ftok(path, id);
}

/* 创建不存在的共享内存 */
static void *_shm_creat(const char *path, size_t size)
{
    FILE *fp;
    void *addr;
    shm_data_t shm;

    /* > 生成KEY值 */
    shm.key = shm_ftok(path, 0);
    if (-1 == shm.key)
    {
        return NULL;
    }
    shm.size = size;
    shm.checksum = SHM_CHECK_SUM;

    /* > 创建共享内存 */
    addr = shm_creat_ex(shm.key, shm.size);
    if (NULL == addr)
    {
        return NULL;
    }

    /* 写入SHM数据 */
    fp = fopen(path, "w");
    if (NULL == fp)
    {
        return NULL;
    }

    fwrite(&shm, sizeof(shm), 1, fp);

    fclose(fp);

    return addr;
}

/******************************************************************************
 **函数名称: shm_creat
 **功    能: 创建共享内存
 **输入参数: 
 **     path: 共享内存路径
 **     size: 共享内存大小
 **输出参数: NONE
 **返    回: 内存地址
 **实现描述: 
 **注意事项: 共享内存不存在的话, 则进行创建; 存在的话, 则进行附着!
 **作    者: # Qifeng.zou # 2015.06.07 00:05:15 #
 ******************************************************************************/
void *shm_creat(const char *path, size_t size)
{
    struct stat st;

    Mkdir2(path, DIR_MODE);

    /* > 判断文件大小, 及是否存在 */
    if (lstat(path, &st) < 0)
    {
        if (ENOENT != errno)
        {
            return NULL;
        }
        return _shm_creat(path, size);  /* 创建 */
    }

    if (0 != st.st_size
        || st.st_size != sizeof(shm_data_t))
    {
        return NULL;
    }

    return shm_attach(path, size);   /* 附着 */
}

/******************************************************************************
 **函数名称: shm_creat_ex
 **功    能: 创建共享内存
 **输入参数: 
 **     key: KEY值
 **     size: 空间SIZE
 **输出参数: NONE
 **返    回: 内存地址
 **实现描述: 
 **注意事项: 
 **     如果创建共享内存时出现错误，可能是因为SHMMAX限制了共享内存的大小.
 **     SHMMAX的默认值为32MB, 可在/proc/sys/kernel/shmmax中查看到SHMMAX的值
 **作    者: # Qifeng.zou # 2014.09.08 #
 ******************************************************************************/
static void *shm_creat_ex(int key, size_t size)
{
    int shmid;
    void *addr;

    /* 1 判断是否已经创建 */
    shmid = shmget(key, 0, 0);
    if (shmid >= 0)
    {
        return NULL;  /* 已创建 */
    }

    /* 2 异常，则退出处理 */
    if (ENOENT != errno)
    {
        return NULL;
    }

    /* 3 创建共享内存 */
    shmid = shmget(key, size, IPC_CREAT|0666);
    if (shmid < 0)
    {
        return NULL;
    }

    /* 4. ATTACH共享内存 */
    addr = (void *)shmat(shmid, NULL, 0);
    if ((void *)-1 == addr)
    {
        return NULL;
    }

    memset(addr, 0, size);

    shmctl(shmid, IPC_RMID, NULL);

    return addr;
}

/******************************************************************************
 **函数名称: shm_attach
 **功    能: 附着共享内存
 **输入参数: 
 **     path: 共享内存路径
 **     size: 共享内存大小(0: 默认值)
 **输出参数: NONE
 **返    回: 内存地址
 **实现描述: 从文件中读取共享内存数据, 再附着到指定共享内存!
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.06 #
 ******************************************************************************/
void *shm_attach(const char *path, size_t size)
{
    FILE *fp;
    int shmid;
    void *addr;
    shm_data_t shm;

    memset(&shm, 0, sizeof(shm));

    /* 加载SHM数据 */
    fp = fopen(path, "r");
    if (NULL == fp)
    {
        return NULL;
    }

    fread(&shm, sizeof(shm), 1, fp);

    fclose(fp);

    /* 验证合法性 */
    if (SHM_DATA_INVALID(&shm)
        || ((0 != size) && (shm.size != size)))
    {
        return NULL;
    }
 
    /* > 判断是否已经创建 */
    shmid = shmget(shm.key, 0, 0666);
    if (shmid < 0)
    {
        return NULL;
    }

    /* > 已创建: 直接附着 */
    addr = shmat(shmid, NULL, 0);
    if ((void *)-1 == addr)
    {
        return NULL;
    }

    shmctl(shmid, IPC_RMID, NULL);

    return addr;
}

/******************************************************************************
 **函数名称: shm_attach_ex
 **功    能: 附着共享内存
 **输入参数: 
 **     key: KEY值
 **输出参数: NONE
 **返    回: 内存地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.22 #
 ******************************************************************************/
static void *shm_attach_ex(key_t key, size_t size)
{
    int shmid;
    void *addr;

    /* > 判断是否已经创建 */
    shmid = shmget(key, 0, 0666);
    if (shmid < 0)
    {
        return NULL;
    }

    /* > 已创建: 直接附着 */
    addr = shmat(shmid, NULL, 0);
    if ((void *)-1 == addr)
    {
        return NULL;
    }

    //shmctl(shmid, IPC_RMID, NULL);

    return addr;
}
