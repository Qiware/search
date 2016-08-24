/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: avl_tree.c
 ** 版本号: 1.0
 ** 描  述: 平衡二叉树.
 **         1. 平衡二叉树是一种性能优异的二叉查找树，其时间复杂度为O(lg^n).
 **         2. 与红黑树相比，其查询性能更加优异。如果构建的树，主要用于查询次数
 **            远高于增加、删除次数的话，应该优先考虑使用平衡二叉树；如果需要频
 **            繁的进行增加和删除操作，则应该考虑使用红黑树。
 **         3. 二叉树的插入、查找过程中，可以通过注册对应的回调，达到对不同存储
 **            数据类型的处理(设计模式: 模板模式/策略模式)
 ** 作  者: # Qifeng.zou # 2013.12.29 #
 ******************************************************************************/
#include "avl_tree.h"

/* 设置node的左孩子节点 */
#define avl_set_lchild(node, _lchild) \
{ \
    (node)->lchild = (_lchild); \
    if(NULL != (_lchild)) { \
        (_lchild)->parent = (node); \
    } \
}

/* 设置node的右孩子节点 */
#define avl_set_rchild(node, _rchild) \
{ \
    (node)->rchild = (_rchild); \
    if(NULL != (_rchild)) { \
        (_rchild)->parent = (node); \
    } \
}

/* 替换父节点的孩子节点 */
#define avl_replace_child(tree, _parent, old, _new) \
{ \
    if(NULL == _parent) { \
        (tree)->root = (_new); \
        if(NULL != (_new)) { \
            (_new)->parent = NULL; \
        } \
    } \
    else if(_parent->lchild == old) { \
        avl_set_lchild(_parent, _new); \
    } \
    else if(_parent->rchild == old) { \
        avl_set_rchild(_parent, _new); \
    } \
}

static int _avl_insert(avl_tree_t *tree, avl_node_t *node, bool *taller, void *data);
static void _avl_destroy(avl_tree_t *tree, avl_node_t *node, mem_dealloc_cb_t dealloc, void *args);

static int avl_right_balance(avl_tree_t *tree, avl_node_t *node);
static int avl_left_balance(avl_tree_t *tree, avl_node_t *node);
static int avl_insert_right(avl_tree_t *tree, avl_node_t *node, bool *taller, void *data);
static int avl_insert_left(avl_tree_t *tree, avl_node_t *node, bool *taller, void *data);

static int _avl_delete(avl_tree_t *tree, avl_node_t *node, void *key, bool *lower, void **data);

static int avl_replace_and_delete(avl_tree_t *tree, avl_node_t *node, avl_node_t *replace, bool *lower);
static int avl_delete_left_balance(avl_tree_t *tree, avl_node_t *node, bool *lower);
static int avl_delete_right_balance(avl_tree_t *tree, avl_node_t *node, bool *lower);

/******************************************************************************
 **函数名称: avl_creat
 **功    能: 创建平衡二叉树对象(对外接口)
 **输入参数:
 **     tree: 平衡二叉树
 **输出参数: NONE
 **返    回: AVL_OK:成功 AVL_ERR:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.12.19 #
 ******************************************************************************/
avl_tree_t *avl_creat(avl_opt_t *opt, cmp_cb_t cmp_cb)
{
    avl_opt_t _opt;
    avl_tree_t *tree;

    /* 合法性验证 */
    if (NULL == cmp_cb) {
        return NULL;
    }
    else if (NULL == opt) {
        opt = &_opt;
        opt->pool = NULL;
        opt->alloc = (mem_alloc_cb_t)mem_alloc;
        opt->dealloc = (mem_dealloc_cb_t)mem_dealloc;
    }

    /* 创建对象 */
    tree = (avl_tree_t *)opt->alloc(opt->pool, sizeof(avl_tree_t));
    if (NULL == tree) {
        return NULL;
    }

    tree->root = NULL;
    tree->cmp_cb = cmp_cb;
    tree->num = 0;

    tree->pool = opt->pool;
    tree->alloc = opt->alloc;
    tree->dealloc = opt->dealloc;

    return tree;
}

/******************************************************************************
 **函数名称: avl_insert
 **功    能: 插入新结点(对外接口)
 **输入参数:
 **     tree: 平衡二叉树
 **     data: 附加数据(注: 附件数据中必须包含主键)
 **输出参数: NONE
 **返    回:
 **     1. AVL_OK:成功
 **     2. AVL_ERR:失败
 **     3. AVL_NODE_EXIST:已存在
 **实现描述:
 **     1. 当树的根结点为空时，则直接创建根结点，并赋值
 **     2. 当树的根结点不为空时，则调用_insert()进行处理
 **注意事项:
 **     1. 当key已经存在时，并不会重置data值.
 **     2. 如果要重置data值，请先执行删除操作.
 **作    者: # Qifeng.zou # 2013.12.12 #
 ******************************************************************************/
int avl_insert(avl_tree_t *tree, void *data)
{
    int ret;
    bool taller = false;
    avl_node_t *root = tree->root;

    /* 如果为空树，则创建第一个结点 */
    if (NULL == root) {
        root = (avl_node_t *)tree->alloc(tree->pool, sizeof(avl_node_t));
        if (NULL == root) {
            return AVL_ERR;
        }
        root->parent = NULL;
        root->rchild = NULL;
        root->lchild = NULL;
        root->bf = AVL_EH;
        root->data = data;

        tree->num = 1;
        tree->root = root;
        return AVL_OK;
    }

    ret = _avl_insert(tree, root, &taller, data);
    if (AVL_OK == ret) {
        ++tree->num;
    }

    return ret;
}

