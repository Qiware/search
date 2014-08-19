#if !defined(__XML_TREE_H__)
#define __XML_TREE_H__

#include <stdio.h>

/* 如需开启一下功能宏，请将其加入到编译选项中 */
/* 功能宏: 节点只有孩子节点或只有数值(Only child or value) */
/* #define __XML_OCOV__ */

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

#if !defined(false)
    #define false (0)
#endif
#if !defined(true)
    #define true (1)
#endif

/* XML节点类型 */
typedef enum
{
    XML_NODE_ROOT,     /* 根节点 */
    XML_NODE_CHILD,    /* 孩子节点 */
    XML_NODE_ATTR,     /* 属性节点 */
    XML_NODE_UNKNOWN,  /* 未知节点 */
    XML_NODE_TYPE_TOTAL = XML_NODE_UNKNOWN    /* 节点类型数 */
}xml_node_type_e;


/* XML节点 */
typedef struct _xml_node_t
{
    char *name;                 /* 节点名 */
    char *value;                /* 节点值 */
    xml_node_type_e type;       /* 节点类型 */

    struct _xml_node_t *next;   /* 兄弟节点链表 */
    struct _xml_node_t *firstchild;  /* 孩子节点链表头: 属性节点+孩子节点 */
    struct _xml_node_t *tail;   /* 孩子节点链表尾 # 构建/修改XML树时使用 # 提高操作效率 */
    struct _xml_node_t *parent; /* 父亲节点 */

    unsigned int flag;          /* 记录节点是否有孩子(XML_NODE_HAS_CHILD)、属性(XML_NODE_HAS_ATTR)、节点值(XML_NODE_HAS_VALUE) */
    struct _xml_node_t *temp;   /* 临时指针: 遍历XML树时，提高效率(其他情况下，此指针值无效) */    
}xml_node_t;

/* XML树 */
typedef struct
{
    xml_node_t *root;           /* 根节点: 注意root的第一个子节点才是真正的根节点 */
}xml_tree_t;

/* 对外的接口 */
#define xml_child(node) (node->firstchild)
#define xml_parent(node) (node->parent)
#define xml_brother(node) (node->next)
#define xml_name(node) (node->name)
#define xml_value(node) (node->value)

extern xml_node_t *xml_node_creat(xml_node_type_e type);
extern xml_node_t *xml_node_creat_ext(xml_node_type_e type, const char *name, const char *value);
extern int xml_node_free(xml_node_t *node);

extern xml_tree_t *xml_creat(const char *fname);
extern xml_tree_t *xml_screat(const char *str);
extern xml_tree_t *xml_screat_ext(const char *str, int length);

extern int xml_fwrite(xml_tree_t *xml, const char *fname);
extern int xml_fprint(xml_tree_t *xml, FILE *fp);
extern int xml_sprint(xml_tree_t *xml, char *str);
extern int xml_spack(xml_tree_t *xml, char *str);

extern xml_node_t *xml_rsearch(xml_node_t *curr, const char *path);
#define xml_search(xml, path) xml_rsearch(xml->root, path)

extern xml_node_t *xml_add_node(xml_node_t *node, const char *name, const char *value, int type);
extern xml_node_t *xml_add_attr(xml_node_t *node, const char *name, const char *value);
extern xml_node_t *xml_add_child(xml_node_t *node, const char *name, const char *value);
#define xml_add_brother(node, name, value) xml_add_node((node)->parent, name, value, node->type)
extern int xml_delete_child(xml_node_t *node, xml_node_t *child);
#define xml_delete_brother(node, brother) xml_delete_child((node)->parent, brother)
extern int xml_delete_empty(xml_tree_t *xml);

extern int xml_set_value(xml_node_t *node, const char *value);
extern int xml_node_length(xml_node_t *node);
#define xml_tree_length(xml) xml_node_length(xml->root->firstchild)

extern int _xml_pack_length(xml_node_t *node);
#define xml_pack_length(xml) _xml_pack_length(xml->root)

#define xml_destroy(xml) {xml_node_free(xml->root); free(xml); xml=NULL;}

#endif /*__XML_TREE_H__*/
