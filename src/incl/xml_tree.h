#if !defined(__XML_TREE_H__)
#define __XML_TREE_H__

#include "log.h"
#include "str.h"

/* 功能宏: 当组报文时，且结点无孩子结点时，必须使用组合标签:
    <NAME attr1="av1" attr2="av2"></NAME> */
/* #define __XML_PACK_CMARK__ */

#define XML_MAX_DEPTH      (32)     /* XML树的最大深度 */

#define XML_MARK_BEGIN     "<"      /* 节点开始 */
#define XML_MARK_BEGIN_LEN  (1)     /* 节点开始 长度 */
#define XML_MARK_END1      "/>"     /* 节点结束 */
#define XML_MARK_END1_LEN   (2)     /* 节点结束 长度 */
#define XML_MARK_END2      "</"     /* 节点结束2 */
#define XML_MARK_END2_LEN   (2)     /* 节点结束2 长度 */
#define XML_VERS_BEGIN     "<?xml " /* 版本开始 */
#define XML_VERS_BEGIN_LEN  (6)     /* 版本开始 长度 */
#define XML_VERS_END       "?>"     /* 版本结束 */
#define XML_VERS_END_LEN    (2)     /* 版本结束 长度 */
#define XML_NOTE_BEGIN     "<!--"   /* 注释开始 */
#define XML_NOTE_BEGIN_LEN  (4)     /* 注释开始 长度 */
#define XML_NOTE_END       "-->"    /* 注释结束 */
#define XML_NOTE_END_LEN    (3)     /* 注释结束 长度 */
/* XML_NOTE_END = (XML_NOTE_END1 + XML_NOTE_END2)  */
#define XML_NOTE_END1      "--"     /* 注释结束1 */
#define XML_NOTE_END1_LEN   (2)     /* 注释结束1 长度 */
#define XML_NOTE_END2      '>'      /* 注释结束2 */

#define XML_ROOT_NAME       "ROOT"  /* 根节点名称 */
#define XML_ROOT_NAME_SIZE  (5)     /* 根节点名称 SIZE */

#define XML_NODE_HAS_NONE   (0)             /* 啥都没有 */
#define XML_NODE_HAS_CHILD  (0x00000001)    /* 有孩子节点 */
#define XML_NODE_HAS_ATTR   (0x00000002)    /* 有属性节点 */
#define XML_NODE_HAS_VALUE  (0x00000004)    /* 有节点值 */

/* XML节点类型 */
typedef enum
{
    XML_NODE_ROOT,     /* 根节点 */
    XML_NODE_CHILD,    /* 孩子节点 */
    XML_NODE_ATTR,     /* 属性节点 */
    XML_NODE_UNKNOWN,  /* 未知节点 */
    XML_NODE_TYPE_TOTAL = XML_NODE_UNKNOWN    /* 节点类型数 */
} xml_node_type_e;

/* 选项 */
typedef struct
{
    void *pool;                 /* 内存池 */
    mem_alloc_cb_t alloc;       /* 申请内存 */
    mem_dealloc_cb_t dealloc;   /* 释放内存 */

    log_cycle_t *log;           /* 日志对象 */
} xml_opt_t;

/* XML节点 */
typedef struct _xml_node_t
{
    str_t name;                 /* 节点名 */
    str_t value;                /* 节点值 */
    xml_node_type_e type;       /* 节点类型 */

    struct _xml_node_t *next;   /* 兄弟节点链表 */
    struct _xml_node_t *child;  /* 孩子节点链表头: 属性节点+孩子节点 */
    struct _xml_node_t *tail;   /* 孩子节点链表尾 # 构建/修改XML树时使用 # 提高操作效率 */
    struct _xml_node_t *parent; /* 父亲节点 */

    unsigned int flag;          /* 记录节点是否有孩子(XML_NODE_HAS_CHILD)、
                                    属性(XML_NODE_HAS_ATTR)、节点值(XML_NODE_HAS_VALUE) */
    struct _xml_node_t *temp;   /* 临时指针: 遍历XML树时，提高效率(其他情况下，此指针值无效) */
} xml_node_t;

