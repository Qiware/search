#include "rb_tree.h"
#include <assert.h>

#define __RB_STACK_PRINT__

static int rb_insert_fixup(rbt_tree_t *tree, rbt_node_t *node);
static int _rb_delete(rbt_tree_t *tree, rbt_node_t *dnode);
static int rbt_delete_fixup(rbt_tree_t *tree, rbt_node_t *node);

/******************************************************************************
 **函数名称: rbt_assert
 **功    能: 检测结点是否正常
 **输入参数: 
 **     node: 被检测的结点
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: # 非常有效 #
 **作    者: # Qifeng.zou # 2013.12.20 #
 ******************************************************************************/
void rbt_assert(const rbt_tree_t *tree, const rbt_node_t *node)
{
    if((NULL == node)
        || (tree->sentinel == node)
        || (tree->sentinel == node->parent)) 
    {
        return;
    }

    if((node->parent == node->lchild)
        || (node->parent == node->rchild))
    {
        assert(0);
    }

    if((node->parent->lchild != node)
        && (node->parent->rchild != node)
        && (tree->sentinel != node)) 
    {
        assert(0);
    }

    if((node->parent == node->lchild)
        || (node->parent == node->rchild))
    {
        assert(0);
    }
}

/******************************************************************************
 **函数名称: rbt_right_rotate
 **功    能: 右旋处理
 **输入参数: 
 **     tree: 红黑树
 **     node: 旋转支点
 **输出参数: NONE
 **返    回: RBT_SUCCESS:成功 RBT_FAILED:失败
 **实现描述: 
 **        G                       G
 **        |                       |
 **        N            ->         L
 **      /   \                   /   \
 **     L     R                 LL    N
 **    / \   / \                     / \
 **   LL LR RL RR                   LR  R
 **                                    / \
 **                                   RL RR
 **            说明: 节点N为旋转支点
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.01.15 #
 ******************************************************************************/
void rbt_right_rotate(rbt_tree_t *tree, rbt_node_t *node)
{
    rbt_node_t *parent = node->parent, *lchild = node->lchild;
    
    if(tree->sentinel == parent)
    {
        tree->root = lchild;
        lchild->parent = tree->sentinel;
    }
    else if(node == parent->lchild)
    {
        rbt_set_lchild(tree, parent, lchild);
    }
    else
    {
        rbt_set_rchild(tree, parent, lchild);
    }
    rbt_set_lchild(tree, node, lchild->rchild);
    rbt_set_rchild(tree, lchild, node);

    rbt_assert(tree, node);
    rbt_assert(tree, lchild);
    rbt_assert(tree, parent);
}

/******************************************************************************
 **函数名称: rbt_left_rotate
 **功    能: 左旋处理
 **输入参数: 
 **     tree: 红黑树
 **     node: 旋转支点
 **输出参数: NONE
 **返    回: RBT_SUCCESS:成功 RBT_FAILED:失败
 **实现描述: 
 **        G                       G
 **        |                       |
 **        N            ->         R
 **      /   \                   /   \
 **     L     R                 N    RR
 **    / \   / \               / \
 **   LL LR RL RR             L  RL
 **                          / \
 **                         LL LR
 **            说明: 节点N为旋转支点
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.01.15 #
 ******************************************************************************/
void rbt_left_rotate(rbt_tree_t *tree, rbt_node_t *node)
{
    rbt_node_t *parent = node->parent, *rchild = node->rchild;
    
    if(tree->sentinel == parent)
    {
        tree->root = rchild;
        rchild->parent = tree->sentinel;
    }
    else if(node == parent->lchild)
    {
        rbt_set_lchild(tree, parent, rchild);
    }
    else
    {
        rbt_set_rchild(tree, parent, rchild);
    }
    rbt_set_rchild(tree, node, rchild->lchild);
    rbt_set_lchild(tree, rchild, node);

    rbt_assert(tree, node);
    rbt_assert(tree, rchild);
    rbt_assert(tree, parent);
}

