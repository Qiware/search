/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: gumbo_ex.c
 ** 版本号: 1.0
 ** 描  述: 用于处理HTML文件的解析处理
 **         在此代码中使用了开源代码GUMBO用于解析HTML文件, 但做了如下扩展:
 **         1. 增加了内存池机制，减少内存碎片的产生;
 **         2. 对原有接口进行封装，增强接口的易用性.
 ** 作  者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "gumbo_ex.h"

static int gumbo_load_html(gumbo_cntx_t *ctx, gumbo_html_t *html);

/******************************************************************************
 **函数名称: gumbo_init
 **功    能: 初始化GUMBO对象
 **输入参数:
 **     ctx: 全局对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.14 #
 ******************************************************************************/
int gumbo_init(gumbo_cntx_t *ctx)
{
    int ret;

    memset(ctx, 0, sizeof(gumbo_cntx_t));

    /* 1. 初始化Slab对象 */
    ret = eslab_init(&ctx->slab, 32 * KB);
    if (0 != ret)
    {
        log2_error("Initialize slab failed!");
        return -1;
    }

    /* 2. 设置OPTIONS对象 */
    ctx->opt.userdata = (void *)&ctx->slab;
    ctx->opt.allocator = (GumboAllocatorFunction)&eslab_alloc;
    ctx->opt.deallocator = (GumboDeallocatorFunction)&eslab_free;
    ctx->opt.tab_stop = 8;
    ctx->opt.stop_on_first_error = false;
    ctx->opt.max_errors = -1;

    return 0;
}

/******************************************************************************
 **函数名称: gumbo_html_parse
 **功    能: 解析指定的HTML文件
 **输入参数:
 **     ctx: 全局对象
 **     path: HTML文件路径
 **输出参数:
 **返    回: HTML对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.14 #
 ******************************************************************************/
gumbo_html_t *gumbo_html_parse(gumbo_cntx_t *ctx, const char *path)
{
    int ret;
    gumbo_html_t *html;

    /* 1. 创建HTML对象 */
    html = eslab_alloc(&ctx->slab, sizeof(gumbo_html_t));
    if (NULL == html)
    {
        log2_error("Alloc memory from slab failed!");
        return NULL;
    }

    /* 2. 加载HTML文件 */
    snprintf(html->path, sizeof(html->path), "%s", path);

    ret = gumbo_load_html(ctx, html);
    if (0 != ret)
    {
        eslab_free(&ctx->slab, html);
        log2_error("Load html failed! path:%s", path);
        return NULL;
    }

    /* 3. 解析HTML文件 */
    html->output = gumbo_parse_with_options(
                        &ctx->opt, html->input, html->input_length);
    if (NULL == html->output)
    {
        eslab_free(&ctx->slab, html->input);
        eslab_free(&ctx->slab, html);
        return NULL;
    }

    return html;
}

/******************************************************************************
 **函数名称: gumbo_html_destroy
 **功    能: 释放HTML对象
 **输入参数:
 **     ctx: 全局对象
 **     html: HTML对象
 **输出参数:
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.14 #
 ******************************************************************************/
void gumbo_html_destroy(gumbo_cntx_t *ctx, gumbo_html_t *html)
{
    gumbo_destroy_output(&ctx->opt, html->output);
    eslab_free(&ctx->slab, html->input);
    eslab_free(&ctx->slab, html);
}

/******************************************************************************
 **函数名称: gumbo_destroy
 **功    能: 释放GUMBO对象
 **输入参数:
 **     ctx: 全局对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.14 #
 ******************************************************************************/
void gumbo_destroy(gumbo_cntx_t *ctx)
{
    eslab_destroy(&ctx->slab);
}

/******************************************************************************
 **函数名称: gumbo_load_html
 **功    能: 将HTML文件载入内存
 **输入参数:
 **     ctx: 全局对象
 **输出参数:
 **     html: HTML对象
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.14 #
 ******************************************************************************/
static int gumbo_load_html(gumbo_cntx_t *ctx, gumbo_html_t *html)
{
    FILE *fp;
    int fd, off, n;
    struct stat st;
    
    /* 1. 打开文件 */
    fp = fopen(html->path, "r");
    if (NULL == fp)
    {
        log2_error("errmsg:[%d] %s! path:%s", errno, strerror(errno), html->path);
        return -1;
    }

    /* 2. 获取文件大小，并分配空间 */
    fd = fileno(fp);
    fstat(fd, &st);

    html->input_length = st.st_size;
    html->input = eslab_alloc(&ctx->slab, html->input_length + 1);
    if (NULL == html->input)
    {
        log2_error("Alloc memory from slab failed!");
        return -1;
    }

    /* 3. 载入内存 */
    off = 0;
    while ((n = fread(html->input + off, 1, html->input_length - off, fp)))
    {
        off += n;
    }

    fclose(fp);
    return 0;
}
