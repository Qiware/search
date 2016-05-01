#if !defined(__XML_COMM_H__)
#define __XML_COMM_H__

#include "stack.h"
#include "xml_tree.h"

#define XmlIsULineChar(ch)      ('_' == ch)     /* 下划线 */
#define XmlIsTableChar(ch)      ('\t' == ch)    /* 制表符 */
#define XmlIsStrEndChar(ch)     ('\0' == ch)    /* 结束符 */
#define XmlIsEqualChar(ch)      ('=' == ch)     /* 等号符 */
#define XmlIsQuotChar(ch)      ('"' == ch)      /* 双引号 */
#define XmlIsSQuotChar(ch)      ('\'' == ch)    /* 单引号 */
#define XmlIsLPBrackChar(ch)    ('<' == ch)     /* 左尖括号 */
#define XmlIsRPBrackChar(ch)    ('>' == ch)     /* 右尖括号 */
#define XmlIsRDLineChar(ch)     ('/' == ch)     /* 右斜线 */
#define XmlIsDoubtChar(ch)      ('?' == ch)     /* 疑问号 */
#define XmlIsAndChar(ch)        ('&' == ch)     /* 与号 */
#define XmlIsSubChar(ch)        ('-' == ch)     /* 减号 */
#define XmlIsColonChar(ch)      (':' == ch)     /* 冒号 */
#define XmlIsNLineChar(ch)      (('\n'==ch) || ('\r'==ch))  /* 换行符 */

/* 转义关键字 */
#define XmlIsLtStr(str) (0 == strncmp(str, XML_ESC_LT_STR, XML_ESC_LT_LEN))       /* 小于 */
#define XmlIsGtStr(str) (0 == strncmp(str, XML_ESC_GT_STR, XML_ESC_GT_LEN))       /* 大于 */
#define XmlIsAmpStr(str) (0 == strncmp(str, XML_ESC_AMP_STR, XML_ESC_AMP_LEN))    /* 与号 */
#define XmlIsAposStr(str) (0 == strncmp(str, XML_ESC_APOS_STR, XML_ESC_APOS_LEN)) /* 单引号 */
#define XmlIsQuotStr(str) (0 == strncmp(str, XML_ESC_QUOT_STR, XML_ESC_QUOT_LEN)) /* 双引号 */

#define XmlIsMarkChar(ch) /* 标签名的合法字符 */\
    (isalpha(ch) || isdigit(ch)    \
     || XmlIsULineChar(ch))  || XmlIsSubChar(ch) || XmlIsColonChar(ch)
#define XmlIsMarkBorder(ch) /* 标签名的合法边界 */\
    (isspace(ch) || XmlIsRDLineChar(ch) \
    || XmlIsRPBrackChar(ch) || XmlIsTableChar(ch) || XmlIsNLineChar(ch))
#define XmlIsIgnoreChar(ch) /* 无意义字符 */\
    (isspace(ch) || XmlIsTableChar(ch) || XmlIsNLineChar(ch))


#define xml_set_type(node, t) ((node)->type = (t))          /* 设置结点类型 */
#define xml_set_flag(node, f) ((node)->flag |= (f))     	/* 设置结点标志 */
#define xml_unset_flag(node, f) ((node)->flag &= ~(f))  	/* 清除结点某个标志 */
#define xml_reset_flag(node) ((node)->flag = XML_NODE_HAS_NONE)   /* 重置结点标志 */

#define xml_set_attr_flag(node)  xml_set_flag(node, XML_NODE_HAS_ATTR)   /* 设置有属性 */
#define xml_unset_attr_flag(node)    xml_unset_flag(node, XML_NODE_HAS_ATTR) /* 设置无属性 */
#define xml_set_child_flag(node) xml_set_flag(node, XML_NODE_HAS_CHILD)  /* 设置有孩子 */
#define xml_unset_child_flag(node) xml_unset_flag(node, XML_NODE_HAS_CHILD)  /* 设置无孩子 */
#define xml_set_value_flag(node) xml_set_flag(node, XML_NODE_HAS_VALUE)  /* 设置有结点值 */
#define xml_unset_value_flag(node) xml_unset_flag(node, XML_NODE_HAS_VALUE)  /* 设置无结点值 */

#define xml_is_attr(node) (XML_NODE_ATTR == (node)->type)   /* 结点是否为属性结点 */
#define xml_is_child(node) (XML_NODE_CHILD == (node)->type) /* 结点是否为孩子结点 */
#define xml_is_root(node) (XML_NODE_ROOT == (node)->type)   /* 结点是否为父亲结点 */
#define xml_has_value(node) (XML_NODE_HAS_VALUE&(node)->flag) /* 结点是否有值 */
#define xml_has_attr(node)  (XML_NODE_HAS_ATTR&(node)->flag)    /* 是否有属性结点 */
#define xml_has_child(node) (XML_NODE_HAS_CHILD&(node)->flag)   /* 是否有孩子结点 */

