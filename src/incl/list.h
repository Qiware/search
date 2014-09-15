#if !defined(__LIST_H__)
#define __LIST_H__

/* 单向链表结点 */
typedef struct _list_node_t
{
    void *data;
    struct _list_node_t *next;
} list_node_t;

/* 单向链表对象 */
typedef struct
{
    int num;
    list_node_t *head;
    list_node_t *tail;
} list_t;

int32_t list_insert(list_t *list, list_node_t *node);
list_node_t *list_delete(list_t *list);
int32_t list_insert_tail(list_t *list, list_node_t *node);
list_node_t *list_delete_tail(list_t *list);
void list_assert(list_t *list);
#endif /*__LIST_H__*/
