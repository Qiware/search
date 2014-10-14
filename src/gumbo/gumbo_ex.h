#if !defined(__GUMBO_EX_H__)
#define __GUMBO_EX_H__

#include "slab.h"

#include <gumbo.h>

/* GUMBO-CTX对象 */
typedef struct
{
    GumboOptions opt;               /* 配置信息 */
    eslab_pool_t slab;              /* 内存池对象 */
} gumbo_cntx_t;

/* GUMBO-HTML对象 */
typedef struct
{
    char path[FILE_PATH_MAX_LEN];   /* HTML文件路径 */

    char *input;                    /* HTML文件缓存 */
    int input_length;               /* HTML文件长度 */

    GumboOutput *output;            /* HTML解析对象 */
} gumbo_html_t;

int gumbo_init(gumbo_cntx_t *ctx);
void gumbo_destroy(gumbo_cntx_t *ctx);

gumbo_html_t *gumbo_html_parse(gumbo_cntx_t *ctx, const char *path);
void gumbo_html_destroy(gumbo_cntx_t *ctx, gumbo_html_t *html);

#endif /*__GUMBO_EX_H__*/
