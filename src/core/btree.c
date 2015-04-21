#include "btree.h"

static btree_node_t *btree_creat_node(btree_t *btree);
static int _btree_insert(btree_t *btree, btree_node_t *node, int key, int idx);
static int btree_split(btree_t *btree, btree_node_t *node);
static int _btree_remove(btree_t *btree, btree_node_t *node, int idx);
static int btree_merge(btree_t *btree, btree_node_t *node);
static void _btree_print(const btree_node_t *node, int deep);
static int _btree_merge(btree_t *btree, btree_node_t *left, btree_node_t *right, int idx);


#define btree_print(btree) \
{ \
    if (NULL != btree->root) \
    { \
        _btree_print(btree->root, 0); \
    } \
}

/******************************************************************************
 **函数名称: btree_creat 
 **功    能: 创建B树
 **输入参数: 
 **     _btree: B树
 **     m: 阶m>=3
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.12 #
 ******************************************************************************/
int btree_creat(btree_t **_btree, int m)
{
    btree_t *btree = NULL;

    if (m < 3)
    {
        fprintf(stderr, "[%s][%d] Parameter 'm' must geater than 2!\n", __FILE__, __LINE__);
        return -1;
    }

    btree = (btree_t *)calloc(1, sizeof(btree_t));
    if (NULL == btree)
    {
        fprintf(stderr, "[%s][%d] errmsg:[%d] %s!\n", __FILE__, __LINE__, errno, strerror(errno));
        return -1;
    }

    btree->max= m - 1;
    btree->min = m / 2;
    if (0 != m%2)
    {
        btree->min++;
    }
    btree->min--;
    btree->sidx = m/2;
    btree->root = NULL;
    fprintf(stderr, "max:%d min:%d sidx:%d\n", btree->max, btree->min, btree->sidx);

    *_btree = btree;
    return 0;
}

/******************************************************************************
 **函数名称: btree_insert
 **功    能: 向B树中插入一个关键字
 **输入参数: 
 **     btree: B树
 **     key: 将被插入的关键字
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.12 #
 ******************************************************************************/
int btree_insert(btree_t *btree, int key)
{
    int idx;
    btree_node_t *node = btree->root;

    /* 1. 插入根结点 */
    if (NULL == node)
    {
        node = btree_creat_node(btree);
        if (NULL == node)
        {
            fprintf(stderr, "[%s][%d] Create node failed!\n", __FILE__, __LINE__);
            return -1;
        }

        node->num = 1; 
        node->key[0] = key;
        node->parent = NULL;

        btree->root = node;
        return 0;
    }

    /* 2. 查找关键字的插入位置 */
    while(NULL != node)
    {
        for(idx=0; idx<node->num; idx++)
        {
            if (key == node->key[idx])
            {
                fprintf(stderr, "[%s][%d] The node is exist!\n", __FILE__, __LINE__);
                return 0;
            }
            else if (key < node->key[idx])
            {
                break;
            }
        }

        if (NULL != node->child[idx])
        {
            node = node->child[idx];
        }
        else
        {
            break;
        }
    }

    /* 3. 执行插入操作 */
    return _btree_insert(btree, node, key, idx);
}

/******************************************************************************
 **函数名称: _btree_insert
 **功    能: 插入关键字到指定节点
 **输入参数: 
 **     btree: B树
 **     node: 指定节点
 **     key: 需被插入的关键字
 **     idx: 插入位置
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.12 #
 ******************************************************************************/
static int _btree_insert(btree_t *btree, btree_node_t *node, int key, int idx)
{
    int i;

    /* 1. 插入最底层的节点: 孩子节点都是空指针 */
    for(i=node->num; i>idx; i--)
    {
        node->key[i] = node->key[i-1];
        /* node->child[i+1] = node->child[i]; */
    }

    node->key[idx] = key;
    node->num++;

    /* 2. 分化节点 */
    if (node->num > btree->max)
    {
        return btree_split(btree, node);
    }

    return 0;
}

