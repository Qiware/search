#if !defined(__LIST2_H__)
#define __LIST2_H__

/* 双向链表结点 */
typedef struct _list2_node_t
{
    void *data;
    struct _list2_node_t *prev;
    struct _list2_node_t *next;
}list2_node_t;

/* 双向链表对象 */
typedef struct
{
    int num;
    list2_node_t *head;
}list2_t;

int32_t list2_insert(list2_t *list, list2_node_t *node);
list2_node_t *list2_delete(list2_t *list);
int32_t list2_insert_tail(list2_t *list, list2_node_t *node);
list2_node_t *list2_delete_tail(list2_t *list);
#endif /*__LIST2_H__*/
