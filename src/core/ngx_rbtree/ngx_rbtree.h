/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui
* @software:  CLion
* @file:      ngx_rbtree.h
* @date:      2017/12/25 下午2:53
* @desc:
 *
 * 分析 https://www.cnblogs.com/doop-ymc/p/3440316.html
 *      http://www.cnblogs.com/skywang12345/p/3624177.html
 *
 *      https://www.sohu.com/a/201923614_466939
 *      https://www.cnblogs.com/sandy2013/p/3270999.html   插入图解
 *      https://blog.csdn.net/qq_37169817/article/details/78880110  删除图解
*/

//
// Created by daemon.xie on 2017/12/25.
//

#ifndef NGX_RBTREE_NGX_RBTREE_H
#define NGX_RBTREE_NGX_RBTREE_H

#include <ngx_config.h>
#include <ngx_core.h>

/*
红黑树的几个特性：
(1) 每个节点或者是黑色，或者是红色。
(2) 根节点是黑色。
(3) 每个叶子节点是黑色。 [注意：这里叶子节点，是指为空的叶子节点！]
(4) 如果一个节点是红色的，则它的子节点必须是黑色的。
(5) 从一个节点到该节点的子孙节点的所有路径上包含相同数目的黑节点。
 */

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

//插入函数
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
 case 1：红黑树为空，新节点插入作为根结点
 case 2：新节点的父节点为黑色，此时直接插入后不会破坏红黑树的性质，可以直接插入
 case 3：父节点为红色，叔父结点也为红色；则需要将父节点及叔父节点均调整为黑色，然后将祖父结点调整为红色，再以祖父结点为起点，进行红黑性恢复的起点，进行继续调整
 case 4:父节点为红色，叔父节点为黑色，父节点为祖父节点的左节点，插入结点在父节点的左节点；需要以插入节点的左节点进行一次右旋转，此处理类型为LL型
 case 5:父节点为红色，叔父节点为黑色，父节点为祖父节点的左节点，插入结点在父节点的右节点；
            需要先以插入节点的父亲节点为旋转点先进行一次左旋转，再以插入节点的的祖父节点进行一次右旋转，此处理类型即为LR型
 case 6:父节点为红色，叔父节点为黑色，父节点为祖父节点的右节点，插入结点在父节点的右节点；需要以插入节点的左节点进行一次左旋转，此处理类型为RR型
 case 7:父节点为红色，叔父节点为黑色，父节点为祖父节点的右节点，插入结点在父节点的左节点；需要先以插入节点的父亲节点为旋转点先进行一次右旋转，
            再以插入节点的的祖父节点进行一次左旋转，此处理类型即为RL型
 */
void ngx_rbtree_insert(ngx_rbtree_t *tree, ngx_rbtree_node_t *node);

/*
第一步：将红黑树当作一颗二叉查找树，将节点删除。
       这和"删除常规二叉查找树中删除节点的方法是一样的"。分3种情况：
       ① 被删除节点没有儿子，即为叶节点。那么，直接将该节点删除就OK了。
       ② 被删除节点只有一个儿子。那么，直接删除该节点，并用该节点的唯一子节点顶替它的位置。
       ③ 被删除节点有两个儿子。那么，先找出它的后继节点；然后把“它的后继节点的内容”复制给“该节点的内容”；之后，删除“它的后继节点”。
       在这里，后继节点相当于替身，在将后继节点的内容复制给"被删除节点"之后，再将后继节点删除。这样就巧妙的将问题转换为"删除后继节点"的情况了，
       下面就考虑后继节点。 在"被删除节点"有两个非空子节点的情况下，它的后继节点不可能是双子非空。既然"的后继节点"不可能双子都非空，
       就意味着"该节点的后继节点"要么没有儿子，要么只有一个儿子。若没有儿子，则按"情况① "进行处理；若只有一个儿子，则按"情况② "进行处理。

第二步：通过"旋转和重新着色"等一系列来修正该树，使之重新成为一棵红黑树。
       因为"第一步"中删除节点之后，可能会违背红黑树的特性。所以需要通过"旋转和重新着色"来修正该树，使之重新成为一棵红黑树。
 */
void ngx_rbtree_delete(ngx_rbtree_t *tree, ngx_rbtree_node_t *node);

/* 这里只是将节点插入到红黑树中，并没有判断是否满足红黑树的性质；
 * 类似于二叉查找树的插入操作，这个函数为红黑树插入操作的函数指针；
 */
void ngx_rbtree_insert_value(ngx_rbtree_node_t *root, ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel);

//向红黑树中添加数据节点，每个数据节点的关键字表示时间或者时间差
//nginx定义的ngx_rbtree_insert_pt方法，和前面的ngx_rbtree_insert_value性质类似，区别是该方法下的红黑树key表示时间或者时间差
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
