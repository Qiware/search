#if !defined(__HTML_COMM_H__)
#define __HTML_COMM_H__

#include "stack.h"
#include "html_tree.h"

#define HtmlIsULineChar(ch)      ('_' == ch)     /* 下划线 */
#define HtmlIsTableChar(ch)      ('\t' == ch)    /* 制表符 */
#define HtmlIsStrEndChar(ch)     ('\0' == ch)    /* 结束符 */
#define HtmlIsEqualChar(ch)      ('=' == ch)     /* 等号符 */
#define HtmlIsDQuotChar(ch)      ('"' == ch)     /* 双引号 */
#define HtmlIsSQuotChar(ch)      ('\'' == ch)    /* 单引号 */
#define HtmlIsLPBrackChar(ch)    ('<' == ch)     /* 左尖括号 */
#define HtmlIsRPBrackChar(ch)    ('>' == ch)     /* 右尖括号 */
#define HtmlIsRDLineChar(ch)     ('/' == ch)     /* 右斜线 */
#define HtmlIsDoubtChar(ch)      ('?' == ch)     /* 疑问号 */
#define HtmlIsAndChar(ch)        ('&' == ch)     /* 与号 */
#define HtmlIsSubChar(ch)        ('-' == ch)     /* 减号 */
#define HtmlIsColonChar(ch)      (':' == ch)     /* 冒号 */
#define HtmlIsNLineChar(ch)      (('\n'==ch) || ('\r'==ch))  /* 换行符 */

#define HtmlIsMarkChar(ch) /* 标签名的合法字符 */\
    (isalpha(ch) || isdigit(ch)    \
     || HtmlIsULineChar(ch))  || HtmlIsSubChar(ch) || HtmlIsColonChar(ch)
#define HtmlIsMarkBorder(ch) /* 标签名的合法边界 */\
    (isspace(ch) || HtmlIsRDLineChar(ch) \
    || HtmlIsRPBrackChar(ch) || HtmlIsTableChar(ch) || HtmlIsNLineChar(ch))
#define HtmlIsIgnoreChar(ch) /* 无意义字符 */\
    (isspace(ch) || HtmlIsTableChar(ch) || HtmlIsNLineChar(ch))


#define html_set_type(node, t) ((node)->type = (t))          /* 设置节点类型 */
#define html_set_flag(node, f) ((node)->flag |= (f))     	/* 设置节点标志 */
#define html_unset_flag(node, f) ((node)->flag &= ~(f))  	/* 清除节点某个标志 */
#define html_reset_flag(node) ((node)->flag = HTML_NODE_HAS_NONE)   /* 重置节点标志 */

#define html_set_attr_flag(node)  html_set_flag(node, HTML_NODE_HAS_ATTR)   /* 设置有属性 */
#define html_unset_attr_flag(node)    html_unset_flag(node, HTML_NODE_HAS_ATTR) /* 设置无属性 */
#define html_set_child_flag(node) html_set_flag(node, HTML_NODE_HAS_CHILD)  /* 设置有孩子 */
#define html_unset_child_flag(node) html_unset_flag(node, HTML_NODE_HAS_CHILD)  /* 设置无孩子 */
#define html_set_value_flag(node) html_set_flag(node, HTML_NODE_HAS_VALUE)  /* 设置有节点值 */
#define html_unset_value_flag(node) html_unset_flag(node, HTML_NODE_HAS_VALUE)  /* 设置无节点值 */

#define html_is_attr(node) (HTML_NODE_ATTR == (node)->type)   /* 节点是否为属性节点 */
#define html_is_child(node) (HTML_NODE_CHILD == (node)->type) /* 节点是否为孩子节点 */
#define html_is_root(node) (HTML_NODE_ROOT == (node)->type)   /* 节点是否为父亲节点 */
#define html_has_value(node) (HTML_NODE_HAS_VALUE&(node)->flag) /* 节点是否有值 */
#define html_has_attr(node)  (HTML_NODE_HAS_ATTR&(node)->flag)    /* 是否有属性节点 */
#define html_has_child(node) (HTML_NODE_HAS_CHILD&(node)->flag)   /* 是否有孩子节点 */

#define HTML_BEGIN_FLAG      '<'	    /* 标签开始标志"<" */
#define HTML_VERS_FLAG       '?'	    /* 版本信息标志"<?html " */
#define HTML_NOTE_FLAG       '!'	    /* 注释信息标志"<!--" */
#define HTML_END_FLAG        '/'	    /* 结束标志"</XXX>" */
#define STR_END_FLAG        '\0'    /* 字串结束符 */

/* 错误信息定义 */
typedef enum
{
    HTML_ERR_CALLOC           /* calloc失败 */
    , HTML_ERR_FORMAT         /* HTML格式错误 */
    , HTML_ERR_STACK          /* 栈出错 */
    , HTML_ERR_NODE_TYPE      /* 节点类型错误 */
    , HTML_ERR_GET_ATTR       /* 属性获取失败 */
    , HTML_ERR_GET_NAME       /* 标签名获取失败 */
    , HTML_ERR_MARK_MISMATCH  /* 标签不匹配 */
    , HTML_ERR_CREAT_NODE     /* 新建节点失败 */
    , HTML_ERR_PTR_NULL       /* 空指针 */
    , HTML_ERR_EMPTY_TREE     /* 空树 */
    , HTML_ERR_FOPEN          /* fopen失败 */
    
    /* 请在此行之上增加新的错误码 */
    , HTML_FAILED             /* 失败 */
    , HTML_SUCCESS = 0
}html_err_e;

/* 文件解析 结构体 */
typedef struct
{
    const char *str;         /* HTML字串 */
    const char *ptr;            /* 当前处理到的位置 */
    int length;
}html_fparse_t;

typedef struct
{
    char *str;
    char *ptr;
}sprint_t;

#define sprint_init(sp, s) ((sp)->str = (s), (sp)->ptr = s)


char *html_fload(const char *fname);

int html_init(html_tree_t **html);
int html_parse(html_tree_t *html, Stack_t *stack, const char *str);
int html_fprint_tree(html_tree_t *html, html_node_t *root, Stack_t *stack, FILE *fp);
int html_sprint_tree(html_tree_t *html, html_node_t *root, Stack_t *stack, sprint_t *sp);
int html_pack_tree(html_tree_t *html, html_node_t *root, Stack_t *stack, sprint_t *sp);

int _html_node_length(html_tree_t *html, html_node_t *root, Stack_t *stack);
int html_pack_node_length(html_tree_t *html, html_node_t *root, Stack_t *stack);

int html_node_sfree(html_node_t *node);
html_node_t *html_free_next(html_tree_t *html, Stack_t *stack, html_node_t *current);

#endif /*__HTML_COMM_H__*/
