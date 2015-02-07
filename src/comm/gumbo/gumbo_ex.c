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
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "list.h"
#include "gumbo_ex.h"

static int gumbo_load_html(gumbo_html_t *html);

/******************************************************************************
 **函数名称: gumbo_html_parse
 **功    能: 解析指定的HTML文件
 **输入参数:
 **     path: HTML文件路径
 **输出参数:
 **返    回: HTML对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.14 #
 ******************************************************************************/
gumbo_html_t *gumbo_html_parse(const char *path)
{
    int ret;
    gumbo_html_t *html;
    mem_pool_t *mem_pool;

    /* 1. 创建内存池 */
    mem_pool = mem_pool_creat(1 * MB);
    if (NULL == mem_pool)
    {
        syslog_error("Create memory pool failed!");
        return NULL;
    }

    /* 2. 创建HTML对象 */
    html = mem_pool_alloc(mem_pool, sizeof(gumbo_html_t));
    if (NULL == html)
    {
        mem_pool_destroy(mem_pool);
        syslog_error("Alloc memory from slab failed!");
        return NULL;
    }

    html->mem_pool = mem_pool;

    /* 3. 加载HTML文件 */
    snprintf(html->path, sizeof(html->path), "%s", path);

    ret = gumbo_load_html(html);
    if (0 != ret)
    {
        mem_pool_destroy(mem_pool);
        syslog_error("Load html failed! path:%s", path);
        return NULL;
    }

    /* 3. 解析HTML文件 */
    html->opt.userdata = (void *)mem_pool;
    html->opt.allocator = (GumboAllocatorFunction)&mem_pool_alloc;
    html->opt.deallocator = (GumboDeallocatorFunction)&_mem_pool_dealloc;
    html->opt.tab_stop = 8;
    html->opt.stop_on_first_error = false;
    html->opt.max_errors = -1;

    html->output = gumbo_parse_with_options(&html->opt, html->input, html->input_length);
    if (NULL == html->output)
    {
        mem_pool_destroy(mem_pool);
        return NULL;
    }

    return html;
}

/******************************************************************************
 **函数名称: gumbo_html_destroy
 **功    能: 释放HTML对象
 **输入参数:
 **     html: HTML对象
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.14 #
 ******************************************************************************/
void gumbo_html_destroy(gumbo_html_t *html)
{
    gumbo_destroy_output(&html->opt, html->output);
    mem_pool_destroy(html->mem_pool);
}

/******************************************************************************
 **函数名称: gumbo_load_html
 **功    能: 将HTML文件载入内存
 **输入参数: NONE
 **输出参数:
 **     html: HTML对象
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.14 #
 ******************************************************************************/