/******************************************************************************
 **函数名称: rbt_creat
 **功    能: 创建红黑树对象(对外接口)
 **输入参数: 
 **     tree: 红黑树
 **输出参数: NONE
 **返    回: RBT_SUCCESS:成功 RBT_FAILED:失败
 **实现描述: 
 **注意事项: 
 **     ①、每个结点要么是红色的，要么是黑色的；
 **     ②、根结点是黑色的；
 **     ③、所有叶子结点（NIL）都是黑色的；
 **     ④、如果一个结点是红色，则它的两个儿子都是黑色的；
 **     ⑤、对任何一个结点，从该结点通过其子孙结点到达叶子结点（NIL）
 **         的所有路径上包含相同数目的黑结点。
 **作    者: # Qifeng.zou # 2013.12.21 #
 ******************************************************************************/
rbt_tree_t *rbt_creat(void)
{
    rbt_tree_t *tree = NULL;
    
    tree = (rbt_tree_t *)calloc(1, sizeof(rbt_tree_t));
    if(NULL == tree)
    {
        return NULL;
    }

    tree->sentinel = (rbt_node_t *)calloc(1, sizeof(rbt_node_t));
    if(NULL == tree->sentinel)
    {
        free(tree);
        return NULL;
    }

    tree->sentinel->color = RBT_COLOR_BLACK;
    tree->root = tree->sentinel;

    return tree;
}

/******************************************************************************
 **函数名称: rbt_insert
 **功    能: 创建关键字为key的节点(内部接口)
 **输入参数: 
 **     key: 红黑树
 **     color: 节点颜色
 **     type: 新增节点是父节点的左孩子还是右孩子
 **     parent: 父节点
 **输出参数: NONE
 **返    回: RBT_SUCCESS:成功 RBT_FAILED:失败
 **实现描述: 
 **注意事项: 新结点的左右孩子肯定为叶子结点
 **作    者: # Qifeng.zou # 2013.12.23 #
 ******************************************************************************/
rbt_node_t *rbt_creat_node(rbt_tree_t *tree, int key, int color, int type, rbt_node_t *parent)
{
    rbt_node_t *node = NULL;

    node = (rbt_node_t *)calloc(1, sizeof(rbt_node_t));
    if(NULL == node)
    {
        return NULL;
    }

    node->color = color;
    node->key = key;
    node->lchild = tree->sentinel;
    node->rchild = tree->sentinel;
    if(NULL != parent)
    {
        rbt_set_child(tree, parent, type, node);
    }
    else
    {
        node->parent = tree->sentinel;
    }

    return node;
}

/******************************************************************************
 **函数名称: rbt_insert
 **功    能: 向红黑树中增加节点(对外接口)
 **输入参数: 
 **     tree: 红黑树
 **     key: 需被添加的关键字
 **输出参数: NONE
 **返    回: RBT_SUCCESS:成功 RBT_FAILED:失败 RBT_NODE_EXIST:节点存在
 **实现描述: 
 **     1. 当根节点为空时，直接添加
 **     2. 将节点插入树中, 检查并修复新节点造成红黑树性质的破坏
 **注意事项: 
 **  红黑树的5点性质:
 **     1、每个结点要么是红色的，要么是黑色的；
 **     2、根结点是黑色的；
 **     3、所有叶子结点(NIL)都是黑色的；
 **     4、如果一个结点是红色，则它的两个儿子都是黑色的；
 **     5、对任何一个结点，从该结点通过其子孙结点到达叶子结点(NIL)
 **         的所有路径上包含相同数目的黑结点。
 **注意事项: 插入节点操作只可能破坏性质(4)
 **作    者: # Qifeng.zou # 2013.12.23 #
 ******************************************************************************/