/******************************************************************************
 **函数名称: btree_split
 **功    能: 插入关键字到指定节点, 并进行分裂处理
 **输入参数: 
 **     btree: B树
 **     node: 指定节点
 **     key: 需被插入的关键字
 **     idx: 插入位置
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.12 #
 ******************************************************************************/
static int btree_split(btree_t *btree, btree_node_t *node)
{
    int idx, total, sidx = btree->sidx;
    btree_node_t *parent, *node2;

    while(node->num > btree->max)
    {
        /* Split node */
        total = node->num;

        node2 = btree_creat_node(btree);
        if (NULL == node2)
        {
            fprintf(stderr, "[%s][%d] Create node failed!\n", __FILE__, __LINE__);
            return -1;
        }

        /* Copy data */
        memcpy(node2->key, node->key+sidx+1, (total-sidx-1) * sizeof(int));
        memcpy(node2->child, node->child+sidx+1, (total-sidx) * sizeof(btree_node_t *));

        node2->num = (total - sidx - 1);
        node2->parent  = node->parent;

        node->num = sidx;

        /* Insert into parent */
	    parent  = node->parent;
        if (NULL == parent)
        {
            /* Split root node */
            parent = btree_creat_node(btree);
            if (NULL == parent)
            {
                fprintf(stderr, "[%s][%d] Create root failed!", __FILE__, __LINE__);
                return -1;
            }

            btree->root = parent;
            parent->child[0] = node;
            node->parent = parent;
            node2->parent = parent;

            parent->key[0] = node->key[sidx];
            parent->child[1] = node2;
            parent->num++;
        }
        else
        {
            /* Insert into parent node */
            for(idx=parent->num; idx>0; idx--)
            {
                if (node->key[sidx] < parent->key[idx-1])
                {
                    parent->key[idx] = parent->key[idx-1];
                    parent->child[idx+1] = parent->child[idx];
                }
                else
                {
                    parent->key[idx] = node->key[sidx];
                    parent->child[idx+1] = node2;
                    node2->parent = parent;
                    parent->num++;
                    break;
                }
            }

            if (0 == idx)
            {
                parent->key[0] = node->key[sidx];
                parent->child[1] = node2;
                node2->parent = parent;
                parent->num++;               
            }
        }

        memset(node->key+sidx, 0, (total - sidx) * sizeof(int));
        memset(node->child+sidx+1, 0, (total - sidx) * sizeof(btree_node_t *));

        /* Change node2's child->parent */
        for(idx=0; idx<=node2->num; idx++)
        {
            if (NULL != node2->child[idx])
            {
                node2->child[idx]->parent = node2;
            }
        }
        node = parent;
    }

    return 0;
}

/******************************************************************************
 **函数名称: btree_remove
 **功    能: 删除指定关键字
 **输入参数: 
 **     btree: B树
 **     key: 关键字
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.12 #
 ******************************************************************************/
int btree_remove(btree_t *btree, int key)
{
    int idx;
    btree_node_t *node = btree->root;

    while(NULL != node)
    {
        for(idx=0; idx<node->num; idx++)
        {
            if (key == node->key[idx])
            {
                return _btree_remove(btree, node, idx);
            }
            else if (key < node->key[idx])
            {
                break;
            }
        }

        node = node->child[idx];
    }

    return 0;
}

/******************************************************************************
 **函数名称: _btree_remove
 **功    能: 在指定结点删除指定关键字
 **输入参数: 
 **     btree: B树
 **     node: 指定结点
 **     idx: 将被删除的关键字在结点node中位置(0 ~ node->num - 1)
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     使用node->child[idx]中的最大值替代被删除的关键字, 
 **     并依次向下处理直至最底层结点, 
 **     -- 其实最终其处理过程相当于是删除最底层结点的关键字
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.12 #
 ******************************************************************************/