/* 结点初始化 */
#define xml_node_init(node, _type)  \
{ \
    (node)->name.str = NULL; \
    (node)->name.len = 0; \
    (node)->value.str = NULL; \
    (node)->value.len = 0; \
    (node)->type = (_type); \
    (node)->next = NULL; \
    (node)->child = NULL; \
    (node)->tail = NULL; \
    (node)->parent = NULL; \
    (node)->flag = XML_NODE_HAS_NONE; \
    (node)->temp = NULL; \
}

/* XML树 */
typedef struct
{
    xml_node_t *root;               /* 根节点: 注意root的第一个子节点才是真正的根节点 */

    struct {
        void *pool;                 /* 内存池 */
        mem_alloc_cb_t alloc;       /* 申请内存 */
        mem_dealloc_cb_t dealloc;   /* 释放内存 */

        log_cycle_t *log;           /* 日志对象 */
    };
} xml_tree_t;

/* 对外的接口 */
#define xml_child(node) (node->child)
#define xml_parent(node) (node->parent)
#define xml_brother(node) (node->next)
#define xml_name(node) (node->name)
#define xml_value(node) (node->value)

xml_node_t *xml_node_creat(xml_tree_t *xml, xml_node_type_e type);
xml_node_t *xml_node_creat_ext(xml_tree_t *xml,
        xml_node_type_e type, const char *name, const char *value);
int xml_node_free(xml_tree_t *xml, xml_node_t *node);

xml_tree_t *xml_empty(xml_opt_t *opt);
xml_tree_t *xml_creat(const char *fname, xml_opt_t *opt);
xml_tree_t *xml_screat(const char *str, size_t len, xml_opt_t *opt);

int xml_fwrite(xml_tree_t *xml, const char *fname);
int xml_fprint(xml_tree_t *xml, FILE *fp);
int xml_sprint(xml_tree_t *xml, char *str);
int xml_spack(xml_tree_t *xml, char *str);

xml_node_t *xml_search(xml_tree_t *xml, xml_node_t *curr, const char *path);
#define xml_query(xml, path) xml_search(xml, xml->root, path)

xml_node_t *xml_add_node(
        xml_tree_t *xml, xml_node_t *node,
        const char *name, const char *value, int type);
xml_node_t *xml_add_attr(
        xml_tree_t *xml, xml_node_t *node,
        const char *name, const char *value);
xml_node_t *xml_add_child(
        xml_tree_t *xml, xml_node_t *node,
        const char *name, const char *value);
#define xml_set_root(xml, name) xml_add_child((xml), (xml)->root, (name), NULL)
#define xml_add_brother(xml, node, name, value) \
    xml_add_node(xml, (node)->parent, name, value, node->type)
int xml_delete_child(xml_tree_t *xml, xml_node_t *node, xml_node_t *child);
#define xml_delete_brother(xml, node, brother) \
    xml_delete_child(xml, (node)->parent, brother)
int xml_delete_empty(xml_tree_t *xml);

int xml_set_value(xml_tree_t *xml, xml_node_t *node, const char *value);

int xml_node_len(xml_tree_t *xml, xml_node_t *node);
/* 功能: 返回格式化XML字串的实际长度LEN
 * 注意: 必须申请(LEN + 1)存储XML字串, 多1个字符是为字串结束符'\0'预留空间 */
#define XML_TREE_LEN(xml) xml_node_len(xml, xml->root->child)

extern int _xml_pack_len(xml_tree_t *xml, xml_node_t *node);
/* 功能: 返回非格式化XML字串的实际长度LEN
 * 注意: 必须申请(LEN + 1)存储XML字串, 多1个字符是为字串结束符'\0'预留空间 */
#define XML_PACK_LEN(xml) _xml_pack_len(xml, xml->root)

#define xml_destroy(xml) do {\
    xml_node_free(xml, xml->root); \
    xml->dealloc(xml->pool, xml); \
    xml=NULL; \
} while(0)

#endif /*__XML_TREE_H__*/