/******************************************************************************
 **函数名称: _avl_insert
 **功    能: 插入新结点(内部接口)
 **输入参数:
 **     tree: 平衡二叉树
 **     node: 需在该结点的子树上插入IDX
 **     data: 附加数据(注: 附加数据中必须包含主键)
 **输出参数:
 **     taller: 是否增高
 **返    回: AVL_OK:成功 AVL_NODE_EXIST:已存在 AVL_ERR:失败
 **实现描述:
 **     1. 当结点关键字等于key值时，结点已存在
 **     2. 当结点关键字小于key值时，插入右子树
 **     3. 当结点关键字大于key值时，插入左子树
 **注意事项:
 **作    者: # Qifeng.zou # 2013.12.13 #
 ******************************************************************************/
static int _avl_insert(avl_tree_t *tree, avl_node_t *node, bool *taller, void *data)
{
    int ret;

    ret = tree->cmp_cb(data, node->data);
    if (0 == ret) {
        *taller = false;
        return AVL_NODE_EXIST;
    }
    else if (ret < 0) {
        return avl_insert_left(tree, node, taller, data);
    }

    return avl_insert_right(tree, node, taller, data);
}

/******************************************************************************
 **函数名称: avl_insert_right
 **功    能: 在node的右子树中插入新结点(内部接口)
 **输入参数:
 **     tree: 平衡二叉树
 **     node: 需在该结点的子树上插入key
 **     data: 数据信息
 **输出参数:
 **     taller: 是否增高
 **返    回: AVL_OK:成功 AVL_NODE_EXIST:已存在 AVL_ERR:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.12.13 #
 ******************************************************************************/
static int avl_insert_right(avl_tree_t *tree, avl_node_t *node, bool *taller, void *data)
{
    int ret;
    avl_node_t *add = NULL;

    if (NULL == node->rchild) {
        add = (avl_node_t *)tree->alloc(tree->pool, sizeof(avl_node_t));
        if (NULL == add) {
            *taller = false;
            return AVL_ERR;
        }

        add->lchild = NULL;
        add->rchild = NULL;
        add->parent = node;
        add->bf = AVL_EH;
        add->data = data;

        node->rchild = add;
        *taller = true;     /* node的高度增加了 */
    }
    else {
        ret = _avl_insert(tree, node->rchild, taller, data);
        if (AVL_OK != ret) {
            return ret;
        }
    }

    if (false == *taller) {
        return AVL_OK;
    }

    /* 右增高: 进行平衡化处理 */
    switch(node->bf) {
        case AVL_LH:    /* 左高: 右子树增高 不会导致失衡 */
            node->bf = AVL_EH;
            *taller = false;
            return AVL_OK;
        case AVL_EH:    /* 等高: 右子树增高 不会导致失衡 */
            node->bf = AVL_RH;
            *taller = true;
            return AVL_OK;
        case AVL_RH:    /* 右高: 右子树增高 导致失衡 */
            avl_right_balance(tree, node);
            *taller = false;
            return AVL_OK;
    }

    return AVL_ERR;
}

/******************************************************************************
 **函数名称: avl_insert_left
 **功    能: 在node的左子树中插入新结点(内部接口)
 **输入参数:
 **     tree: 平衡二叉树
 **     node: 需在该结点的子树上插入key
 **     data: 需要插入的数据
 **输出参数:
 **     taller: 是否增高
 **返    回: AVL_OK:成功 AVL_NODE_EXIST:已存在 AVL_ERR:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.12.13 #
 ******************************************************************************/
static int avl_insert_left(avl_tree_t *tree, avl_node_t *node, bool *taller, void *data)
{
    int ret;
    avl_node_t *add;

    if (NULL == node->lchild) {
        add = (avl_node_t *)tree->alloc(tree->pool, sizeof(avl_node_t));
        if (NULL == add) {
            *taller = false;
            return AVL_ERR;
        }

        add->lchild = NULL;
        add->rchild = NULL;
        add->parent = node;
        add->bf = AVL_EH;
        add->data = data;

        node->lchild = add;
        *taller = true;     /* node的高度增加了 */
    }
    else {
        ret = _avl_insert(tree, node->lchild, taller, data);
        if (AVL_OK != ret) {
            return ret;
        }
    }

    if (false == *taller) {
        return AVL_OK;
    }

    /* 左增高: 进行平衡化处理 */
    switch(node->bf) {
        case AVL_RH:    /* 右高: 左子树增高 不会导致失衡 */
            node->bf = AVL_EH;
            *taller = false;
            return AVL_OK;
        case AVL_EH:    /* 等高: 左子树增高 不会导致失衡 */
            node->bf = AVL_LH;
            *taller = true;
            return AVL_OK;
        case AVL_LH:    /* 左高: 左子树增高 导致失衡 */
            avl_left_balance(tree, node);
            *taller = false;
            return AVL_OK;
    }

    return AVL_ERR;
}

