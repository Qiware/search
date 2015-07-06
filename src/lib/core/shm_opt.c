#include "shm_opt.h"
#include "syscall.h"
#include "xml_tree.h"

static void *shm_at_or_creat(const char *path, size_t size);

/******************************************************************************
 **函数名称: shm_data_read
 **功    能: 读取SHM数据
 **输入参数: 
 **     path: 共享内存路径
 **输出参数:
 **     shm: 共享内存数据
 **返    回: 0:成功 !0:失败
 **实现描述: 读取文件中存储的共享内存数据
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.06 #
 ******************************************************************************/
static int shm_data_read(const char *path, shm_data_t *shm)
{
    xml_opt_t opt;
    xml_tree_t *xml;
    xml_node_t *node;

    memset(&opt, 0, sizeof(opt));

    opt.log = NULL;
    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    xml = xml_creat(path, &opt);
    if (NULL == xml)
    {
        return -1;
    }

    /* > 获取SHM-ID */
    node = xml_query(xml, ".SHM.ID");
    if (NULL == node
        || 0 == node->value.len)
    {
        xml_destroy(xml);
        return -1;
    }

    shm->shmid = atoi(node->value.str);

    /* > 获取SIZE */
    node = xml_query(xml, ".SHM.SIZE");
    if (NULL == node
        || 0 == node->value.len)
    {
        xml_destroy(xml);
        return -1;
    }

    shm->size = atoi(node->value.str);

    /* > 获取校验值 */
    node = xml_query(xml, ".SHM.CHECKSUM");
    if (NULL == node
        || 0 == node->value.len)
    {
        xml_destroy(xml);
        return -1;
    }

    shm->checksum = atoi(node->value.str);

    xml_destroy(xml);

    return 0;
}

/******************************************************************************
 **函数名称: shm_data_write
 **功    能: 写入SHM数据
 **输入参数: 
 **     path: 共享内存路径
 **     shm: 共享内存数据
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 将共享内存数据写入文件
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.06.06 #
 ******************************************************************************/
static int shm_data_write(const char *path, shm_data_t *shm)
{
    xml_opt_t opt;
    xml_tree_t *xml;
    xml_node_t *node;
    char value[INT_MAX_LEN];

    memset(&opt, 0, sizeof(opt));

    opt.log = NULL;
    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    xml = xml_screat("", &opt);
    if (NULL == xml)
    {
        return -1;
    }

    do
    {
        /* > 根节点 */
        node = xml_add_child(xml, xml->root, "SHM", NULL);
        if (NULL == node)
        {
            break;
        }

        /* > 新建SHM-ID */
        snprintf(value, sizeof(value), "%d", shm->shmid);

        if (!xml_add_child(xml, node, "ID", value))
        {
            break;
        }

        /* > 新建SHM-SIZE */
        snprintf(value, sizeof(value), "%ld", shm->size);

        if (!xml_add_child(xml, node, "SIZE", value))
        {
            break;
        }

        /* > 新建校验值 */
        snprintf(value, sizeof(value), "%d", shm->checksum);

        if (!xml_add_child(xml, node, "CHECKSUM", value))
        {
            break;
        }

        xml_fwrite(xml, path);

        xml_destroy(xml);

        return 0;
    } while(0);

    xml_destroy(xml);

    return -1;
}

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
    void *addr;
    shm_data_t shm;

    memset(&shm, 0, sizeof(shm));

    /* > 创建共享内存 */
    shm.shmid = shmget(IPC_PRIVATE, size, IPC_CREAT|0666);
    if (shm.shmid < 0)
    {
        return NULL;
    }

    shm.size = size;
    shm.checksum = SHM_CHECK_SUM;

    /* > ATTACH共享内存 */
    addr = (void *)shmat(shm.shmid, NULL, 0);
    if ((void *)-1 == addr)
    {
        return NULL;
    }

    memset(addr, 0, size);

    shmctl(shm.shmid, IPC_RMID, NULL);

    /* 写入SHM数据 */
    shm_data_write(path, &shm);

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
    else if (0 == st.st_size)
    {
        return _shm_creat(path, size);  /* 创建 */
    }

    return shm_at_or_creat(path, size);  /* 附着 */
}

/******************************************************************************
 **函数名称: shm_creat_by_key
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
void *shm_creat_by_key(int key, size_t size)
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
    void *addr;
    shm_data_t shm;

    /* 加载SHM数据 */
    if (shm_data_read(path, &shm))
    {
        return NULL;
    }

    /* 验证合法性 */
    if (SHM_DATA_INVALID(&shm)
        || ((0 != size) && (shm.size != size)))
    {
        return NULL;
    }
 
    /* > 已创建: 直接附着 */
    addr = shmat(shm.shmid, NULL, 0);
    if ((void *)-1 == addr)
    {
        return NULL;
    }

    shmctl(shm.shmid, IPC_RMID, NULL);

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
void *shm_attach_by_key(key_t key, size_t size)
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

    return addr;
}

/******************************************************************************
 **函数名称: shm_at_or_creat
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
static void *shm_at_or_creat(const char *path, size_t size)
{
    void *addr;
    shm_data_t shm;

    /* 加载SHM数据 */
    if (shm_data_read(path, &shm))
    {
        return NULL;
    }

    /* 验证合法性 */
    if (SHM_DATA_INVALID(&shm)
        || ((0 != size) && (shm.size != size)))
    {
        return NULL;
    }
 
    /* > 直接附着 */
    addr = shmat(shm.shmid, NULL, 0);
    if ((void *)-1 == addr)
    {
        if ((EIDRM == errno)
            || (EINVAL == errno))
        {
            return _shm_creat(path, size); /* 未创建 */
        }
        return NULL;
    }

    shmctl(shm.shmid, IPC_RMID, NULL);

    return addr;
}
