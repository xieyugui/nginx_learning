/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_rbtree.h
* @date:      2017/12/25 下午2:53
* @desc:
 *
 * 分析 https://www.cnblogs.com/doop-ymc/p/3440316.html
 *      http://www.cnblogs.com/skywang12345/p/3624177.html
 *      http://www.lxway.com/816555252.htm
*/

//
// Created by daemon.xie on 2017/12/25.
//

#ifndef NGX_RBTREE_NGX_RBTREE_H
#define NGX_RBTREE_NGX_RBTREE_H

#include <ngx_config.h>
#include <ngx_core.h>

typedef ngx_uint_t ngx_rbtree_key_t;
typedef ngx_int_t ngx_rbtree_key_int_t;

typedef struct ngx_rbtree_node_s ngx_rbtree_node_t;

struct ngx_rbtree_node_s {
    ngx_rbtree_key_t key; //无符号的键值
    ngx_rbtree_node_t *left;
    ngx_rbtree_node_t *right;
    ngx_rbtree_node_t *parent;
    u_char color; //节点颜色，0表示黑色，1表示红色
    u_char data; //数据
};

typedef struct ngx_rbtree_s ngx_rbtree_t;

//为解决不同节点含有相同关节自的元素冲突问题，红黑树设置了ngx_rbtree_insert_pt指针，这样可灵活的添加元素
typedef void (*ngx_rbtree_insert_pt) (ngx_rbtree_node_t *root,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);

struct ngx_rbtree_s {
    ngx_rbtree_node_t *root; //根节点
    ngx_rbtree_node_t *sentinel; //设置树的哨兵节点
    //表示红黑树添加元素的函数指针，它决定在添加新节点时的行为究竟是替换还是新增
    ngx_rbtree_insert_pt insert; //插入方法的函数指针
};

//初始化红黑树
#define ngx_rbtree_init(tree, s, i) \
    ngx_rbtree_sentinel_init(s);    \
    (tree)->root = s;               \
    (tree)->sentinel = s;           \
    (tree)->insert = i;
/*
向红黑树中插入节点。上面说过，向红黑树中插入节点，有可能破坏红黑树的性质，这时就需要调整红黑树，哪些情况会破坏红黑树的性质呢？
 下面三种情况会破坏红黑树的性质：
1）如果当前结点的父结点是红色且祖父结点的另一个子结点（叔叔结点）是红色
2）当前节点的父节点是红色,叔叔节点是黑色，当前节点是其父节点的右子
3）当前节点的父节点是红色,叔叔节点是黑色，当前节点是其父节点的左子
下面是这三种情况的对应调整方法（这里只是父节点为祖父节点的左孩子（右孩子情况类同））：
如果当前结点的父结点是红色且祖父结点的另一个子结点（叔叔结点）是红色，调整方法如下：
1）将当前节点的父节点和叔叔节点涂黑，祖父结点涂红，把当前结点指向祖父节点，从新的当前节点重新开始算法
如果当前节点的父节点是红色,叔叔节点是黑色，当前节点是其父节点的右子，调整方法如下：
2）当前节点的父节点做为新的当前节点，以新当前节点为支点左旋
如果当前节点的父节点是红色,叔叔节点是黑色，当前节点是其父节点的左子，调整方法如下：
3）父节点变为黑色，祖父节点变为红色，在祖父节点为支点右旋
 */
void ngx_rbtree_insert(ngx_rbtree_t *tree, ngx_rbtree_node_t *node);

/*
 * 实际上红黑树的删除操作，是通过将待删除节点与其后继节点的值进行互换后，通过删除后续节点来完成的；
 * 由于后续节点必定是只有一个子结点或者没有子节点的情况，因此有以下几条性质是在红黑树删除时，应知晓的：

（1）删除操作中真正被删除的必定是只有一个红色孩子或没有孩子的节点。
（2）如果真正的删除点是一个红色节点，那么它必定没有孩子节点。
（3）如果真正的删除点是一个黑色节点，那么它要么有一个红色的右孩子，要么没有孩子节点（针对找后继的情况）。

 case 1：删除的节点为红色，则它必定无孩子节点。可以直接删除
 case 2：删除的节点为黑色且有一个红色的右孩子，这时可以直接用右孩子替换删除节点，将右孩子修改为黑色，红黑性不破坏。
 case 3：删除节点为黑色没有孩子节点，那么它有一对NIL节点构成的孩子节点，此时情况较为复杂，有以下几种情况：

 */
void ngx_rbtree_delete(ngx_rbtree_t *tree, ngx_rbtree_node_t *node);

/* 这里只是将节点插入到红黑树中，并没有判断是否满足红黑树的性质；
 * 类似于二叉查找树的插入操作，这个函数为红黑树插入操作的函数指针；
 */
void ngx_rbtree_insert_value(ngx_rbtree_node_t *root, ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel);

//向红黑树中添加数据节点，每个数据节点的关键字表示时间或者时间差
void ngx_rbtree_insert_timer_value(ngx_rbtree_node_t *root, ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel);

//后继
ngx_rbtree_node_t *ngx_rbtree_next(ngx_rbtree_t *tree, ngx_rbtree_node_t *node);

#define ngx_rbt_red(node)               ((node)->color = 1)
#define ngx_rbt_black(node)             ((node)->color = 0)
//若节点颜色为红色返回非0，否则返回0
#define ngx_rbt_is_red(node)            ((node)->color)
//若节点颜色为黑色返回非0，否则返回0
#define ngx_rbt_is_black(node)          (!ngx_rbt_is_red(node))
//把节点2的颜色赋值给节点1
#define ngx_rbt_copy_color(n1, n2)      (n1->color = n2->color)

/* a sentinel must be black */
#define ngx_rbtree_sentinel_init(node) ngx_rbt_black(node);

//找到最左值，一直遍历到哨兵节点
static ngx_inline ngx_rbtree_node_t *
ngx_rbtree_min(ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    while(node->left != sentinel) {
        node = node->left;
    }

    return node;
}

#endif //NGX_RBTREE_NGX_RBTREE_H