/******************************************************************************
 **函数名称: avl_rr_balance
 **功    能: RR型平衡化处理 - 向左旋转(内部接口)
 **输入参数:
 **     tree: 平衡二叉树
 **     node: 右边失去平衡的结点
 **输出参数: NONE
 **返    回: AVL_OK:成功 AVL_ERR:失败
 **实现描述: RR型
 **              A                  C
 **             / \                / \
 **            AL  C      ->      A  CR
 **               / \            / \   \
 **              CL CR          AL CL   X
 **                   \
 **                    X
 **              (1)                (2)
 ** 说明: 结点A是失衡结点，此时当结点C的平衡因子为-1时，可判断为RR型。
 **注意事项:
 **     1. 图(1)中A表示右边失衡的结点 图(2)表示平衡处理的结果
 **作    者: # Qifeng.zou # 2013.12.13 #
 ******************************************************************************/
static int avl_rr_balance(avl_tree_t *tree, avl_node_t *node)
{
    avl_node_t *rchild = node->rchild, *parent = node->parent;

    avl_set_rchild(node, rchild->lchild);
    node->bf = AVL_EH;

    avl_set_lchild(rchild, node);
    rchild->bf = AVL_EH;

    avl_replace_child(tree, parent, node, rchild);

    return AVL_OK;
}

/******************************************************************************
 **函数名称: avl_rl_balance
 **功    能: RL型平衡化处理 - 先向右旋转 再向左旋转(内部接口)
 **输入参数:
 **     tree: 平衡二叉树
 **     node: 右边失去平衡的结点
 **输出参数: NONE
 **返    回: AVL_OK:成功 AVL_ERR:失败
 **实现描述:
 ** 场景1: RL型
 **              A                    B
 **             / \                /    \
 **            AL  C      ->      A      C
 **               / \            / \    / \
 **              B  CR          AL BL  BR CR
 **             / \
 **            BL BR
 **              (1)                (2)
 ** 说明: 结点A是失衡结点，此时当结点C的平衡因子为1时，可判断为RL型。
 **       虽然此时结点B的平衡因子的值可能为:-1, 0, 1.
 **       但旋转处理的方式是一致的，只是旋转之后的平衡因子不一致.
 **注意事项:
 **     1. 图(1)中A表示右边失衡的结点 图(2)表示平衡处理的结果
 **作    者: # Qifeng.zou # 2013.12.13 #
 ******************************************************************************/
int avl_rl_balance(avl_tree_t *tree, avl_node_t *node)
{
    avl_node_t *rchild = node->rchild,
        *parent = node->parent, *rlchild = NULL;

    rlchild = rchild->lchild;
    switch(rlchild->bf) {
        case AVL_LH:
        {
            node->bf = AVL_EH;
            rchild->bf = AVL_RH;
            rlchild->bf = AVL_EH;
            break;
        }
        case AVL_EH:
        {
            node->bf = AVL_EH;
            rchild->bf = AVL_EH;
            rlchild->bf = AVL_EH;
            break;
        }
        case AVL_RH:
        {
            node->bf = AVL_LH;
            rchild->bf = AVL_EH;
            rlchild->bf = AVL_EH;
            break;
        }
    }

    avl_set_lchild(rchild, rlchild->rchild);
    avl_set_rchild(rlchild, rchild);
    avl_set_rchild(node, rlchild->lchild);
    avl_set_lchild(rlchild, node);

    avl_replace_child(tree, parent, node, rlchild);

    return AVL_OK;
}

/******************************************************************************
 **函数名称: avl_right_balance
 **功    能: 对右边失去平衡的结点进行平衡化处理(内部接口)
 **输入参数:
 **     tree: 平衡二叉树
 **     node: 右边失去平衡的结点
 **输出参数: NONE
 **返    回: AVL_OK:成功 AVL_ERR:失败
 **实现描述:
 ** 场景1: RR型
 **              A                  C
 **             / \                / \
 **            AL  C      ->      A  CR
 **               / \            / \   \
 **              CL CR          AL CL   X
 **                   \
 **                    X
 **              (1)                (2)
 ** 说明: 结点A是失衡结点，此时当结点C的平衡因子为-1时，可判断为RR型。
 ** 场景2: RL型
 **              A                    B
 **             / \                /    \
 **            AL  C      ->      A      C
 **               / \            / \    / \
 **              B  CR          AL BL  BR CR
 **             / \
 **            BL BR
 **              (1)                (2)
 ** 说明: 结点A是失衡结点，此时当结点C的平衡因子为1时，可判断为RL型。
 **       虽然此时结点B的平衡因子的值可能为:-1, 0, 1.
 **       但旋转处理的方式是一致的，只是旋转之后的平衡因子不一致.
 **注意事项:
 **     1. 图(1)中A表示右边失衡的结点 图(2)表示平衡处理的结果
 **作    者: # Qifeng.zou # 2013.12.13 #
 ******************************************************************************/
static int avl_right_balance(avl_tree_t *tree, avl_node_t *node)
{
    avl_node_t *rchild = node->rchild;

    switch(rchild->bf) {
        case AVL_RH:    /* 场景1: RR型 - 向左旋转 */
        {
            return avl_rr_balance(tree, node);
        }
        case AVL_LH:    /* 场景2: RL型 - 先向右旋转 再向左旋转 */
        {
            return avl_rl_balance(tree, node);
        }
    }

    return AVL_ERR;
}

