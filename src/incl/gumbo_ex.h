#if !defined(__GUMBO_EX_H__)
#define __GUMBO_EX_H__

#include "log.h"
#include "slab.h"
#include "list.h"
#include "mem_pool.h"

#include <gumbo.h>

/* GUMBO-HTML对象 */
typedef struct
{
    char path[FILE_PATH_MAX_LEN];   /* HTML文件路径 */

    char *input;                    /* HTML文件缓存 */
    int input_length;               /* HTML文件长度 */

    GumboOutput *output;            /* HTML解析对象 */
    mem_pool_t *mem_pool;           /* 内存池对象 */
    GumboOptions opt;               /* 解析HTML的选项 */
} gumbo_html_t;

/* 查询结果 */
typedef struct
{
    list_t *list;                   /* 结果链表 */
    log_cycle_t *log;               /* 日志对象 */
    mem_pool_t *mem_pool;           /* 内存池 */
} gumbo_result_t;

gumbo_html_t *gumbo_html_parse(const char *path, log_cycle_t *log);
void gumbo_html_destroy(gumbo_html_t *html);

void gumbo_print_result(gumbo_result_t *r);
void gumbo_result_destroy(gumbo_result_t *r);

const char *gumbo_get_title(const gumbo_html_t *html);
gumbo_result_t *gumbo_parse_href(const gumbo_html_t *html, log_cycle_t *log);

#endif /*__GUMBO_EX_H__*/