#define XML_BEGIN_FLAG      '<'	    /* 标签开始标志"<" */
#define XML_VERS_FLAG       '?'	    /* 版本信息标志"<?xml " */
#define XML_NOTE_FLAG       '!'	    /* 注释信息标志"<!--" */
#define XML_END_FLAG        '/'	    /* 结束标志"</XXX>" */
#define STR_END_FLAG        '\0'    /* 字串结束符 */

#if defined(__XML_ESC_PARSE__)
/* 转义字串及对应长度 */
#define XML_ESC_LT_STR     "&lt;"
#define XML_ESC_LT_LEN     (4)

#define XML_ESC_GT_STR     "&gt;"
#define XML_ESC_GT_LEN     (4)

#define XML_ESC_AMP_STR    "&amp;"
#define XML_ESC_AMP_LEN    (5)

#define XML_ESC_APOS_STR   "&apos;"
#define XML_ESC_APOS_LEN   (6)

#define XML_ESC_QUOT_STR   "&quot;"
#define XML_ESC_QUOT_LEN   (6)

#define XML_ESC_UNKNOWN_STR "&"
#define XML_ESC_UNKNOWN_LEN (1)
#endif /*__XML_ESC_PARSE__*/

/* 错误信息定义 */
typedef enum
{
    XML_OK = 0
    , XML_HAS_VALUE             /* 有值 */
    , XML_NO_VALUE              /* 无值 */

    , XML_ERR = ~0x7fffffff     /* 失败 */
    , XML_ERR_CALLOC            /* calloc失败 */
    , XML_ERR_FORMAT            /* XML格式错误 */
    , XML_ERR_STACK             /* 栈出错 */
    , XML_ERR_NODE_TYPE         /* 结点类型错误 */
    , XML_ERR_GET_ATTR          /* 属性获取失败 */
    , XML_ERR_GET_NAME          /* 标签名获取失败 */
    , XML_ERR_MARK_MISMATCH     /* 标签不匹配 */
    , XML_ERR_CREAT_NODE        /* 新建结点失败 */
    , XML_ERR_PTR_NULL          /* 空指针 */
    , XML_ERR_EMPTY_TREE        /* 空树 */
    , XML_ERR_FOPEN             /* fopen失败 */
    , XML_ERR_PTR               /* 指针错误 */
} xml_err_e;

#if defined(__XML_ESC_PARSE__)
/* 转义字串类型 */
typedef enum
{
    XML_ESC_LT             /* &lt;     < */
    , XML_ESC_GT           /* &gt;     > */
    , XML_ESC_AMP          /* &amp;    & */
    , XML_ESC_APOS         /* &apos;   ' */
    , XML_ESC_QUOT         /* &quot;   " */
    , XML_ESC_UNKNOWN

    , XML_ESC_TOTAL = XML_ESC_UNKNOWN
} xml_esc_e;

typedef struct
{
    xml_esc_e type;
    char *str;
    char ch;
    int len;
} xml_esc_t;
#endif /*__XML_ESC_PARSE__*/

/* 文件解析 结构体 */
typedef struct
{
    const char *str;            /* XML字串 */
    const char *ptr;            /* 当前处理到的位置 */
    size_t len;                 /* 需处理的长度 */
} xml_parse_t;

#if defined(__XML_ESC_PARSE__)
/* 转义字串分割链表: 用于有转义字串的结点值或属性值的处理 */
typedef struct _xml_esc_node_t
{
    int len;
    char *str;
    struct _xml_esc_node_t *next;
} xml_esc_node_t;

/* 转义字串分割链表 */
typedef struct
{
    xml_esc_node_t *head;
    xml_esc_node_t *tail;
} xml_esc_split_t;
#endif /*__XML_ESC_PARSE__*/

typedef struct
{
    char *str;
    char *ptr;
} sprint_t;

#define sprint_init(sp, s) ((sp)->str = (s), (sp)->ptr = s)


char *xml_fload(const char *fname, xml_opt_t *opt);

xml_tree_t *xml_init(xml_opt_t *opt);
int xml_parse(xml_tree_t *xml, Stack_t *stack, const char *str, size_t len);
int xml_fprint_tree(xml_tree_t *xml, xml_node_t *root, Stack_t *stack, FILE *fp);
int xml_sprint_tree(xml_tree_t *xml, xml_node_t *root, Stack_t *stack, sprint_t *sp);
int xml_pack_tree(xml_tree_t *xml, xml_node_t *root, Stack_t *stack, sprint_t *sp);

int _xml_node_len(xml_tree_t *xml, xml_node_t *root, Stack_t *stack);
int xml_pack_node_len(xml_tree_t *xml, xml_node_t *root, Stack_t *stack);

int xml_node_free_one(xml_tree_t *xml, xml_node_t *node);
xml_node_t *xml_free_next(xml_tree_t *xml, Stack_t *stack, xml_node_t *curr);

#endif /*__XML_COMM_H__*/