int rbt_insert(rbt_tree_t *tree, int key)
{
    rbt_node_t *node = tree->root,
              *add = NULL, *parent = NULL;

    /* 1. 当根节点为空时，直接添加 */
    if(tree->sentinel == tree->root)
    {
        /* 性质2: 根结点是黑色的 */
        tree->root = rbt_creat_node(tree, key, RBT_COLOR_BLACK, 0, NULL);
        if(tree->sentinel == tree->root)
        {
            return RBT_FAILED;
        }
        return RBT_SUCCESS;
    }
    
    /* 2. 将节点插入树中, 检查并修复新节点造成红黑树性质的破坏 */
    while(tree->sentinel != node)
    {
        if(key == node->key)
        {
            return RBT_NODE_EXIST;
        }
        else if(key < node->key)
        {
            if(tree->sentinel == node->lchild)
            {
                add = rbt_creat_node(tree, key, RBT_COLOR_RED, RBT_LCHILD, node);
                if(NULL == add)
                {
                    return RBT_FAILED;
                }
                
                return rb_insert_fixup(tree, add); /* 防止红黑树的性质被破坏 */
            }
            node = node->lchild;
        }
        else
        {
            if(tree->sentinel == node->rchild)
            {
                add = rbt_creat_node(tree, key, RBT_COLOR_RED, RBT_RCHILD, node);
                if(NULL == add)
                {
                    return RBT_FAILED;
                }

                return rb_insert_fixup(tree, add); /* 防止红黑树的性质被破坏 */
            }
            node = node->rchild;
        }
    }

    return RBT_SUCCESS;
}

/******************************************************************************
 **函数名称: rb_insert_fixup
 **功    能: 插入操作修复(内部接口)
 **输入参数: 
 **     tree: 红黑树
 **     node: 新增节点的地址
 **输出参数: NONE
 **返    回: RBT_SUCCESS:成功 RBT_FAILED:失败
 **实现描述: 
 **     1. 检查红黑树性质是否被破坏
 **     2. 如果被破坏，则进行对应的处理
 **注意事项: 插入节点操作只可能破坏性质④
 **作    者: # Qifeng.zou # 2013.12.23 #
 ******************************************************************************/
static int rb_insert_fixup(rbt_tree_t *tree, rbt_node_t *node)
{
    rbt_node_t *parent = NULL, *uncle = NULL, *grandpa = NULL, *gparent = NULL;

    while(rbt_is_red(node))
    {
        parent = node->parent;
        if(rbt_is_black(parent))
        {
            return RBT_SUCCESS;
        }
        
        grandpa = parent->parent;
        if(parent == grandpa->lchild) /* 父节点为左节点 */
        {
            uncle = grandpa->rchild;
            
            /* case 1: 父节点和叔节点为红色 */
            if(rbt_is_red(uncle))
            {
                rbt_set_black(parent);
                rbt_set_black(uncle);
                if(grandpa != tree->root)
                {
                    rbt_set_red(grandpa);
                }
                node = grandpa;
                continue;
            }
            /* case 2: 叔结点为黑色，结点为左孩子 */
            else if(node == parent->lchild)
            {
                /* 右旋转: 以grandpa为支点 */
                gparent = grandpa->parent;
                rbt_set_red(grandpa);
                rbt_set_black(parent);

                rbt_right_rotate(tree, grandpa);
                node = gparent;
                continue;
            }
            /* case 3: 叔结点为黑色，结点为右孩子 */
            else
            {
                /* 左旋转: 以parent为支点 */
                rbt_left_rotate(tree, parent);
                
                node = parent;
                continue;
            }
        }
        else                        /* 父节点为右孩子 */
        {
            uncle = grandpa->lchild;
            
            /* case 1: 父节点和叔节点为红色 */
            if(rbt_is_red(uncle))
            {
                rbt_set_black(parent);
                rbt_set_black(uncle);
                if(grandpa != tree->root)
                {
                    rbt_set_red(grandpa);
                }

                node = grandpa;
                continue;
            }
            /* case 2: 叔结点为黑色，结点为左孩子 */
            else if(node == parent->lchild)
            {
                /* 右旋转: 以parent为支点 */
                rbt_right_rotate(tree, parent);
                node = parent;
                continue;
            }
            /* case 3: 叔结点为黑色，结点为右孩子 */
            else
            {
                /* 左旋转: 以grandpa为支点 */
                gparent = grandpa->parent;
                rbt_set_black(parent);
                rbt_set_red(grandpa);

                rbt_left_rotate(tree, grandpa);
                node = gparent;
                continue;
            }
        }
    }

    return RBT_SUCCESS;
}