static int _btree_remove(btree_t *btree, btree_node_t *node, int idx)
{
    btree_node_t *orig = node, *child = node->child[idx];

    /* 使用node->child[idx]中的最大值替代被删除的关键字 */
    while(NULL != child)
    {
        node = child;
        child = node->child[child->num];
    }

	orig->key[idx] = node->key[node->num - 1];

    /* 最终其处理过程相当于是删除最底层结点的关键字 */
    node->key[--node->num] = 0;
    if (node->num < btree->min)
    {
        return btree_merge(btree, node);
    }

    return 0;
}

/******************************************************************************
 **函数名称: btree_merge
 **功    能: 合并结点
 **输入参数: 
 **     btree: B树
 **     node: 该结点关键字数num<min
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **    处理情况分类:
 **     1) 合并结点的情况: node->num + brother->num + 1 <= max
 **     2) 借用结点的情况: node->num + brother->num + 1 >  max
 **注意事项: 
 **     node此时为最底层结点
 **作    者: # Qifeng.zou # 2014.03.12 #
 ******************************************************************************/
static int btree_merge(btree_t *btree, btree_node_t *node)
{
    int idx = 0, m = 0, mid = 0;
    btree_node_t *parent = node->parent, *right = NULL, *left = NULL;

    /* 1. node是根结点, 不必进行合并处理 */
    if (NULL == parent)
    {
        if (0 == node->num)
        {
            if (NULL != node->child[0])
            {
                btree->root = node->child[0];
                node->child[0]->parent = NULL;
            }
            else
            {
                btree->root = NULL;
            }
            free(node);
        }
        return 0;
    }

    /* 2. 查找node是其父结点的第几个孩子结点 */
    for(idx=0; idx<=parent->num; idx++)
    {
        if (parent->child[idx] == node)
        {
            break;
        }
    }

    if (idx > parent->num)
    {
        fprintf(stderr, "[%s][%d] Didn't find node in parent's children array!\n", __FILE__, __LINE__);
        return -1;
    }
    /* 3. node: 最后一个孩子结点(left < node)
     * node as right child */
    else if (idx == parent->num)
    {
        mid = idx - 1;
        left = parent->child[mid];

        /* 1) 合并结点 */
        if ((node->num + left->num + 1) <= btree->max)
        {
            return _btree_merge(btree, left, node, mid);
        }

        /* 2) 借用结点:brother->key[num-1] */
        for(m=node->num; m>0; m--)
        {
            node->key[m] = node->key[m - 1];
            node->child[m+1] = node->child[m];
        }
        node->child[1] = node->child[0];

        node->key[0] = parent->key[mid];
        node->num++;
        node->child[0] = left->child[left->num];
        if (NULL != left->child[left->num])
        {
            left->child[left->num]->parent = node;
        }

        parent->key[mid] = left->key[left->num - 1];
        left->key[left->num - 1] = 0;
        left->child[left->num] = NULL;
        left->num--;
        return 0;
    }
    
    /* 4. node: 非最后一个孩子结点(node < right)
     * node as left child */
    mid = idx;
    right = parent->child[mid + 1];

    /* 1) 合并结点 */
    if ((node->num + right->num + 1) <= btree->max)
    {
        return _btree_merge(btree, node, right, mid);
    }

    /* 2) 借用结点: right->key[0] */
    node->key[node->num++] = parent->key[mid];
    node->child[node->num] = right->child[0];
    if (NULL != right->child[0])
    {
        right->child[0]->parent = node;
    }

    parent->key[mid] = right->key[0];
    for(m=0; m<right->num; m++)
    {
        right->key[m] = right->key[m+1];
        right->child[m] = right->child[m+1];
    }
    right->child[m] = NULL;
    right->num--;
    return 0;
}

/******************************************************************************
 **函数名称: _btree_merge
 **功    能: 合并结点
 **输入参数: 
 **     btree: B树
 **     node: 
 **     brother:
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.12 #
 ******************************************************************************/
