/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: copy.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # Mon 27 Jul 2015 07:08:59 PM CST #
 ******************************************************************************/

#include <malloc.h>
#include "comm.h"
#include "redo.h"
#include "thread_pool.h"

#define CP_THD_NUM      (4)         /* 线程数 */
#define CP_SLOT_SIZE    (32 * MB)    /* 每次拷贝大小 */

typedef struct
{
    thread_pool_t *tpool;           /* 线程池 */

    char src[FILE_PATH_MAX_LEN];    /* 源路径 */
    char dst[FILE_PATH_MAX_LEN];    /* 目的路径 */

    struct stat fst;                /* 源文件大小 */
    int complete;                   /* 完成计数(当complete==tpool->num时, 表示结束) */
} cp_cntx_t;

void *cp_copy_routine(void *_ctx)
{
    int tid, fd, to;
    ssize_t n, size;
    off_t off;
    void *buff;
    cp_cntx_t *ctx = (cp_cntx_t *)_ctx;

    tid = thread_pool_get_tidx(ctx->tpool);

    if (posix_memalign(&buff, 4*KB, CP_SLOT_SIZE))
    {
        fprintf(stderr, "%s:[%d] errmsg:[%d] %s!\n", __FILE__, __LINE__, errno, strerror(errno));
        pthread_exit((void *)-1);
        return (void *)-1;
    }

    fd = Open(ctx->src, O_RDONLY|O_DIRECT, OPEN_MODE);
    if (fd < 0)
    {
        fprintf(stderr, "%s:[%d] errmsg:[%d] %s!\n", __FILE__, __LINE__, errno, strerror(errno));
        pthread_exit((void *)-1);
        return (void *)-1;
    }

    to = Open(ctx->dst, O_CREAT|O_RDWR|O_DIRECT, OPEN_MODE);
    if (to < 0)
    {
        fprintf(stderr, "%s:[%d] errmsg:[%d] %s!\n", __FILE__, __LINE__, errno, strerror(errno));
        pthread_exit((void *)-1);
        return (void *)-1;
    }

    off = tid * CP_SLOT_SIZE;
    while (1)
    {
        if (off > ctx->fst.st_size)
        {
            break;
        }
        else if (off + CP_SLOT_SIZE > ctx->fst.st_size)
        {
            size = ctx->fst.st_size - off;
        }
        else
        {
            size = CP_SLOT_SIZE;
        }

        lseek(fd, off, SEEK_SET);

        n = Readn(fd, buff, size);
        if (n != size)
        {
            fprintf(stderr, "%s:[%d] errmsg:[%d] %s!\n", __FILE__, __LINE__, errno, strerror(errno));
            break;
        }

        lseek(to, off, SEEK_SET);

        n = Writen(to, buff, size);
        if (n != size)
        {
            fprintf(stderr, "%s:[%d] errmsg:[%d] %s!\n", __FILE__, __LINE__, errno, strerror(errno));
            break;
        }

        off += (ctx->tpool->num * CP_SLOT_SIZE);
    }

    CLOSE(fd);
    CLOSE(to);

    ++ctx->complete;
    if (ctx->complete == ctx->tpool->num)
    {
        exit(0);
    }

    return (void *)0;
}

int main(int argc, char *argv[])
{
    int idx, thd_num;
    cp_cntx_t *ctx;
    thread_pool_opt_t opt;

    if (4 != argc)
    {
        fprintf(stderr, "Paramter isn't right!\n");
        return -1;
    }

    //nice(-20);

    thd_num = atoi(argv[3]);
    if (thd_num <= 0)
    {
        thd_num = CP_THD_NUM;
    }

    /* > 初始化处理 */
    ctx = (cp_cntx_t *)calloc(1, sizeof(cp_cntx_t));
    if (NULL == ctx)
    {
        fprintf(stderr, "%s:[%d] errmsg:[%d] %s!\n", __FILE__, __LINE__, errno, strerror(errno));
        return -1;
    }

    snprintf(ctx->src, sizeof(ctx->src), "%s", argv[1]);
    snprintf(ctx->dst, sizeof(ctx->dst), "%s", argv[2]);

    if (stat(ctx->src, &ctx->fst))
    {
        fprintf(stderr, "%s:[%d] errmsg:[%d] %s!\n", __FILE__, __LINE__, errno, strerror(errno));
        return -1;
    }

    /* > 创建内存池 */
    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    ctx->tpool = thread_pool_init(thd_num, &opt, NULL);
    if (NULL == ctx->tpool)
    {
        fprintf(stderr, "%s:[%d] errmsg:[%d] %s!\n", __FILE__, __LINE__, errno, strerror(errno));
        return -1;
    }

    /* > 执行拷贝处理 */
    for (idx=0; idx<thd_num; ++idx)
    {
        thread_pool_add_worker(ctx->tpool, cp_copy_routine, ctx);
    }

    while (1) { pause(); }

    return 0;
}