/******************************************************************************
 **函数名称: rbt_delete
 **功    能: 删除结点(外部接口)
 **输入参数: 
 **     tree: 红黑树
 **     key: 关键字
 **输出参数: NONE
 **返    回: RBT_SUCCESS:成功  RBT_FAILED:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.12.27 #
 ******************************************************************************/
int rbt_delete(rbt_tree_t *tree, int key)
{
    rbt_node_t *node = tree->root;

    while(tree->sentinel != node)
    {
        if(key == node->key)
        {
            return _rb_delete(tree, node);
        }
        else if(key < node->key)
        {
            node = node->lchild;
        }
        else
        {
            node = node->rchild;
        }
    }

    return RBT_SUCCESS;
}

/******************************************************************************
 **函数名称: rbt_delete
 **功    能: 删除结点(内部接口)
 **输入参数: 
 **     tree: 红黑树
 **     dnode: 将被删除的结点
 **输出参数: NONE
 **返    回: RBT_SUCCESS:成功  RBT_FAILED:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.12.28 #
 ******************************************************************************/
static int _rb_delete(rbt_tree_t *tree, rbt_node_t *dnode)
{
    rbt_node_t *parent = NULL, *next = NULL, *refer = NULL;

#if 1
    /* Case 1: 被删结点D的左孩子为叶子结点, 右孩子无限制(可为叶子结点，也可为非叶子结点) */
    if(tree->sentinel == dnode->lchild)
    {
        parent = dnode->parent;
        refer = dnode->rchild;

        refer->parent = parent;
        if(tree->sentinel == parent)
        {
            tree->root = refer;
        }
        else if(dnode == parent->lchild)
        {
            parent->lchild = refer;
        }
        else /* dnode == parent->rchild */
        {
            parent->rchild = refer;
        }

        if(rbt_is_red(dnode))
        {
            free(dnode);
            return RBT_SUCCESS;
        }

        free(dnode);
        return rbt_delete_fixup(tree, refer);
    }
    /* Case 2: 被删结点D的右孩子为叶子结点, 左孩子不为叶子结点 */
    else if(tree->sentinel == dnode->rchild)
    {
        parent = dnode->parent;
        refer = dnode->lchild;

        refer->parent = parent;
        if(tree->sentinel == parent)
        {
            tree->root = refer;
        }
        else if(dnode == parent->lchild)
        {
            parent->lchild = refer;
        }
        else /* dnode == parent->rchild */
        {
            parent->rchild = refer;
        }

        if(rbt_is_red(dnode))
        {
            free(dnode);
            return RBT_SUCCESS;
        }

        free(dnode);
        return rbt_delete_fixup(tree, refer);
    }

    /* Case 3: 被删结点D的左右孩子均不为叶子节点 */
    /* 3.1 查找dnode的后继结点next */
    next = dnode->rchild;
    while(tree->sentinel != next->lchild)
    {
        next = next->lchild;
    }

    parent = next->parent;
    refer = next->rchild;

    refer->parent = parent;
    if(next == parent->lchild)
    {
        parent->lchild = refer;
    }
    else /* next == parent->rchild */
    {
        parent->rchild = refer;
    }

    dnode->key = next->key;

    if(rbt_is_red(next)) /* Not black */
    {
        free(next);
        return RBT_SUCCESS;
    }

    free(next);

    return rbt_delete_fixup(tree, refer);

#else
    /* 查找dnode的后继结点next */
    if((tree->sentinel == dnode->lchild)
        || (tree->sentinel == dnode->rchild))
    {
        next = dnode;
    }
    else
    {
        next = dnode->rchild;
        while(tree->sentinel != next->lchild)
        {
            next = next->lchild;
        }
    }

    /* 设置替代后继结点的结点refer(参考结点) */
    if(tree->sentinel != next->lchild)
    {
        refer = next->lchild;
    }
    else
    {
        refer = next->rchild;
    }

    refer->parent = next->parent;
    if(tree->sentinel == next->parent)
    {
        tree->root = refer;
    }
    else
    {
        if(next == next->parent->lchild)
        {
            next->parent->lchild = refer;
        }
        else
        {
            next->parent->rchild = refer;
        }
    }

    if(next != dnode)
    {
        dnode->key = next->key;
        /* Copy next's satellite data into dnode */
    }

    if(rbt_is_red(next)) /* Not black */
    {
        free(next);
        return RBT_SUCCESS;
    }

    free(next);

    return rbt_delete_fixup(tree, refer);
#endif
}