/******************************************************************************
 **函数名称: avl_ll_balance
 **功    能: LL型平衡化处理 - 向右旋转(内部接口)
 **输入参数:
 **     tree: 平衡二叉树
 **     node: 左边失去平衡的结点
 **输出参数: NONE
 **返    回: AVL_OK:成功 AVL_ERR:失败
 **实现描述:
 ** 场景1: LL型
 **              A                  B
 **             / \                / \
 **            B   C      ->      BL  A
 **           / \                /   / \
 **          BL BR              X   BR  C
 **         /
 **        X
 **             (1)                (2)
 ** 说明: 结点A是失衡结点，此时当结点B的平衡因子为1时，可判断为LL型。
 **注意事项:
 **     1. 图(1)中A表示左边失衡的结点 图(2)表示平衡处理的结果
 **作    者: # Qifeng.zou # 2013.12.13 #
 ******************************************************************************/
int avl_ll_balance(avl_tree_t *tree, avl_node_t *node)
{
    avl_node_t *lchild = node->lchild, *parent = node->parent;

    avl_set_lchild(node, lchild->rchild);
    node->bf = AVL_EH;

    avl_set_rchild(lchild, node);
    lchild->bf = AVL_EH;

    avl_replace_child(tree, parent, node, lchild);

    return AVL_OK;
}

/******************************************************************************
 **函数名称: avl_lr_balance
 **功    能: LR型平衡化处理 - 先左旋转 再向右旋转(内部接口)
 **输入参数:
 **     tree: 平衡二叉树
 **     node: 左边失去平衡的结点
 **输出参数: NONE
 **返    回: AVL_OK:成功 AVL_ERR:失败
 **实现描述:
 ** 场景1: LL型
 **              A                  B
 **             / \                / \
 **            B   C      ->      BL  A
 **           / \                /   / \
 **          BL BR              X   BR  C
 **         /
 **        X
 **             (1)                (2)
 ** 说明: 结点A是失衡结点，此时当结点B的平衡因子为1时，可判断为LL型。
 ** 场景2: LR型
 **              A                    C
 **             / \                /     \
 **            B  AR      ->      B       A
 **           / \                / \     / \
 **          BL  C              BL CL   CR AR
 **             / \
 **            CL CR
 **             (1)                (2)
 ** 说明: 结点A是失衡结点，此时当结点B的平衡因子为-1时，可判断为LR型。
 **       虽然此时结点C的平衡因子的值可能为:-1, 0, 1.
 **       但旋转处理的方式是一致的，只是旋转之后的平衡因子不一致.
 **注意事项:
 **     1. 图(1)中A表示左边失衡的结点 图(2)表示平衡处理的结果
 **作    者: # Qifeng.zou # 2013.12.13 #
 ******************************************************************************/
int avl_lr_balance(avl_tree_t *tree, avl_node_t *node)
{
    avl_node_t *lchild = node->lchild,
               *parent = node->parent, *lrchild = NULL;


    lrchild = lchild->rchild;
    switch(lrchild->bf) {
        case AVL_LH:
        {
            node->bf = AVL_RH;
            lchild->bf = AVL_EH;
            lrchild->bf = AVL_EH;
            break;
        }
        case AVL_EH:
        {
            node->bf = AVL_EH;
            lchild->bf = AVL_EH;
            lrchild->bf = AVL_EH;
            break;
        }
        case AVL_RH:
        {
            node->bf = AVL_EH;
            lchild->bf = AVL_LH;
            lrchild->bf = AVL_EH;
            break;
        }
    }

    avl_set_rchild(lchild, lrchild->lchild);
    avl_set_lchild(lrchild, lchild);
    avl_set_lchild(node, lrchild->rchild);
    avl_set_rchild(lrchild, node);

    avl_replace_child(tree, parent, node, lrchild);

    return AVL_OK;
}

/******************************************************************************
 **函数名称: avl_left_balance
 **功    能: 对左边失去平衡的结点进行平衡化处理(内部接口)
 **输入参数:
 **     tree: 平衡二叉树
 **     node: 左边失去平衡的结点
 **输出参数: NONE
 **返    回: AVL_OK:成功 AVL_ERR:失败
 **实现描述:
 ** 场景1: LL型
 **              A                  B
 **             / \                / \
 **            B   C      ->      BL  A
 **           / \                /   / \
 **          BL BR              X   BR  C
 **         /
 **        X
 **             (1)                (2)
 ** 说明: 结点A是失衡结点，此时当结点B的平衡因子为1时，可判断为LL型。
 ** 场景2: LR型
 **              A                    C
 **             / \                /     \
 **            B  AR      ->      B       A
 **           / \                / \     / \
 **          BL  C              BL CL   CR AR
 **             / \
 **            CL CR
 **             (1)                (2)
 ** 说明: 结点A是失衡结点，此时当结点B的平衡因子为-1时，可判断为LR型。
 **       虽然此时结点C的平衡因子的值可能为:-1, 0, 1.
 **       但旋转处理的方式是一致的，只是旋转之后的平衡因子不一致.
 **注意事项:
 **     1. 图(1)中A表示左边失衡的结点 图(2)表示平衡处理的结果
 **作    者: # Qifeng.zou # 2013.12.13 #
 ******************************************************************************/
