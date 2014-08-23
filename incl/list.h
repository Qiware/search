#if !defined(__LIST_H__)
#define __LIST_H__

/* 单向链表结点 */
typedef struct _list_node_t
{
    void *data;
    struct _list_node_t *next;
}list_node_t;

/* 单向链表对象 */
typedef struct
{
    int num;
    list_node_t *head;
    list_node_t *tail;
}list_t;

int list_insert_head(list_t *list, void *data);
int list_insert_head_ex(list_t *list, list_node_t *node);

int list_insert_tail(list_t *list, void *data);
int list_insert_tail_ex(list_t *list, list_node_t *node);
#endif /*__LIST_H__*/