static int _btree_merge(btree_t *btree, btree_node_t *left, btree_node_t *right, int mid)
{
    int m = 0;
    btree_node_t *parent = left->parent;

    left->key[left->num++] = parent->key[mid];

    memcpy(left->key + left->num, right->key, right->num*sizeof(int));
    memcpy(left->child + left->num, right->child, (right->num+1)*sizeof(int));
    for(m=0; m<=right->num; m++)
    {
        if (NULL != right->child[m])
        {
            right->child[m]->parent = left;
        }
    }
    left->num += right->num;

    for(m=mid; m<parent->num-1; m++)
    {
        parent->key[m] = parent->key[m+1];
        parent->child[m+1] = parent->child[m+2];
    }

    parent->key[m] = 0;
    parent->child[m+1] = NULL;
    parent->num--;
    free(right);

    /* Check */
    if (parent->num < btree->min)
    {
        return btree_merge(btree, parent);
    }

    return 0;
}

/******************************************************************************
 **函数名称: btree_destroy
 **功    能: 销毁B树
 **输入参数: 
 **     btree: B树
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.12 #
 ******************************************************************************/
int btree_destroy(btree_t **btree)
{
    if (NULL != (*btree)->root)
    {
        free((*btree)->root);
    }

    free(*btree);
    *btree = NULL;
    return 0;
}

/******************************************************************************
 **函数名称: _btree_print
 **功    能: 打印B树结构
 **输入参数: 
 **     node: B树结点
 **     deep: 结点深度
 **输出参数: NONE
 **返    回: 节点地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.12 #
 ******************************************************************************/
static void _btree_print(const btree_node_t *node, int deep)
{
    int idx, d, flag = 0;

    /* 1. Print Start */
    for(d=0; d<deep; d++)
    {
        if (d == deep-1)
        {
            fprintf(stderr, "|-------");
        }
        else
        {
            fprintf(stderr, "|        ");
        }
    }

    fprintf(stderr, "<%d| ", node->num);
    for(idx=0; idx<node->num; idx++)
    {
        fprintf(stderr, "%d ", node->key[idx]);
    }

    fprintf(stderr, ">\n");

    /* 2. Print node's children */
    for(idx=0; idx<node->num+1; idx++)
    {
        if (NULL != node->child[idx])
        { 
            _btree_print(node->child[idx], deep+1);
            flag = 1;
        }
    }

    if (1 == flag)
    {
        for(d=0; d<deep; d++)
        {
            fprintf(stderr, "|        ");
        }

        fprintf(stderr, "</%d| ", node->num);
        for(idx=0; idx<node->num; idx++)
        {
            fprintf(stderr, "%d ", node->key[idx]);
        }

        fprintf(stderr, ">\n");
    }
}

/******************************************************************************
 **函数名称: btree_creat_node
 **功    能: 创建一个节点
 **输入参数: 
 **     btree: B树
 **输出参数: NONE
 **返    回: 节点地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.12 #
 ******************************************************************************/
static btree_node_t *btree_creat_node(btree_t *btree)
{
    btree_node_t *node;

    node = (btree_node_t *)calloc(1, sizeof(btree_node_t));
    if (NULL == node)
    {
        fprintf(stderr, "[%s][%d] errmsg:[%d] %s\n", __FILE__, __LINE__, errno, strerror(errno));
        return NULL;
    }

    node->num = 0;

    /* More than (max) is for move */
    node->key = (int *)calloc(btree->max+1, sizeof(int));
    if (NULL == node->key)
    {
        free(node), node=NULL;
        fprintf(stderr, "[%s][%d] errmsg:[%d] %s\n", __FILE__, __LINE__, errno, strerror(errno));
        return NULL;
    }

    /* More than (max+1) is for move */
    node->child = (btree_node_t **)calloc(btree->max+2, sizeof(btree_node_t *));
    if (NULL == node->child)
    {
        free(node->key);
        free(node), node=NULL;
        fprintf(stderr, "[%s][%d] errmsg:[%d] %s\n", __FILE__, __LINE__, errno, strerror(errno));
        return NULL;
    }

    return node;
}
