#if !defined(__HTML_TREE_H__)
#define __HTML_TREE_H__

#include <stdio.h>
#include "log.h"

/* 如需开启一下功能宏，请将其加入到编译选项中 */
/* 功能宏: 节点只有孩子节点或只有数值(Only child or value) */
/* #define __HTML_OCOV__ */

/* 功能宏: 当组报文时，且结点无孩子结点时，必须使用组合标签:
    <NAME attr1="av1" attr2="av2"></NAME> */
/* #define __HTML_PACK_CMARK__ */

#define HTML_MAX_DEPTH      (32)    /* HTML树的最大深度 */

#define HTML_MARK_BEGIN     "<"     /* 节点开始 */
#define HTML_MARK_BEGIN_LEN (1)     /* 节点开始 长度 */
#define HTML_MARK_END1      "/>"    /* 节点结束 */
#define HTML_MARK_END1_LEN  (2)     /* 节点结束 长度 */
#define HTML_MARK_END2      "</"    /* 节点结束2 */
#define HTML_MARK_END2_LEN  (2)     /* 节点结束2 长度 */
#define HTML_DOC_TYPE_BEGIN "<!DOCTYPE " /* 文档类型开始 */
#define HTML_DOC_TYPE_BEGIN_LEN (10)/* 文档类型开始 长度 */
#define HTML_VERS_END       "?>"    /* 文档类型结束 */
#define HTML_VERS_END_LEN   (2)     /* 版本结束 长度 */
#define HTML_NOTE_BEGIN     "<!--"  /* 注释开始 */
#define HTML_NOTE_BEGIN_LEN (4)     /* 注释开始 长度 */
#define HTML_NOTE_END       "-->"   /* 注释结束 */
#define HTML_NOTE_END_LEN   (3)     /* 注释结束 长度 */
/* HTML_NOTE_END = (HTML_NOTE_END1 + HTML_NOTE_END2)  */
#define HTML_NOTE_END1      "--"    /* 注释结束1 */
#define HTML_NOTE_END1_LEN  (2)     /* 注释结束1 长度 */
#define HTML_NOTE_END2      '>'     /* 注释结束2 */

#define HTML_ROOT_NAME       "ROOT" /* 根节点名称 */
#define HTML_ROOT_NAME_SIZE  (5)    /* 根节点名称 SIZE */

#define HTML_NODE_HAS_NONE   (0)             /* 啥都没有 */
#define HTML_NODE_HAS_CHILD  (0x00000001)    /* 有孩子节点 */
#define HTML_NODE_HAS_ATTR   (0x00000002)    /* 有属性节点 */
#define HTML_NODE_HAS_VALUE  (0x00000004)    /* 有节点值 */

#if !defined(false)
    #define false (0)
#endif
#if !defined(true)
    #define true (1)
#endif

/* HTML节点类型 */
typedef enum
{
    HTML_NODE_ROOT,     /* 根节点 */
    HTML_NODE_CHILD,    /* 孩子节点 */
    HTML_NODE_ATTR,     /* 属性节点 */
    HTML_NODE_UNKNOWN,  /* 未知节点 */
    HTML_NODE_TYPE_TOTAL = HTML_NODE_UNKNOWN    /* 节点类型数 */
} html_node_type_e;


/* HTML节点 */
typedef struct _html_node_t
{
    char *name;                 /* 节点名 */
    char *value;                /* 节点值 */
    html_node_type_e type;      /* 节点类型 */

    struct _html_node_t *next;  /* 兄弟节点链表 */
    struct _html_node_t *firstchild;  /* 孩子节点链表头: 属性节点+孩子节点 */
    struct _html_node_t *tail;  /* 孩子节点链表尾 # 构建/修改HTML树时使用 # 提高操作效率 */
    struct _html_node_t *parent; /* 父亲节点 */

    unsigned int flag;          /* 记录节点是否有孩子(HTML_NODE_HAS_CHILD)、
                                    属性(HTML_NODE_HAS_ATTR)、节点值(HTML_NODE_HAS_VALUE) */
    struct _html_node_t *temp;  /* 临时指针: 遍历HTML树时，提高效率(其他情况下，此指针值无效) */    
} html_node_t;

/* HTML树 */
typedef struct
{
    html_node_t *root;           /* 根节点: 注意root的第一个子节点才是真正的根节点 */
} html_tree_t;

/* 对外的接口 */
#define html_child(node) (node->firstchild)
#define html_parent(node) (node->parent)
#define html_brother(node) (node->next)
#define html_name(node) (node->name)
#define html_value(node) (node->value)

html_node_t *html_node_creat(html_node_type_e type);
html_node_t *html_node_creat_ext(
        html_node_type_e type, const char *name, const char *value);
int html_node_free(html_tree_t *html, html_node_t *node);

html_tree_t *html_creat(const char *fname);
html_tree_t *html_screat(const char *str);
html_tree_t *html_screat_ext(const char *str, int length);

int html_fwrite(html_tree_t *html, const char *fname);
int html_fprint(html_tree_t *html, FILE *fp);
int html_sprint(html_tree_t *html, char *str);
int html_spack(html_tree_t *html, char *str);

html_node_t *html_rsearch(html_tree_t *html, html_node_t *curr, const char *path);
#define html_search(html, path) html_rsearch(html, html->root, path)

html_node_t *html_add_node(
        html_tree_t *html, html_node_t *node,
        const char *name, const char *value, int type);
html_node_t *html_add_attr(
        html_tree_t *html, html_node_t *node,
        const char *name, const char *value);
html_node_t *html_add_child(
        html_tree_t *html, html_node_t *node,
        const char *name, const char *value);
#define html_add_brother(html, node, name, value) \
    html_add_node(html, (node)->parent, name, value, node->type)
int html_delete_child(html_tree_t *html, html_node_t *node, html_node_t *child);
#define html_delete_brother(html, node, brother) \
    html_delete_child(html, (node)->parent, brother)
int html_delete_empty(html_tree_t *html);

int html_set_value(html_node_t *node, const char *value);
int html_node_length(html_tree_t *html, html_node_t *node);
#define html_tree_length(html) html_node_length(html, html->root->firstchild)

extern int _html_pack_length(html_tree_t *html, html_node_t *node);
#define html_pack_length(html) _html_pack_length(html, html->root)

#define html_destroy(html) {html_node_free(html, html->root); free(html); html=NULL;}

#endif /*__HTML_TREE_H__*/
