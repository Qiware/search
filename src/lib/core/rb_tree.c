/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: rb_tree.c
 ** 版本号: 1.0
 ** 描  述: 红黑树
 **         1. 红黑树是一种性能优异的二叉查找树，其时间复杂度为O(lg^n).
 **         2. 与平衡二叉树相比，其插入、删除性能更加优异。如果构建的树，其查询
 **            次数远高于增加、删除次数的话，应该优先考虑使用平衡二叉树；如果需
 **            要频繁的进行增加、删除操作，则应该考虑使用红黑树。
 ** 作  者: # Qifeng.zou # 2013.12.28 #
 ******************************************************************************/
#include "rb_tree.h"

static int rbt_insert_fixup(rbt_tree_t *tree, rbt_node_t *node);
static int _rbt_delete(rbt_tree_t *tree, rbt_node_t *dnode);
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
    if ((NULL == node)
        || (tree->sentinel == node)
        || (tree->sentinel == node->parent))
    {
        return;
    }

    if ((node->parent == node->lchild)
        || (node->parent == node->rchild))
    {
        assert(0);
    }

    if ((node->parent->lchild != node)
        && (node->parent->rchild != node)
        && (tree->sentinel != node))
    {
        assert(0);
    }

    if ((node->parent == node->lchild)
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
 **返    回: RBT_OK:成功 RBT_ERR:失败
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
 **            说明: 结点N为旋转支点
 **注意事项:
 **作    者: # Qifeng.zou # 2014.01.15 #
 ******************************************************************************/
static void rbt_right_rotate(rbt_tree_t *tree, rbt_node_t *node)
{
    rbt_node_t *parent = node->parent, *lchild = node->lchild;

    if (tree->sentinel == parent) {
        tree->root = lchild;
        lchild->parent = tree->sentinel;
    }
    else if (node == parent->lchild) {
        rbt_set_lchild(tree, parent, lchild);
    }
    else {
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
 **返    回: RBT_OK:成功 RBT_ERR:失败
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
 **            说明: 结点N为旋转支点
 **注意事项:
 **作    者: # Qifeng.zou # 2014.01.15 #
 ******************************************************************************/
static void rbt_left_rotate(rbt_tree_t *tree, rbt_node_t *node)
{
    rbt_node_t *parent = node->parent, *rchild = node->rchild;

    if (tree->sentinel == parent) {
        tree->root = rchild;
        rchild->parent = tree->sentinel;
    }
    else if (node == parent->lchild) {
        rbt_set_lchild(tree, parent, rchild);
    }
    else {
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
 **     opt: 参数选项
 **     cmp_cb: 键值比较函数
 **输出参数: NONE
 **返    回: RBT_OK:成功 RBT_ERR:失败
 **实现描述:
 **注意事项:
 **     ①、每个结点要么是红色的，要么是黑色的；
 **     ②、根结点是黑色的；
 **     ③、所有叶子结点（NIL）都是黑色的；
 **     ④、如果一个结点是红色，则它的两个儿子都是黑色的；
 **     ⑤、对任何一个结点，从该结点通过其子孙结点到达叶子结点（NIL）
 **        的所有路径上包含相同数目的黑结点。
 **作    者: # Qifeng.zou # 2013.12.21 #
 ******************************************************************************/
rbt_tree_t *rbt_creat(rbt_opt_t *opt, cmp_cb_t cmp_cb)
{
    rbt_opt_t _opt;
    rbt_tree_t *tree;

    if (NULL == opt) {
        opt = &_opt;
        opt->pool = (void *)NULL;
        opt->alloc = (mem_alloc_cb_t)mem_alloc;
        opt->dealloc = (mem_dealloc_cb_t)mem_dealloc;
    }

    tree = (rbt_tree_t *)opt->alloc(opt->pool, sizeof(rbt_tree_t));
    if (NULL == tree) {
        return NULL;
    }

    tree->sentinel = (rbt_node_t *)opt->alloc(opt->pool, sizeof(rbt_node_t));
    if (NULL == tree->sentinel) {
        tree->dealloc(tree->pool, tree);
        return NULL;
    }

    tree->sentinel->color = RBT_COLOR_BLACK;
    tree->root = tree->sentinel;
    tree->cmp_cb = cmp_cb;
    tree->num = 0;

    tree->pool = opt->pool;
    tree->alloc = opt->alloc;
    tree->dealloc = opt->dealloc;

    return tree;
}

/******************************************************************************
 **函数名称: rbt_creat_node
 **功    能: 创建关键字为key的结点(内部接口)
 **输入参数:
 **     tree: 红黑树
 **     key: 主键
 **     len: 主键长度
 **     color: 结点颜色
 **     type: 新增结点是父结点的左孩子还是右孩子
 **     parent: 父结点
 **输出参数: NONE
 **返    回: RBT_OK:成功 RBT_ERR:失败
 **实现描述:
 **注意事项: 新结点的左右孩子肯定为叶子结点
 **作    者: # Qifeng.zou # 2013.12.23 #
 ******************************************************************************/
static rbt_node_t *rbt_creat_node(rbt_tree_t *tree, int color, int type, rbt_node_t *parent)
{
    rbt_node_t *node;

    node = (rbt_node_t *)tree->alloc(tree->pool, sizeof(rbt_node_t));
    if (NULL == node) {
        return NULL;
    }

    node->color = color;
    node->lchild = tree->sentinel;
    node->rchild = tree->sentinel;
    if (NULL != parent) {
        rbt_set_child(tree, parent, type, node);
    }
    else {
        node->parent = tree->sentinel;
    }

    return node;
}

/******************************************************************************
 **函数名称: rbt_insert
 **功    能: 向红黑树中增加结点(对外接口)
 **输入参数:
 **     tree: 红黑树
 **     key: 关键字
 **     size: 关键字SIZE(如果关键字为字符串时, 注意其为SIZE, 不是长度)
 **     data: 关键字对应的数据块
 **输出参数: NONE
 **返    回: RBT_OK:成功 RBT_ERR:失败 RBT_NODE_EXIST:结点存在
 **实现描述:
 **     1. 当根结点为空时，直接添加
 **     2. 将结点插入树中, 检查并修复新结点造成红黑树性质的破坏
 **注意事项:
 **  红黑树的5点性质:
 **     1、每个结点要么是红色的，要么是黑色的；
 **     2、根结点是黑色的；
 **     3、所有叶子结点(NIL)都是黑色的；
 **     4、如果一个结点是红色，则它的两个儿子都是黑色的；
 **     5、对任何一个结点，从该结点通过其子孙结点到达叶子结点(NIL)
 **         的所有路径上包含相同数目的黑结点。
 **注意事项: 插入结点操作只可能破坏性质(4)
 **作    者: # Qifeng.zou # 2013.12.23 # 2015.07.21 21:29:07 #
 ******************************************************************************/
int rbt_insert(rbt_tree_t *tree, void *data)
{
    int ret;
    rbt_node_t *node = tree->root, *add;

    /* 1. 当根结点为空时，直接添加 */
    if (tree->sentinel == tree->root) {
        /* 性质2: 根结点是黑色的 */
        tree->root = rbt_creat_node(tree, RBT_COLOR_BLACK, 0, NULL);
        if (NULL == tree->root) {
            tree->root = tree->sentinel;
            return RBT_ERR;
        }

        ++tree->num;
        tree->root->data = data;
        return RBT_OK;
    }

    /* 2. 将结点插入树中, 检查并修复新结点造成红黑树性质的破坏 */
    while (tree->sentinel != node) {
            ret = tree->cmp_cb(data, node->data);
            if (0 == ret) {
                return RBT_NODE_EXIST;
            }
            else if (ret < 0) {
                if (tree->sentinel == node->lchild) {
                    add = rbt_creat_node(tree, RBT_COLOR_RED, RBT_LCHILD, node);
                    if (NULL == add) {
                        return RBT_ERR;
                    }

                    ++tree->num;
                    add->data = data;
                    return rbt_insert_fixup(tree, add); /* 防止红黑树的性质被破坏 */
                }
                node = node->lchild;
            }
            else {
                if (tree->sentinel == node->rchild) {
                    add = rbt_creat_node(tree, RBT_COLOR_RED, RBT_RCHILD, node);
                    if (NULL == add) {
                        return RBT_ERR;
                    }

                    ++tree->num;
                    add->data = data;
                    return rbt_insert_fixup(tree, add); /* 防止红黑树的性质被破坏 */
                }
                node = node->rchild;
            }
    }

    return RBT_OK;
}

/******************************************************************************
 **函数名称: rbt_insert_fixup
 **功    能: 插入操作修复(内部接口)
 **输入参数:
 **     tree: 红黑树
 **     node: 新增结点的地址
 **输出参数: NONE
 **返    回: RBT_OK:成功 RBT_ERR:失败
 **实现描述:
 **     1. 检查红黑树性质是否被破坏
 **     2. 如果被破坏，则进行对应的处理
 **注意事项: 插入结点操作只可能破坏性质④
 **作    者: # Qifeng.zou # 2013.12.23 #
 ******************************************************************************/
static int rbt_insert_fixup(rbt_tree_t *tree, rbt_node_t *node)
{
    rbt_node_t *parent = NULL, *uncle = NULL, *grandpa = NULL, *gparent = NULL;

    while (rbt_is_red(node)) {
        parent = node->parent;
        if (rbt_is_black(parent)) {
            return RBT_OK;
        }

        grandpa = parent->parent;
        if (parent == grandpa->lchild) { /* 父结点为左结点 */
            uncle = grandpa->rchild;
            /* case 1: 父结点和叔结点为红色 */
            if (rbt_is_red(uncle)) {
                rbt_set_black(parent);
                rbt_set_black(uncle);
                if (grandpa != tree->root) {
                    rbt_set_red(grandpa);
                }
                node = grandpa;
                continue;
            }
            /* case 2: 叔结点为黑色，结点为左孩子 */
            else if (node == parent->lchild) {
                /* 右旋转: 以grandpa为支点 */
                gparent = grandpa->parent;
                rbt_set_red(grandpa);
                rbt_set_black(parent);

                rbt_right_rotate(tree, grandpa);
                node = gparent;
                continue;
            }
            /* case 3: 叔结点为黑色，结点为右孩子 */
            else {
                /* 左旋转: 以parent为支点 */
                rbt_left_rotate(tree, parent);

                node = parent;
                continue;
            }
        }
        else {                      /* 父结点为右孩子 */
            uncle = grandpa->lchild;

            /* case 1: 父结点和叔结点为红色 */
            if (rbt_is_red(uncle)) {
                rbt_set_black(parent);
                rbt_set_black(uncle);
                if (grandpa != tree->root) {
                    rbt_set_red(grandpa);
                }

                node = grandpa;
                continue;
            }
            /* case 2: 叔结点为黑色，结点为左孩子 */
            else if (node == parent->lchild) {
                /* 右旋转: 以parent为支点 */
                rbt_right_rotate(tree, parent);
                node = parent;
                continue;
            }
            /* case 3: 叔结点为黑色，结点为右孩子 */
            else {
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

    return RBT_OK;
}

/******************************************************************************
 **函数名称: rbt_delete
 **功    能: 删除结点(外部接口)
 **输入参数:
 **     tree: 红黑树
 **     key: 关键字
 **     size: 关键字SIZE
 **输出参数:
 **     data: 关键字对应的数据块
 **返    回: RBT_OK:成功  RBT_ERR:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.12.27 #
 ******************************************************************************/
int rbt_delete(rbt_tree_t *tree, void *key, void **data)
{
    int ret;
    rbt_node_t *node = tree->root;

    while (tree->sentinel != node) {
        ret = tree->cmp_cb(key, node->data);
        if (0 == ret) {
            --tree->num;
            *data = node->data;
            return _rbt_delete(tree, node);
        }
        else if (ret < 0) {
            node = node->lchild;
        }
        else {
            node = node->rchild;
        }
    }

    *data = NULL;

    return RBT_OK;
}

/******************************************************************************
 **函数名称: _rbt_delete
 **功    能: 删除结点(内部接口)
 **输入参数:
 **     tree: 红黑树
 **     dnode: 将被删除的结点
 **输出参数: NONE
 **返    回: RBT_OK:成功  RBT_ERR:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.12.28 #
 ******************************************************************************/
static int _rbt_delete(rbt_tree_t *tree, rbt_node_t *dnode)
{
    //key_obj_t key;
    rbt_node_t *parent, *next, *refer;

    /* Case 1: 被删结点D的左孩子为叶子结点, 右孩子无限制(可为叶子结点，也可为非叶子结点) */
    if (tree->sentinel == dnode->lchild) {
        parent = dnode->parent;
        refer = dnode->rchild;

        refer->parent = parent;
        if (tree->sentinel == parent) {
            tree->root = refer;
        }
        else if (dnode == parent->lchild) {
            parent->lchild = refer;
        }
        else { /* dnode == parent->rchild */
            parent->rchild = refer;
        }

        if (rbt_is_red(dnode)) {
            //tree->dealloc(tree->pool, dnode->key.k);
            tree->dealloc(tree->pool, dnode);
            return RBT_OK;
        }

        //tree->dealloc(tree->pool, dnode->key.k);
        tree->dealloc(tree->pool, dnode);

        return rbt_delete_fixup(tree, refer);
    }
    /* Case 2: 被删结点D的右孩子为叶子结点, 左孩子不为叶子结点 */
    else if (tree->sentinel == dnode->rchild) {
        parent = dnode->parent;
        refer = dnode->lchild;

        refer->parent = parent;
        if (tree->sentinel == parent) {
            tree->root = refer;
        }
        else if (dnode == parent->lchild) {
            parent->lchild = refer;
        }
        else { /* dnode == parent->rchild */
            parent->rchild = refer;
        }

        if (rbt_is_red(dnode)) {
            //tree->dealloc(tree->pool, dnode->key.k);
            tree->dealloc(tree->pool, dnode);
            return RBT_OK;
        }

        //tree->dealloc(tree->pool, dnode->key.k);
        tree->dealloc(tree->pool, dnode);

        return rbt_delete_fixup(tree, refer);
    }

    /* Case 3: 被删结点D的左右孩子均不为叶子结点 */
    /* 3.1 查找dnode的后继结点next */
    next = dnode->rchild;
    while (tree->sentinel != next->lchild) {
        next = next->lchild;
    }

    parent = next->parent;
    refer = next->rchild;

    refer->parent = parent;
    if (next == parent->lchild) {
        parent->lchild = refer;
    }
    else { /* next == parent->rchild */
        parent->rchild = refer;
    }

    //key = dnode->key;
    //dnode->key = next->key;
    dnode->data = next->data; /* Copy next's satellite data into dnode */

    if (rbt_is_red(next)) {  /* Not black */
        //tree->dealloc(tree->pool, key.k);
        tree->dealloc(tree->pool, next);
        return RBT_OK;
    }

    //tree->dealloc(tree->pool, key.k);
    tree->dealloc(tree->pool, next);

    return rbt_delete_fixup(tree, refer);
}

/******************************************************************************
 **函数名称: rbt_delete_fixup
 **功    能: 修复删除操作造成的黑红树性质的破坏(内部接口)
 **输入参数:
 **     tree: 红黑树
 **     node: 实际被删结点的替代结点(注: node有可能是叶子结点)
 **输出参数: NONE
 **返    回: RBT_OK:成功  RBT_ERR:失败
 **实现描述:
 **注意事项: 被删结点为黑色结点，才能调用此函数进行性质调整
 **作    者: # Qifeng.zou # 2013.12.28 #
 ******************************************************************************/
static int rbt_delete_fixup(rbt_tree_t *tree, rbt_node_t *node)
{
    rbt_node_t *parent = NULL, *brother = NULL;

    while (rbt_is_black(node) && (tree->root != node)) {
        /* Set parent and brother */
        parent = node->parent;

        if (node == parent->lchild) {
            brother = parent->rchild;

            /* Case 1: 兄弟结点为红色:  以parent为支点, 左旋处理 */
            if (rbt_is_red(brother)) {
                rbt_set_red(parent);
                rbt_set_black(brother);
                rbt_left_rotate(tree, parent);

                /* 参照结点node不变, 兄弟结点改为parent->rchild */
                brother = parent->rchild;

                /* 注意: 此时处理还没有结束，还需要做后续的调整处理 */
            }

            /* Case 2: 兄弟结点为黑色(默认), 且兄弟结点的2个子结点都为黑色 */
            if (rbt_is_black(brother->lchild) && rbt_is_black(brother->rchild)) {
                rbt_set_red(brother);
                node = parent;
            }
            else {
                /* Case 3: 兄弟结点为黑色(默认),
                    兄弟结点的左子结点为红色, 右子结点为黑色:  以brother为支点, 右旋处理 */
                if (rbt_is_black(brother->rchild)) {
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
        else {
            brother = parent->lchild;

            /* Case 5: 兄弟结点为红色:  以parent为支点, 右旋处理 */
            if (rbt_is_red(brother)) {
                rbt_set_red(parent);
                rbt_set_black(brother);

                rbt_right_rotate(tree, parent);

                /* 参照结点node不变 */
                brother = parent->lchild;

                /* 注意: 此时处理还没有结束，还需要做后续的调整处理 */
            }

            /* Case 6: 兄弟结点为黑色(默认), 且兄弟结点的2个子结点都为黑色 */
            if (rbt_is_black(brother->lchild) && rbt_is_black(brother->rchild)) {
                rbt_set_red(brother);
                node = parent;
            }
            else {
                /* Case 7: 兄弟结点为黑色(默认),
                    兄弟结点的右子结点为红色, 左子结点为黑色:  以brother为支点, 左旋处理 */
                if (rbt_is_black(brother->lchild)) {
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
    return RBT_OK;
}

/******************************************************************************
 **函数名称: rbt_print_head
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
static void rbt_print_head(const rbt_tree_t *tree,
        const rbt_node_t *node, int depth, print_cb_t print)
{
    int idx;
    rbt_node_t *parent = node->parent;

    while (depth > 0 && (NULL != parent)) {
        if (1 == depth) {
            fprintf(stderr, "|");
            for (idx=0; idx<8; idx++) {
                if (0 == idx) {
                    if (parent->lchild == node) {
                        fprintf(stderr, "l");
                    }
                    else {
                        fprintf(stderr, "r");
                    }
                }
                else {
                    fprintf(stderr, "-");
                }
            }
        }
        else {
            fprintf(stderr, "|");
            for (idx=0; idx<8; idx++) {
                fprintf(stderr, " ");
            }
        }
        depth--;
    }

    if ((tree->sentinel == node->lchild)
        && (tree->sentinel == node->rchild))
    {
        //fprintf(stderr, "<%03ld:%c/>\n", node->idx, node->color);
        fprintf(stderr, "<");
        print(node->data);
        fprintf(stderr, ":%c/>\n", node->color);
    }
    else {
        //fprintf(stderr, "<%03ld:%c>\n", node->idx, node->color);
        fprintf(stderr, "<");
        print(node->data);
        fprintf(stderr, ":%c>\n", node->color);
    }
}

/******************************************************************************
 **函数名称: rbt_print_tail
 **功    能: 打印结点尾(内部接口)
 **输入参数:
 **     tree: 红黑树
 **     node: 被打印的结点
 **     depth: 结点深度
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.12.17 #
 ******************************************************************************/
static void rbt_print_tail(const rbt_tree_t *tree,
        const rbt_node_t *node, int depth, print_cb_t print)
{
    int idx;

    if ((tree->sentinel == node->lchild)
        && (tree->sentinel == node->rchild))
    {
        return;
    }

    while (depth > 0) {
        fprintf(stderr, "|");
        for (idx=0; idx<8; idx++) {
            fprintf(stderr, " ");
        }
        depth--;
    }

    //fprintf(stderr, "</%03ld>\n", node->idx);
    fprintf(stderr, "</");
    print(node->data);
    fprintf(stderr, ">\n");
}

/******************************************************************************
 **函数名称: rbt_print
 **功    能: 打印红黑树(外部接口)
 **输入参数:
 **     tree: 红黑树
 **     print: 打印结点主键
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **     回调print只打印主键内容
 **作    者: # Qifeng.zou # 2013.12.17 #
 ******************************************************************************/
int rbt_print(rbt_tree_t *tree, print_cb_t print)
{
    int depth = 0;
    Stack_t _stack, *stack = &_stack;
    rbt_node_t *node = tree->root, *parent = NULL;

    if (tree->sentinel == node) return 0;

    stack_init(stack, RBT_MAX_DEPTH);

    while (tree->sentinel != node) {
        /* 压左孩子入栈 */
        while (tree->sentinel != node->lchild) {
            rbt_assert(tree, node);
            depth = stack_depth(stack);
            stack_push(stack, node);
            rbt_print_head(tree, node, depth, print);   /* 打印头：入栈时打印头 出栈时打印尾 */
            node = node->lchild;
        }

        /* 打印最左端的子孙结点 */
        depth = stack_depth(stack);
        rbt_print_head(tree, node, depth, print);

        /* 最左端的孩子有右孩子 */
        if (tree->sentinel != node->rchild) {
            stack_push(stack, node);
            node = node->rchild;
            continue;
        }

        /* 最左端的孩子无右孩子 */
        rbt_print_tail(tree, node, depth, print);

        parent = stack_gettop(stack);
        if (NULL == parent) {
            return stack_destroy(stack);
        }

        /* 判断最左结点的父结点未处理完成 */
        if (parent->lchild == node) {
            if (tree->sentinel != parent->rchild) {
                node = parent->rchild;
                continue;
            }
        }

        /* 判断最左结点的父结点已处理完成 */
        while ((node == parent->rchild)
            || (tree->sentinel == parent->rchild))
        {
            stack_pop(stack);

            depth = stack_depth(stack);
            rbt_print_tail(tree, parent, depth, print);    /* 打印尾：出栈时打印尾 入栈时已打印头 */

            node = parent;
            parent = stack_gettop(stack);
            if (NULL == parent) {
                return stack_destroy(stack);
            }
        }

        node = parent->rchild;
    }

    return stack_destroy(stack);
}

/******************************************************************************
 **函数名称: rbt_query
 **功    能: 搜索指定关键字结点(外部接口)
 **输入参数:
 **     tree: 红黑树
 **     size: 关键字长度
 **输出参数: NONE
 **返    回: 数据地址
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.12.23 #
 ******************************************************************************/
void *rbt_query(rbt_tree_t *tree, void *key)
{
    int ret;
    rbt_node_t *node = tree->root;

    while (tree->sentinel != node) {
        ret = tree->cmp_cb(key, node->data);
        if (0 == ret) {
            return node->data;
        }
        else if (ret < 0) {
            node = node->lchild;
        }
        else {
            node = node->rchild;
        }
    }

    return NULL;
}

/******************************************************************************
 **函数名称: rbt_destroy
 **功    能: 销毁红黑树(外部接口)
 **输入参数:
 **     tree: 红黑树
 **     dealloc: 释放的回调函数
 **     args: 内存池对象
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **     1. 左右孩子均为叶子结点的结点直接释放
 **     2. 其他树内结点出栈时释放
 **     3. 叶子结点最后释放
 **注意事项:
 **作    者: # Qifeng.zou # 2013.12.27 #
 ******************************************************************************/
int rbt_destroy(rbt_tree_t *tree, mem_dealloc_cb_t dealloc, void *args)
{
    Stack_t _stack, *stack = &_stack;
    rbt_node_t *node = tree->root, *parent;

    if (tree->sentinel == node) {
        tree->dealloc(tree->pool, tree->sentinel);
        tree->dealloc(tree->pool, tree);
        return RBT_OK;
    }

    stack_init(stack, RBT_MAX_DEPTH);

    while (tree->sentinel != node) {
        /* 压左孩子入栈 */
        while (tree->sentinel != node->lchild) {
            stack_push(stack, node);

            node = node->lchild;
        }

        /* 最左端的孩子有右孩子 */
        if (tree->sentinel != node->rchild) {
            stack_push(stack, node);
            node = node->rchild;
            continue;
        }

        parent = stack_gettop(stack);
        if (NULL == parent) {
            dealloc(args, node->data);
            //tree->dealloc(tree->pool, node->key.k);
            tree->dealloc(tree->pool, node);
            tree->dealloc(tree->pool, tree->sentinel);
            tree->dealloc(tree->pool, tree);
            stack_destroy(stack);
            return RBT_OK;
        }

        if ((parent->lchild == node) /* 右孩子是否已处理 */
            && (tree->sentinel != parent->rchild))
        {
            dealloc(args, node->data);
            //tree->dealloc(tree->pool, node->key.k);
            tree->dealloc(tree->pool, node);
            node = parent->rchild;
            continue;
        }

        /* 其他树内结点出栈时释放 */
        while ((node == parent->rchild)
            || (tree->sentinel == parent->rchild))
        {
            stack_pop(stack);

            dealloc(args, node->data);
            //tree->dealloc(tree->pool, node->key.k);
            tree->dealloc(tree->pool, node);     /* 出栈结点下一次循环时释放 */

            node = parent;
            parent = stack_gettop(stack);
            if (NULL == parent) {
                dealloc(args, node->data);
                //tree->dealloc(tree->pool, node->key.k);
                tree->dealloc(tree->pool, node);
                tree->dealloc(tree->pool, tree->sentinel);
                tree->dealloc(tree->pool, tree);
                stack_destroy(stack);
                return RBT_OK;
            }
        }

        if (NULL != node) {  /* 释放上面出栈的结点 */
            dealloc(args, node->data);
            //tree->dealloc(tree->pool, node->key.k);
            tree->dealloc(tree->pool, node);
        }
        node = parent->rchild;
    }

    tree->dealloc(tree->pool, tree->sentinel);
    tree->dealloc(tree->pool, tree);
    stack_destroy(stack);
    return RBT_OK;
}

/******************************************************************************
 **函数名称: rbt_trav
 **功    能: 遍历红黑树(外部接口)
 **输入参数:
 **     tree: 红黑树
 **     cb: 回调函数
 **     args: 附加参数
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 处理思路可以参考rbt_print(), 但是稍微有些不同之处.
 **注意事项: 遍历处理过程中的回调函数proc()必须禁止改变当前树的结构, 否则程序必
 **          然出现crash的现象.
 **作    者: # Qifeng.zou # 2014.12.26 #
 ******************************************************************************/
int rbt_trav(rbt_tree_t *tree, trav_cb_t proc, void *args)
{
    Stack_t _stack, *stack = &_stack;
    rbt_node_t *node = tree->root, *parent;

    if (tree->sentinel == node) { return 0; }

    stack_init(stack, RBT_MAX_DEPTH);

    while (tree->sentinel != node) {
        /* 压左孩子入栈, 直到最左端 */
        while (tree->sentinel != node->lchild) {
            rbt_assert(tree, node);
            stack_push(stack, node);
            node = node->lchild;
        }

        proc(node->data, args); /* 处理最左端结点 */

        /* 最左端结点有右孩子 */
        if (tree->sentinel != node->rchild) {
            stack_push(stack, node); /* 最左端结点入栈 */
            node = node->rchild;
            continue;
        }

        /* 最左端结点无右孩子 */
        parent = stack_gettop(stack);
        if (NULL == parent) {
            return stack_destroy(stack);
        }

        if (parent->lchild == node) {
            proc(parent->data, args);   /* 处理最左结点的父结点 */
            if (tree->sentinel != parent->rchild) { /* 有右子树 */
                node = parent->rchild;  /* 处理右子树 */
                continue;
            }
            else {
                stack_pop(stack);
                node = parent;
                parent = stack_gettop(stack);
                if (NULL == parent) {
                    return stack_destroy(stack);
                }
            }
        }

        /* 判断最左结点的父结点已处理完成 */
        while ((node == parent->rchild)
            || (tree->sentinel == parent->rchild))
        {
            stack_pop(stack);

            node = parent;
            parent = stack_gettop(stack);
            if (NULL == parent) {
                return stack_destroy(stack);
            }
        }

        proc(parent->data, args); /* 处理有左子树而无右子树的结点 */

        node = parent->rchild;
    }

    return stack_destroy(stack);
}

/******************************************************************************
 **函数名称: rbt_find
 **功    能: 通过自定义函数筛选结点(外部接口)
 **输入参数:
 **     tree: 红黑树
 **     find: 回调函数
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 数据地址
 **实现描述: 处理思路可以参考rbt_print(), 但是稍微有些不同之处.
 **注意事项:
 **作    者: # Qifeng.zou # 2016.02.14 16:21:43 #
 ******************************************************************************/
void *rbt_find(rbt_tree_t *tree, find_cb_t find, void *args)
{
    Stack_t _stack, *stack = &_stack;
    rbt_node_t *node = tree->root, *parent;

    if (tree->sentinel == node) { return NULL; }

    stack_init(stack, RBT_MAX_DEPTH);

    while (tree->sentinel != node) {
        /* 压左孩子入栈, 直到最左端 */
        while (tree->sentinel != node->lchild) {
            rbt_assert(tree, node);
            stack_push(stack, node);
            node = node->lchild;
        }

        if (find(node->data, args)) { /* 处理最左端结点 */
            stack_destroy(stack);
            return node->data;
        }

        /* 最左端结点有右孩子 */
        if (tree->sentinel != node->rchild) {
            stack_push(stack, node); /* 最左端结点入栈 */
            node = node->rchild;
            continue;
        }

        /* 最左端结点无右孩子 */
        parent = stack_gettop(stack);
        if (NULL == parent) {
            stack_destroy(stack);
            return NULL;
        }

        if (parent->lchild == node) {
            if (find(parent->data, args)) { /* 处理最左结点的父结点 */
                stack_destroy(stack);
                return parent->data;
            }

            if (tree->sentinel != parent->rchild) { /* 有右子树 */
                node = parent->rchild;  /* 处理右子树 */
                continue;
            }
            else {
                stack_pop(stack);
                node = parent;
                parent = stack_gettop(stack);
                if (NULL == parent) {
                    stack_destroy(stack);
                    return NULL;
                }
            }
        }

        /* 判断最左结点的父结点已处理完成 */
        while ((node == parent->rchild)
            || (tree->sentinel == parent->rchild))
        {
            stack_pop(stack);

            node = parent;
            parent = stack_gettop(stack);
            if (NULL == parent) {
                stack_destroy(stack);
                return NULL;
            }
        }

        if (find(parent->data, args)) { /* 处理有左子树而无右子树的结点 */
            stack_destroy(stack);
            return parent->data;
        }

        node = parent->rchild;
    }

    stack_destroy(stack);
    return NULL;
}