static int gumbo_load_html(gumbo_html_t *html)
{
    FILE *fp;
    int fd, off, n;
    struct stat st;
    
    /* 1. 打开文件 */
    fp = fopen(html->path, "r");
    if (NULL == fp)
    {
        syslog_error("errmsg:[%d] %s! path:%s", errno, strerror(errno), html->path);
        return -1;
    }

    /* 2. 获取文件大小，并分配空间 */
    fd = fileno(fp);
    fstat(fd, &st);

    html->input_length = st.st_size;
    html->input = mem_pool_alloc(html->mem_pool, html->input_length + 1);
    if (NULL == html->input)
    {
        syslog_error("Alloc memory from slab failed!");
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

/******************************************************************************
 **函数名称: gumbo_get_title
 **功    能: 获取TITLE字段
 **输入参数: 
 **     html: HTML对象
 **输出参数: NONE
 **返    回: TITLE字段
 **实现描述: 
 **     1. 查找HEAD结点
 **     2. 查找HTML结点
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.14 #
 ******************************************************************************/
const char *gumbo_get_title(const gumbo_html_t *html)
{
    int idx;
    const GumboNode *root, *head, *child, *title_text;
    const GumboVector *children;

    root = html->output->root;

    /* 1. 查找HEAD结点 */
    children = &root->v.element.children;
    head = NULL;
    for (idx=0; idx<children->length; ++idx)
    {
        child = children->data[idx];
        if (GUMBO_NODE_ELEMENT == child->type
            && GUMBO_TAG_HEAD == child->v.element.tag)
        {
            head = child;
            break;
        }
    }

    /* 2. 查找HTML结点 */
    children = &head->v.element.children;
    for (idx=0; idx<children->length; ++idx)
    {
        child = children->data[idx];
        if (GUMBO_NODE_ELEMENT == child->type
            && GUMBO_TAG_TITLE == child->v.element.tag)
        {
            if (1 != child->v.element.children.length)
            {
                return NULL;
            }

            title_text = child->v.element.children.data[0];
            return title_text->v.text.text;
        }
    }

    return NULL;
}

/******************************************************************************
 **函数名称: gumbo_parse_href
 **功    能: 查找HREF字段
 **输入参数: 
 **     html: HTML对象
 **输出参数:
 **     r: 查询结果
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.15 #
 ******************************************************************************/
static void _gumbo_parse_href(GumboNode *node, gumbo_result_t *r)
{
    int len, idx;
    list_node_t *list_node;
    GumboAttribute *href;
    GumboVector *children;

    if (GUMBO_NODE_ELEMENT != node->type)
    {
        return;
    }

    if (GUMBO_TAG_A == node->v.element.tag
        && (href = gumbo_get_attribute(&node->v.element.attributes, "href")))
    {
        /* 新建链表结点 */
        list_node = mem_pool_alloc(r->mem_pool, sizeof(list_node_t));
        if (NULL == list_node)
        {
            syslog_error("Alloc memory from slab failed!");
            return;
        }

        /* 申请数据空间 */
        len = strlen(href->value);

        list_node->data = mem_pool_alloc(r->mem_pool, len + 1);
        if (NULL == list_node->data)
        {
            mem_pool_dealloc(r->mem_pool, list_node);
            syslog_error("Alloc memory from slab failed!");
            return;
        }

        snprintf(list_node->data, len+1, "%s", href->value);

        /* 插入链表尾部 */
        list_insert_tail(&r->list, list_node);
    }

    children = &node->v.element.children;
    for (idx = 0; idx < children->length; ++idx)
    {
        _gumbo_parse_href((GumboNode *)children->data[idx], r);
    }
}

/******************************************************************************
 **函数名称: gumbo_parse_href
 **功    能: 查找HREF字段
 **输入参数: 
 **     html: HTML对象
 **输出参数: NONE
 **返    回: HREF字段列表
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.14 #
 ******************************************************************************/
gumbo_result_t *gumbo_parse_href(const gumbo_html_t *html)
{
    gumbo_result_t *r;
    mem_pool_t *mem_pool;

    /* 1. 创建内存池 */
    mem_pool = mem_pool_creat(1 * MB);
    if (NULL == mem_pool)
    {
        syslog_error("Create memory pool failed!");
        return NULL;
    }

    /* 2. 创建结果集对象 */
    r = mem_pool_alloc(mem_pool, sizeof(gumbo_result_t));
    if (NULL == r)
    {
        mem_pool_destroy(mem_pool);
        syslog_error("Alloc memory from slab failed!");
        return NULL;
    }

    r->list.num = 0;
    r->list.head = NULL;
    r->list.tail = NULL;
    r->mem_pool = mem_pool;

    /* 3. 提取HREF字段 */
    _gumbo_parse_href(html->output->root, r);

    return r;
}

/******************************************************************************
 **函数名称: gumbo_print_result
 **功    能: 打印结果
 **输入参数: 
 **     r: 结果对象
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.15 #
 ******************************************************************************/
void gumbo_print_result(gumbo_result_t *r)
{
    list_node_t *node = r->list.head;

    while (NULL != node)
    {
        fprintf(stdout, "%s\n", (char *)node->data);
        node = node->next;
    }
}

/******************************************************************************
 **函数名称: gumbo_result_destroy
 **功    能: 释放结果对象
 **输入参数: 
 **     r: 结果对象
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.15 #
 ******************************************************************************/
void gumbo_result_destroy(gumbo_result_t *r)
{
    mem_pool_destroy(r->mem_pool);
}
