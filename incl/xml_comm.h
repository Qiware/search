#if !defined(__XML_COMM_H__)
#define __XML_COMM_H__

#include "stack.h"
#include "xml_tree.h"


#define xml_set_type(node, t) ((node)->type = (t))          /* 设置节点类型 */
#define xml_set_flag(node, f) ((node)->flag |= (f))     	/* 设置节点标志 */
#define xml_unset_flag(node, f) ((node)->flag &= ~(f))  	/* 清除节点某个标志 */
#define xml_reset_flag(node) ((node)->flag = XML_NODE_HAS_NONE)   /* 重置节点标志 */

#define xml_set_attr_flag(node)  xml_set_flag(node, XML_NODE_HAS_ATTR)   /* 设置有属性 */
#define xml_unset_attr_flag(node)    xml_unset_flag(node, XML_NODE_HAS_ATTR) /* 设置无属性 */
#define xml_set_child_flag(node) xml_set_flag(node, XML_NODE_HAS_CHILD)  /* 设置有孩子 */
#define xml_unset_child_flag(node) xml_unset_flag(node, XML_NODE_HAS_CHILD)  /* 设置无孩子 */
#define xml_set_value_flag(node) xml_set_flag(node, XML_NODE_HAS_VALUE)  /* 设置有节点值 */
#define xml_unset_value_flag(node) xml_unset_flag(node, XML_NODE_HAS_VALUE)  /* 设置无节点值 */

#define xml_is_attr(node) (XML_NODE_ATTR == (node)->type)   /* 节点是否为属性节点 */
#define xml_is_child(node) (XML_NODE_CHILD == (node)->type) /* 节点是否为孩子节点 */
#define xml_is_root(node) (XML_NODE_ROOT == (node)->type)   /* 节点是否为父亲节点 */
#define xml_has_value(node) (XML_NODE_HAS_VALUE&(node)->flag) /* 节点是否有值 */
#define xml_has_attr(node)  (XML_NODE_HAS_ATTR&(node)->flag)    /* 是否有属性节点 */
#define xml_has_child(node) (XML_NODE_HAS_CHILD&(node)->flag)   /* 是否有孩子节点 */

#define XML_BEGIN_FLAG      '<'	    /* 标签开始标志"<" */
#define XML_VERS_FLAG       '?'	    /* 版本信息标志"<?xml " */
#define XML_NOTE_FLAG       '!'	    /* 注释信息标志"<!--" */
#define XML_END_FLAG        '/'	    /* 结束标志"</XXX>" */
#define STR_END_FLAG        '\0'    /* 字串结束符 */

#if defined(__XML_FRWD_PARSE__)
/* 转义字串及对应长度 */
#define XML_FRWD_LT_STR     "&lt;"
#define XML_FRWD_LT_LEN     (4)

#define XML_FRWD_GT_STR     "&gt;"
#define XML_FRWD_GT_LEN     (4)

#define XML_FRWD_AMP_STR    "&amp;"
#define XML_FRWD_AMP_LEN    (5)

#define XML_FRWD_APOS_STR   "&apos;"
#define XML_FRWD_APOS_LEN   (6)

#define XML_FRWD_QUOT_STR   "&quot;"
#define XML_FRWD_QUOT_LEN   (6)

#define XML_FRWD_UNKNOWN_STR "&"
#define XML_FRWD_UNKNOWN_LEN (1)
#endif /*__XML_FRWD_PARSE__*/

/* 错误信息定义 */
typedef enum
{
    XML_ERR_CALLOC           /* calloc失败 */
    , XML_ERR_FORMAT         /* XML格式错误 */
    , XML_ERR_STACK          /* 栈出错 */
    , XML_ERR_NODE_TYPE      /* 节点类型错误 */
    , XML_ERR_GET_ATTR       /* 属性获取失败 */
    , XML_ERR_GET_NAME       /* 标签名获取失败 */
    , XML_ERR_MARK_MISMATCH  /* 标签不匹配 */
    , XML_ERR_CREAT_NODE     /* 新建节点失败 */
    , XML_ERR_PTR_NULL       /* 空指针 */
    , XML_ERR_EMPTY_TREE     /* 空树 */
    , XML_ERR_FOPEN          /* fopen失败 */
    
    /* 请在此行之上增加新的错误码 */
    , XML_FAILED             /* 失败 */
    , XML_SUCCESS = 0
}xml_err_e;

#if defined(__XML_FRWD_PARSE__)
/* 转义字串类型 */
typedef enum
{
    XML_FRWD_LT             /* &lt;     < */
    , XML_FRWD_GT           /* &gt;     > */
    , XML_FRWD_AMP          /* &amp;    & */
    , XML_FRWD_APOS         /* &apos;   ' */
    , XML_FRWD_QUOT         /* &quot;   " */
    , XML_FRWD_UNKNOWN

    , XML_FRWD_TOTAL = XML_FRWD_UNKNOWN
}xml_frwd_e;

typedef struct
{
    xml_frwd_e type;
    char *str;
    char ch;
    int length;
}xml_frwd_t;
#endif /*__XML_FRWD_PARSE__*/

/* 文件解析 结构体 */
typedef struct
{
    const char *str;         /* XML字串 */
    const char *ptr;            /* 当前处理到的位置 */
    int length;
}xml_fparse_t;

#if defined(__XML_FRWD_PARSE__)
/* 转义字串分割链表: 用于有转义字串的结点值或属性值的处理 */
typedef struct _xml_frwd_node_t
{
    int length;
    char *str;
    struct _xml_frwd_node_t *next;
}xml_frwd_node_t;

/* 转义字串分割链表 */
typedef struct
{
    xml_frwd_node_t *head;
    xml_frwd_node_t *tail;
}xml_frwd_split_t;
#endif /*__XML_FRWD_PARSE__*/

typedef struct
{
    char *str;
    char *ptr;
}sprint_t;

#define sprint_init(sp, s) ((sp)->str = (s), (sp)->ptr = s)


extern char *xml_fload(const char *fname);

extern int xml_init(xml_tree_t **xmltree);
extern int xml_parse(xml_tree_t *xmltree, Stack_t *stack, const char *str);
extern int xml_fprint_tree(xml_node_t *root, Stack_t *stack, FILE *fp);
extern int xml_sprint_tree(xml_node_t *root, Stack_t *stack, sprint_t *sp);
extern int xml_pack_tree(xml_node_t *root, Stack_t *stack, sprint_t *sp);

extern int _xml_node_length(xml_node_t *root, Stack_t *stack);
extern int xml_pack_node_length(xml_node_t *root, Stack_t *stack);
    
extern int xml_node_sfree(xml_node_t *node);
extern xml_node_t *xml_free_next(Stack_t *stack, xml_node_t *current);

#endif /*__XML_COMM_H__*/
