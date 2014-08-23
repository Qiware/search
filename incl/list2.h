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

int list2_insert_head(list2_t *list, void *data);
int list2_insert_head_ex(list2_t *list, list2_node_t *node);
int list2_insert_tail(list2_t *list, void *data);
int list2_insert_tail_ex(list2_t *list, list2_node_t *node);
#endif /*__LIST2_H__*/