/******************************************************************************
 **函数名称: rbt_delete_fixup
 **功    能: 修复删除操作造成的黑红树性质的破坏(内部接口)
 **输入参数: 
 **     tree: 红黑树
 **     node: 实际被删结点的替代结点(注: node有可能是叶子结点)
 **输出参数: NONE
 **返    回: RBT_SUCCESS:成功  RBT_FAILED:失败
 **实现描述: 
 **注意事项: 
 **     注意: 被删结点为黑色结点，才能调用此函数进行性质调整
 **作    者: # Qifeng.zou # 2013.12.28 #
 ******************************************************************************/
static int rbt_delete_fixup(rbt_tree_t *tree, rbt_node_t *node)
{
    rbt_node_t *parent = NULL, *brother = NULL;

    while(rbt_is_black(node) && (tree->root != node))
    {   
        /* Set parent and brother */
        parent = node->parent;
        
        if(node == parent->lchild)
        {
            brother = parent->rchild;

            /* Case 1: 兄弟结点为红色:  以parent为支点, 左旋处理 */
            if(rbt_is_red(brother))
            {
                rbt_set_red(parent);
                rbt_set_black(brother);
                rbt_left_rotate(tree, parent);

                /* 参照结点node不变, 兄弟结点改为parent->rchild */
                brother = parent->rchild;
                
                /* 注意: 此时处理还没有结束，还需要做后续的调整处理 */
            }

            /* Case 2: 兄弟结点为黑色(默认), 且兄弟结点的2个子结点都为黑色 */
            if(rbt_is_black(brother->lchild) && rbt_is_black(brother->rchild))
            {
                rbt_set_red(brother);
                node = parent;
            }
            else 
            {
                /* Case 3: 兄弟结点为黑色(默认),
                    兄弟节点的左子结点为红色, 右子结点为黑色:  以brother为支点, 右旋处理 */
                if(rbt_is_black(brother->rchild))
                {
                    rbt_set_black(brother->lchild);
                    rbt_set_red(brother);

                    rbt_right_rotate(tree, brother);

                    /* 参照结点node不变 */
                    brother = parent->rchild;
                }
                
                /* Case 4: 兄弟结点为黑色(默认),
                    兄弟结点右孩子结点为红色:  以parent为支点, 左旋处理 */
                rbt_copy_color(brother, parent);
                rbt_set_black(brother->rchild);
                rbt_set_black(parent);

                rbt_left_rotate(tree, parent);
                
                node = tree->root;
            }
        }
        else
        {
            brother = parent->lchild;

            /* Case 5: 兄弟结点为红色:  以parent为支点, 右旋处理 */
            if(rbt_is_red(brother))
            {
                rbt_set_red(parent);
                rbt_set_black(brother);

                rbt_right_rotate(tree, parent);

                /* 参照结点node不变 */
                brother = parent->lchild;
                
                /* 注意: 此时处理还没有结束，还需要做后续的调整处理 */
            }

            /* Case 6: 兄弟结点为黑色(默认), 且兄弟结点的2个子结点都为黑色 */
            if(rbt_is_black(brother->lchild) && rbt_is_black(brother->rchild))
            {
                rbt_set_red(brother);
                node = parent;
            }
            else
            {
                /* Case 7: 兄弟结点为黑色(默认),
                    兄弟节点的右子结点为红色, 左子结点为黑色:  以brother为支点, 左旋处理 */
                if(rbt_is_black(brother->lchild))
                {
                    rbt_set_red(brother);
                    rbt_set_black(brother->rchild);

                    rbt_left_rotate(tree, brother);

                    /* 参照结点node不变 */
                    brother = parent->lchild;
                }
            
                /* Case 8: 兄弟结点为黑色(默认),
                    兄弟结点左孩子结点为红色: 以parent为支点, 右旋处理 */
                rbt_copy_color(brother, parent);
                rbt_set_black(brother->lchild);
                rbt_set_black(parent);

                rbt_right_rotate(tree, parent);
                
                node = tree->root;
            }
        }
    }

    rbt_set_black(node);
    
    rbt_assert(tree, node);
    rbt_assert(tree, brother);
    rbt_assert(tree, parent);
    return RBT_SUCCESS;
}