int avl_left_balance(avl_tree_t *tree, avl_node_t *node)
{
    avl_node_t *lchild = node->lchild;

    switch(lchild->bf) {
        case AVL_LH:    /* 场景1: LL型 */
        {
            return avl_ll_balance(tree, node);
        }
        case AVL_RH:    /* 场景2: LR型 */
        {
            return avl_lr_balance(tree, node);
        }
    }

    return AVL_ERR;
}

/******************************************************************************
 **函数名称: avl_query
 **功    能: 查找指定的结点
 **输入参数:
 **     tree: 平衡二叉树
 **     key: 主键(注: 与各结点挂载数据类型一致)
 **     size: 主键SIZE
 **输出参数: NONE
 **返    回: 数据地址
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.12.12 #
 ******************************************************************************/
void *avl_query(avl_tree_t *tree, void *key)
{
    int ret;
    avl_node_t *node = tree->root;

    while (NULL != node) {
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

    return NULL; /* Didn't find */
}

/******************************************************************************
 **函数名称: avl_destroy
 **功    能: 销毁AVL树(对外接口)
 **输入参数:
 **     tree: 平衡二叉树
 **     dealloc_cb: 节点数据空间释放函数
 **     args: 附加参数(用于释放节点数据空间)
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.12.15 #
 ******************************************************************************/
void avl_destroy(avl_tree_t *tree, mem_dealloc_cb_t dealloc_cb, void *args)
{
    if (NULL != tree->root) {
        _avl_destroy(tree, tree->root, dealloc_cb, args);
        tree->root = NULL;
    }

    tree->dealloc(tree->pool, tree);
}

/******************************************************************************
 **函数名称: avl_assert
 **功    能: 检测结点是否正常
 **输入参数:
 **     node: 被检测的结点
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项: # 非常有效 #
 **作    者: # Qifeng.zou # 2013.12.20 #
 ******************************************************************************/
void avl_assert(const avl_node_t *node)
{
    if ((NULL == node)
        || (NULL == node->parent))
    {
        return;
    }

    if ((node->parent->lchild != node)
        && (node->parent->rchild != node))
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
 **函数名称: avl_delete
 **功    能: 删除key值结点(对外接口)
 **输入参数:
 **     tree: 平衡二叉树
 **     key: 被删除的关键字(注: 与被删结点类型一致)
 **     len: 关键字长度
 **输出参数:
 **     data: 附加数据
 **返    回: AVL_OK:成功 AVL_ERR:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.12.19 #
 ******************************************************************************/
int avl_delete(avl_tree_t *tree, void *key, void **data)
{
    bool lower = false;

    if (NULL == tree->root) {
        *data = NULL;
        return AVL_OK;
    }

    return _avl_delete(tree, tree->root, key, &lower, data);
}

/******************************************************************************
 **函数名称: _avl_delete
 **功    能: 搜索并删除指定的key值结点(内部接口)
 **输入参数:
 **     tree: 平衡二叉树
 **     node: 以node为根结点的子树
 **     key: 主键(Primary idx)
 **输出参数:
 **     lower: 高度是否降低
 **     data: 附加数据
 **返    回: AVL_OK:成功 AVL_ERR:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.12.19 #
 ******************************************************************************/
static int _avl_delete(avl_tree_t *tree,
        avl_node_t *node, void *key, bool *lower, void **data)
{
    int ret;
    avl_node_t *parent = node->parent;

    /* 1. 查找需要被删除的结点 */
    ret = tree->cmp_cb(key, node->data);
    if (ret < 0) { /* 左子树上查找 */
        if (NULL == node->lchild) {
            *data = NULL;
            return AVL_OK;
        }

        _avl_delete(tree, node->lchild, key, lower, data);
        avl_assert(node);
        avl_assert(node->lchild);
        if (true == *lower) {
            return avl_delete_left_balance(tree, node, lower);
        }
        return AVL_OK;
    }
    else if (ret > 0) { /* 右子树上查找 */
        if (NULL == node->rchild) {
            *data = NULL;
            return AVL_OK;
        }

        _avl_delete(tree, node->rchild, key, lower, data);
        avl_assert(node);
        avl_assert(node->rchild);
        if (true == *lower) {
            return avl_delete_right_balance(tree, node, lower);
        }
        return AVL_OK;
    }

    /* 查找成功 */
    /* 2. 已找到将被删除的结点node */
    --tree->num;
    *data = node->data;

    /* 2.1 右子树为空, 只需接它的左子树(叶子结点也走这) */
    if (NULL == node->rchild) {
        *lower = true;

        avl_replace_child(tree, parent, node, node->lchild);

        avl_assert(parent);
        avl_assert(node->lchild);
        //tree->dealloc(tree->pool, node->key.k);
        tree->dealloc(tree->pool, node), node = NULL;
        return AVL_OK;
    }
    /* 2.2 左子树空, 只需接它的右子树 */
    else if (NULL == node->lchild) {
        *lower = true;

        avl_replace_child(tree, parent, node, node->rchild)

        avl_assert(parent);
        avl_assert(node->rchild);
        //tree->dealloc(tree->pool, node->key.k);
        tree->dealloc(tree->pool, node), node = NULL;
        return AVL_OK;
    }

    /* 2.3 左右子树均不为空: 查找左子树最右边的结点 替换被删的结点 */
    avl_replace_and_delete(tree, node, node->lchild, lower);
    if (true == *lower) {
        avl_assert(node);
        return avl_delete_left_balance(tree, node, lower);
    }

    return AVL_OK;
}

/******************************************************************************
 **函数名称: avl_replace_and_delete
 **功    能: 找到替换结点, 并替换被删除的结点(内部接口)
 **输入参数:
 **     tree: 平衡二叉树
 **     node: 将被删除的结点
 **     prev: 前继结点:此结点最右端的结点将会用来替换被删除的结点.
 **输出参数:
 **     lower: 高度是否变化
 **返    回: AVL_OK:成功 AVL_ERR:失败
 **实现描述:
 **注意事项:
 **     >> 在此其实并不会删除node, 而是将replace的值给了node, 再删了replace.
 **         因为在此使用的递归算法, 如果真把node给释放了，会造成压栈的信息出现错误!
 **作    者: # Qifeng.zou # 2013.12.19 #
 ******************************************************************************/
int avl_replace_and_delete(avl_tree_t *tree,
        avl_node_t *node, avl_node_t *prev, bool *lower)
{
    //key_obj_t key;

    if (NULL == prev->rchild) {
        *lower = true;

        /* > 备份主键数据 */
        //key.k = node->key.k;
        //key.l = node->key.l;

        /* > 转移结点数据 */
        //node->key.k = prev->key.k;    /* 注: 将rnode的值给了dnode */
        //node->key.l = prev->key.l;
        node->data = prev->data;
        if (prev == node->lchild) {
            avl_set_lchild(node, prev->lchild);
            /* prev->parent == dnode结点可能失衡，此处理交给前栈的函数处理 */
        }
        else {
            avl_set_rchild(prev->parent, prev->lchild);
            /* rnode的父结点可能失衡，此处理交给前栈的函数处理 */
        }
        avl_assert(node);
        avl_assert(prev->parent);
        avl_assert(prev->lchild);
        //tree->dealloc(tree->pool, key.k);
        tree->dealloc(tree->pool, prev); /* 注意: 释放的不是dnode, 而是rnode */
        return AVL_OK;
    }

    avl_replace_and_delete(tree, node, prev->rchild, lower);
    if (true == *lower) {
        avl_assert(node);
        avl_assert(prev);
        /* dnode的父结点可能失衡，此处理交给前栈的函数处理
            但replace可能失衡，因此必须在此自己处理 */
        avl_delete_right_balance(tree, prev, lower);
    }
    return AVL_OK;
}

/******************************************************************************
 **函数名称: avl_delete_left_balance
 **功    能: 结点node的左子树某结点被删除, 左高度降低后, 平衡化处理(内部接口)
 **输入参数:
 **     tree: 平衡二叉树
 **     node: 结点node的左子树的某个结点已被删除
 **输出参数:
 **     lower: 高度是否变化
 **返    回: AVL_OK:成功 AVL_ERR:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.12.19 #
 ******************************************************************************/
int avl_delete_left_balance(avl_tree_t *tree, avl_node_t *node, bool *lower)
{
    avl_node_t *rchild = NULL, *rlchild = NULL, *parent = node->parent;

    switch(node->bf) {
        case AVL_LH:    /* 左高: 左子树高度减1 树变矮 */
        {
            node->bf = AVL_EH;
            *lower = true;
            break;
        }
        case AVL_EH:    /* 等高: 左子树高度减1 树高度不变 */
        {
            node->bf = AVL_RH;
            *lower = false;
            break;
        }
        case AVL_RH:    /* 右高: 左子树高度减1 树失去平衡 */
        {
            rchild = node->rchild;
            switch(rchild->bf) {
                case AVL_EH:    /* RR型: 向左旋转 */
                case AVL_RH:    /* RR型: 向左旋转 */
                {
                    if (AVL_EH == rchild->bf) {
                        *lower = false;
                        rchild->bf = AVL_LH;
                        node->bf = AVL_RH;
                    }
                    else if (AVL_RH == rchild->bf) {
                        *lower = true;
                        rchild->bf = AVL_EH;
                        node->bf = AVL_EH;
                    }

                    avl_set_rchild(node, rchild->lchild);
                    avl_set_lchild(rchild, node);
                    avl_replace_child(tree, parent, node, rchild);

                    avl_assert(node);
                    avl_assert(rchild);
                    break;
                }
                case AVL_LH:    /* RL型: 先向右旋转 再向左旋转 */
                {
                    *lower = true;
                    rlchild = rchild->lchild;
                    switch(rlchild->bf) {
                        case AVL_LH:
                        {
                            node->bf = AVL_EH;
                            rchild->bf = AVL_RH;
                            rlchild->bf = AVL_EH;
                            break;
                        }
                        case AVL_EH:
                        {
                            node->bf = AVL_EH;
                            rchild->bf = AVL_EH;
                            rlchild->bf = AVL_EH;
                            break;
                        }
                        case AVL_RH:
                        {
                            node->bf = AVL_LH;
                            rchild->bf = AVL_EH;
                            rlchild->bf = AVL_EH;
                            break;
                        }
                    }

                    avl_set_rchild(node, rlchild->lchild);
                    avl_set_lchild(rchild, rlchild->rchild);
                    avl_set_lchild(rlchild, node);
                    avl_set_rchild(rlchild, rchild);

                    avl_replace_child(tree, parent, node, rlchild);

                    avl_assert(parent);
                    avl_assert(node);
                    avl_assert(rchild);
                    avl_assert(rlchild);
                    break;
                }
            }
            break;
        }
    }

    return AVL_OK;
}

/******************************************************************************
 **函数名称: avl_delete_right_balance
 **功    能: 结点node的右子树某结点被删除, 左高度降低后, 平衡化处理(内部接口)
 **输入参数:
 **     tree: 平衡二叉树
 **     node: 结点node的右子树的某个结点已被删除
 **输出参数:
 **     lower: 高度是否变化
 **返    回: AVL_OK:成功 AVL_ERR:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.12.19 #
 ******************************************************************************/
int avl_delete_right_balance(avl_tree_t *tree, avl_node_t *node, bool *lower)
{
    avl_node_t *lchild = NULL, *lrchild = NULL, *parent = node->parent;

    switch(node->bf) {
        case AVL_LH:    /* 左高: 右子树高度减1 树失去平衡 */
        {
            lchild = node->lchild;
            switch(lchild->bf)
            {
                case AVL_EH:    /* LL型: 向右旋转 */
                case AVL_LH:    /* LL型: 向右旋转 */
                {
                    if (AVL_EH == lchild->bf) {
                        *lower = false;
                        lchild->bf = AVL_RH;
                        node->bf = AVL_LH;
                    }
                    else {
                        *lower = true;
                        lchild->bf = AVL_EH;
                        node->bf = AVL_EH;
                    }

                    avl_set_lchild(node, lchild->rchild);
                    avl_set_rchild(lchild, node);
                    avl_replace_child(tree, parent, node, lchild);

                    avl_assert(parent);
                    avl_assert(node);
                    avl_assert(lchild);
                    break;
                }
                case AVL_RH:    /* LR型: 先向左旋转 再向右旋转 */
                {
                    *lower = true;
                    lrchild = lchild->rchild;
                    switch(lrchild->bf) {
                        case AVL_LH:
                        {
                            node->bf = AVL_RH;
                            lchild->bf = AVL_EH;
                            lrchild->bf = AVL_EH;
                            break;
                        }
                        case AVL_EH:
                        {
                            node->bf = AVL_EH;
                            lchild->bf = AVL_EH;
                            lrchild->bf = AVL_EH;
                            break;
                        }
                        case AVL_RH:
                        {
                            node->bf = AVL_EH;
                            lchild->bf = AVL_LH;
                            lrchild->bf = AVL_EH;
                            break;
                        }
                    }

                    avl_set_lchild(node, lrchild->rchild);
                    avl_set_rchild(lchild, lrchild->lchild);
                    avl_set_rchild(lrchild, node);
                    avl_set_lchild(lrchild, lchild);

                    avl_replace_child(tree, parent, node, lrchild);

                    avl_assert(parent);
                    avl_assert(node);
                    avl_assert(lchild);
                    avl_assert(lrchild);
                    break;
                }
            }
            break;
        }
        case AVL_EH:    /* 等高: 右子树高度减1 树高度不变 */
        {
            node->bf = AVL_LH;
            *lower = false;
            break;
        }
        case AVL_RH:    /* 右高: 右子树高度减1 树变矮 */
        {
            node->bf = AVL_EH;
            *lower = true;
            break;
        }
    }
    return AVL_OK;
}

/******************************************************************************
 **函数名称: _avl_destroy
 **功    能: 销毁AVL树(内部接口)
 **输入参数:
 **     tree: AVL树对象
 **     node: 需要被销毁的结点
 **     dealloc_cb: 节点数据空间释放函数
 **     args: 附加参数(用于释放节点数据空间)
 **输出参数:
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.12.15 #
 ******************************************************************************/
static void _avl_destroy(avl_tree_t *tree, avl_node_t *node, mem_dealloc_cb_t dealloc, void *args)
{
    if (NULL != node->lchild) {
        _avl_destroy(tree, node->lchild, dealloc, args);
    }

    if (NULL != node->rchild) {
        _avl_destroy(tree, node->rchild, dealloc, args);
    }

    dealloc(args, node->data);
    tree->dealloc(tree->pool, node);
}

static int _avl_print(avl_node_t *node, Stack_t *stack);
/******************************************************************************
 **函数名称: avl_print
 **功    能: 打印平衡二叉树(外部接口)
 **输入参数:
 **     tree: 平衡二叉树
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.12.17 #
 ******************************************************************************/
int avl_print(avl_tree_t *tree)
{
    Stack_t stack;

    memset(&stack, 0, sizeof(stack));

    if (NULL == tree->root) {
        return AVL_OK;
    }

    if (stack_init(&stack, AVL_MAX_DEPTH)) {
        return AVL_ERR_STACK;
    }

    _avl_print(tree->root, &stack);

    stack_destroy(&stack);
    return AVL_OK;
}

/******************************************************************************
 **函数名称: avl_print_head
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
static void avl_print_head(avl_node_t *node, int depth)
{
    int idx;
    avl_node_t *parent = node->parent;

    while (depth > 1) {
        depth--;
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
    }

    if ((NULL == node->lchild)
        && (NULL == node->rchild))
    {
        //fprintf(stderr, "<%03ld:%d/>\n", node->idx, node->bf);
    }
    else {
        //fprintf(stderr, "<%03ld:%d>\n", node->idx, node->bf);
    }
}

/******************************************************************************
 **函数名称: avl_print_tail
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
static void avl_print_tail(avl_node_t *node, int depth)
{
    int idx;

    if ((NULL == node->lchild)
        && (NULL == node->rchild))
    {
        return;
    }

    while (depth > 1) {
        fprintf(stderr, "|");
		for (idx=0; idx<8; idx++) {
			fprintf(stderr, " ");
		}
        depth--;
    }

    //fprintf(stderr, "</%03ld>\n", node->idx);
}

/******************************************************************************
 **函数名称: _avl_print
 **功    能: 打印平衡二叉树(内部接口)
 **输入参数:
 **     tree: 平衡二叉树
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **     1. 结点入栈
 **     2. 打印该结点(头)
 **     3. 打印该结点的右子树
 **     4. 打印该结点的左子树
 **     5. 打印该结点(尾)
 **     6. 结点出栈
 **注意事项:
 **作    者: # Qifeng.zou # 2013.12.17 #
 ******************************************************************************/
int _avl_print(avl_node_t *root, Stack_t *stack)
{
    int depth;
    avl_node_t *node = root;

    /* 1. 将要处理的结点压栈 */
    stack_push(stack, node);

    depth = stack_depth(stack);

    /* 2. 打印结点头 */
    avl_print_head(node, depth);

    /* 3. 打印右子树 */
    if (NULL != node->rchild) {
        _avl_print(node->rchild, stack);
    }

    /* 4. 打印左子树 */
    if (NULL != node->lchild) {
        _avl_print(node->lchild, stack);
    }

    depth = stack_depth(stack);

    /* 5. 打印结点尾 */
    avl_print_tail(node, depth);

    /* 6. 出栈 */
    stack_pop(stack);
    return AVL_OK;
}

/******************************************************************************
 **函数名称: _avl_trav
 **功    能: 遍历平衡二叉树(内部接口)
 **输入参数:
 **     node: 结点
 **     proc: 各结点的处理回调
 **     args: 附加参数
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 使用递归算法实现遍历(简单)
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.24 #
 ******************************************************************************/
static int _avl_trav(avl_node_t *node, trav_cb_t proc, void *args)
{
    /* 1. 遍历左子树 */
    if (NULL != node->lchild) {
        _avl_trav(node->lchild, proc, args);
    }

    /* 2. 处理当前结点 */
    proc(node->data, args);

    /* 3. 遍历右子树 */
    if (NULL != node->rchild) {
        _avl_trav(node->rchild, proc, args);
    }

    return AVL_OK;
}

/******************************************************************************
 **函数名称: avl_trav
 **功    能: 遍历平衡二叉树(外部接口)
 **输入参数:
 **     tree: 平衡二叉树
 **     proc: 各结点的处理回调
 **     args: 附加参数
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 按照从小到大的顺序进行遍历
 **注意事项: 遍历处理过程中的回调函数proc()必须禁止改变当前树的结构, 否则程序必
 **          然出现crash的现象.
 **作    者: # Qifeng.zou # 2014.12.23 #
 ******************************************************************************/
int avl_trav(avl_tree_t *tree, trav_cb_t proc, void *args)
{
    if (NULL == tree->root) {
        return AVL_OK;
    }

    return _avl_trav(tree->root, proc, args);
}

/******************************************************************************
 **函数名称: _avl_find
 **功    能: 查找平衡二叉树(内部接口)
 **输入参数:
 **     node: 结点
 **     find: 查找回调
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 数据地址
 **实现描述: 使用递归算法实现遍历(简单)
 **注意事项:
 **作    者: # Qifeng.zou # 2016.02.21 21:43:58 #
 ******************************************************************************/
static void *_avl_find(avl_node_t *node, find_cb_t find, void *args)
{
    void *data;

    /* 1. 遍历左子树 */
    if (NULL != node->lchild) {
        data = _avl_find(node->lchild, find, args);
        if (NULL != data) {
            return data;
        }
    }

    /* 2. 处理当前结点 */
    if (find(node->data, args)) {
        return node->data;
    }

    /* 3. 遍历右子树 */
    if (NULL != node->rchild) {
        data = _avl_find(node->rchild, find, args);
        if (NULL != data) {
            return data;
        }
    }

    return NULL;
}

/******************************************************************************
 **函数名称: avl_find
 **功    能: 通过自定义函数筛选结点(外部接口)
 **输入参数:
 **     tree: 平衡二叉树
 **     find: 回调函数
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 数据地址
 **实现描述: 处理思路可以参考avl_print(), 但是稍微有些不同之处.
 **注意事项:
 **作    者: # Qifeng.zou # 2016.02.21 21:38:29 #
 ******************************************************************************/
void *avl_find(avl_tree_t *tree, find_cb_t find, void *args)
{
    if (NULL == tree->root) {
        return NULL;
    }

    return _avl_find(tree->root, find, args);
}