/******************************************************************************
 **函数名称: rbt_hprint
 **功    能: 打印结点头(内部接口)
 **输入参数: 
 **     node: 被打印的结点
 **     depth: 结点深度
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.12.17 #
 ******************************************************************************/
static void rbt_hprint(const rbt_tree_t *tree, const rbt_node_t *node, int depth)
{
    int idx = 0;
    rbt_node_t *parent = node->parent;

    while(depth > 0 && (NULL != parent))
    {
        if(1 == depth)
        {
            fprintf(stderr, "|");
            for(idx=0; idx<8; idx++)
            {
                if(0 == idx)
                {
                    if(parent->lchild == node)
                    {
                        fprintf(stderr, "l");
                    }
                    else
                    {
                        fprintf(stderr, "r");
                    }
                }
                else
                {
                    fprintf(stderr, "-");
                }
            }
        }
        else
        {
            fprintf(stderr, "|");
            for(idx=0; idx<8; idx++)
            {
                fprintf(stderr, " ");
            }
        }
        depth--;
    }
    
    if((tree->sentinel == node->lchild)
        && (tree->sentinel == node->rchild))
    {
        fprintf(stderr, "<%03d:%c/>\n", node->key, node->color);
    }
    else
    {
        fprintf(stderr, "<%03d:%c>\n", node->key, node->color);
    }
}

/******************************************************************************
 **函数名称: avl_tprint
 **功    能: 打印结点尾(内部接口)
 **输入参数: 
 **     node: 被打印的结点
 **     depth: 结点深度
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.12.17 #
 ******************************************************************************/
static void rbt_tprint(const rbt_tree_t *tree, const rbt_node_t *node, int depth)
{
    int idx = 0;

    if((tree->sentinel == node->lchild)
        && (tree->sentinel == node->rchild))
    {
        return;
    }

    while(depth > 0)
    {
        fprintf(stderr, "|");
        for(idx=0; idx<8; idx++)
        {
            fprintf(stderr, " ");
        }
        depth--;
    }

    fprintf(stderr, "</%03d>\n", node->key);
}

/******************************************************************************
 **函数名称: rbt_print
 **功    能: 打印红黑树(外部接口)
 **输入参数: 
 **     tree: 红黑树
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.12.17 #
 ******************************************************************************/
int rbt_print(rbt_tree_t *tree)
{
    int depth = 0;
    Stack_t _stack, *stack = &_stack;
    rbt_node_t *node = tree->root, *parent = NULL;

    if(tree->sentinel == node) return 0;

    stack_init(stack, RBT_MAX_DEPTH);

    while(tree->sentinel != node)
    {
        /* 压左孩子入栈 */
        while(tree->sentinel != node->lchild)
        {
            rbt_assert(tree, node);
            
            depth = stack_depth(stack);

            stack_push(stack, node);
                        
            rbt_hprint(tree, node, depth);   /* 打印头：入栈时打印头 出栈时打印尾 */

            node = node->lchild;
        }

        /* 打印最左端的子孙结点 */
        depth = stack_depth(stack);
        rbt_hprint(tree, node, depth);

        /* 最左端的孩子有右孩子 */
        if(tree->sentinel != node->rchild)
        {
            stack_push(stack, node);
            node = node->rchild;
            continue;
        }
        
        /* 最左端的孩子无右孩子 */
        rbt_tprint(tree, node, depth);

        parent = stack_gettop(stack);
        if(NULL == parent)
        {
            return stack_destroy(stack);
        }

        /* 判断最左结点的父结点未处理完成 */
        if((parent->lchild == node)
            && (tree->sentinel != parent->rchild))
        {
            node = parent->rchild;
            continue;
        }

        /* 判断最左结点的父结点已处理完成 */
        while((node == parent->rchild)
            || (tree->sentinel == parent->rchild))
        {
            stack_pop(stack);
            
            depth = stack_depth(stack);
            rbt_tprint(tree, parent, depth);    /* 打印尾：出栈时打印尾 入栈时已打印头 */

            node = parent;
            parent = stack_gettop(stack);
            if(NULL == parent)
            {
                return stack_destroy(stack);
            }
        }
        
        node = parent->rchild;
    }

    return stack_destroy(stack);
}

/******************************************************************************
 **函数名称: rbt_search
 **功    能: 搜索指定关键字节点(外部接口)
 **输入参数: 
 **     tree: 红黑树
 **     key: 关键字
 **输出参数: NONE
 **返    回: 查找到的节点地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.12.23 #
 ******************************************************************************/
const rbt_node_t *rbt_search(const rbt_tree_t *tree, int key)
{
    const rbt_node_t *node = tree->root;

    while(NULL != node)
    {
        if(key == node->key)
        {
            return node;
        }
        else if(key < node->key)
        {
            node = node->lchild;
        }
        else
        {
            node = node->rchild;
        }
    }
    
    return node;
}

/******************************************************************************
 **函数名称: rbt_destroy
 **功    能: 销毁红黑树(外部接口)
 **输入参数: 
 **     tree: 红黑树
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **     1. 左右孩子均为叶子结点的结点直接释放
 **     2. 其他树内结点出栈时释放
 **     3. 叶子结点最后释放
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.12.27 #
 ******************************************************************************/
int rbt_destroy(rbt_tree_t **tree)
{
    Stack_t _stack, *stack = &_stack;
    rbt_node_t *node = (*tree)->root, *parent = NULL, *top = NULL;

    if((*tree)->sentinel == node) return 0;

    stack_init(stack, RBT_MAX_DEPTH);

    while((*tree)->sentinel != node)
    {
        /* 压左孩子入栈 */
        while((*tree)->sentinel != node->lchild)
        {
            stack_push(stack, node);
            
            node = node->lchild;
        }

        /* 最左端的孩子有右孩子 */
        if((*tree)->sentinel != node->rchild)
        {
            stack_push(stack, node);
            node = node->rchild;
            continue;
        }
        
        parent = stack_gettop(stack);
        if(NULL == parent)
        {
            free(node);
            free((*tree)->sentinel);
            free(*tree), *tree = NULL;
            stack_destroy(stack);
            return RBT_SUCCESS;
        }

        if((parent->lchild == node) /* 右孩子是否已处理 */
            && ((*tree)->sentinel != parent->rchild))
        {
            free(node);
            node = parent->rchild;
            continue;
        }

        /* 其他树内结点出栈时释放 */
        while((node == parent->rchild)
            || ((*tree)->sentinel == parent->rchild))
        {
            stack_pop(stack);

            free(node);     /* 出栈结点下一次循环时释放 */

            node = parent;
            parent = stack_gettop(stack);
            if(NULL == parent)
            {
                free(node);
                free((*tree)->sentinel);
                free(*tree), *tree = NULL;
                stack_destroy(stack);
                return RBT_SUCCESS;
            }
        }

        if(NULL != node)    /* 释放上面出栈的结点 */
        {
            free(node);
        }
        node = parent->rchild;
    }

    free((*tree)->sentinel);
    free(*tree), *tree = NULL;
    stack_destroy(stack);
    return RBT_SUCCESS;
}
