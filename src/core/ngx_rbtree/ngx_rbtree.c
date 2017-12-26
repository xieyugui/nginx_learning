/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_rbtree.c
* @date:      2017/12/25 下午2:53
* @desc:
*/

//
// Created by daemon.xie on 2017/12/25.
//
#include <ngx_config.h>
#include <ngx_core.h>

static ngx_inline void ngx_rbtree_left_rotate(ngx_rbtree_node_t **root,
    ngx_rbtree_node_t *sentinel, ngx_rbtree_node_t *node);
static ngx_inline void ngx_rbtree_right_rotate(ngx_rbtree_node_t **root,
    ngx_rbtree_node_t *sentinel, ngx_rbtree_node_t *node);

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

/* 插入节点 */
/* 插入节点的步骤：
 * 1、首先按照二叉查找树的插入操作插入新节点；
 * 2、然后把新节点着色为红色（避免破坏红黑树性质5）；
 * 3、为维持红黑树的性质，调整红黑树的节点（着色并旋转），使其满足红黑树的性质；
 */
void
ngx_rbtree_insert(ngx_rbtree_t *tree, ngx_rbtree_node_t *node)
{
    ngx_rbtree_node_t **root, *temp, *sentinel;

    root = &tree->root;
    sentinel = tree->sentinel;
    //如果为空
    /* 若红黑树为空，则比较简单，把新节点作为根节点，
     * 并初始化该节点使其满足红黑树性质
     */
    if(*root == sentinel) {
        node->parent = NULL;
        node->left = sentinel;
        node->right = sentinel;
        ngx_rbt_black(node);
        *root = node;

        return;
    }
    /* 若红黑树不为空，则按照二叉查找树的插入操作进行
     * 该操作由函数指针提供
     */
    tree->insert(*root, node, sentinel);

    /*插入调整*/

    /* 调整红黑树，使其满足性质，
     * 其实这里只是破坏了性质4：若一个节点是红色，则孩子节点都为黑色；
     * 若破坏了性质4，则新节点 node 及其父亲节点 node->parent 都为红色；
     */
    //当前节点不是根节点，并且节点的父节点颜色为红色
    while (node != *root && ngx_rbt_is_red(node->parent)) {

        /* 若node的父亲节点是其祖父节点的左孩子 */
        if(node->parent == node->parent->parent->left) {
            temp = node->parent->parent->right;//叔节点
            /* case1：node的叔叔节点是红色 */
            /* 此时，node的父亲及叔叔节点都为红色；
             * 解决办法：将node的父亲及叔叔节点着色为黑色，将node祖父节点着色为红色；
             * 然后沿着祖父节点向上判断是否会破会红黑树的性质；
             */
            if(ngx_rbt_is_red(temp)) {
                ngx_rbt_black(node->parent);
                ngx_rbt_black(temp);
                ngx_rbt_red(node->parent->parent);
                node = node->parent->parent; //修改祖父节点为当前节点，重新开始算法

            } else {//否则就为黑色
                /* case2：node的叔叔节点是黑色且node是父亲节点的右孩子 */
                /* 则此时，以node父亲节点进行左旋转，使case2转变为case3；
                 */
                if (node == node->parent->right) {
                    node = node->parent;
                    ngx_rbtree_left_rotate(root, sentinel, node); //左旋当前节点
                }

                /* case3：node的叔叔节点是黑色且node是父亲节点的左孩子 */
                /* 首先，将node的父亲节点着色为黑色，祖父节点着色为红色；
                 * 然后以祖父节点进行一次右旋转；
                 */
                ngx_rbt_black(node->parent);
                ngx_rbt_red(node->parent->parent);
                ngx_rbtree_right_rotate(root, sentinel, node->parent->parent);
            }
        } else {//否则父节点为右孩子
            /* 若node的父亲节点是其祖父节点的右孩子 */
            /* 这里跟上面的情况是对称的，就不再进行讲解了
             */
            temp = node->parent->parent->left;

            if(ngx_rbt_is_red(temp)) {
                ngx_rbt_black(node->parent);
                ngx_rbt_black(temp);
                ngx_rbt_red(node->parent->parent);
                node = node->parent->parent;

            } else {
                if (node == node->parent->left) {
                    node = node->parent;
                    ngx_rbtree_right_rotate(root, sentinel, node);
                }

                ngx_rbt_black(node->parent);
                ngx_rbt_red(node->parent->parent);
                ngx_rbtree_left_rotate(root, sentinel, node->parent->parent);
            }
        }
    }

    //设置根为黑色
    ngx_rbt_black(*root);
}

void
ngx_rbtree_insert_value(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel)
{//遍历树temp找到node那个合适的插入NIL位置(p)
    ngx_rbtree_node_t **p;

    for ( ;; ) {
        p = (node->key < temp->key) ? &temp->left : &temp->right;
        //若node->key == temp->key时插入到temp->right位置，若想要自行定义key相同时的方法可以重写ngx_rbtree_insert方法
        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;//p是个NIL就是node带插入的位置，而temp指向p的父节点
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);//插入节点默认是红色
}

void
ngx_rbtree_insert_timer_value(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node,
                              ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t  **p;

    for ( ;; ) {

        /*
         * Timer values
         * 1) are spread in small range, usually several minutes,
         * 2) and overflow each 49 days, if milliseconds are stored in 32 bits.
         * The comparison takes into account that overflow.
         */

        /*  node->key < temp->key */

        p = ((ngx_rbtree_key_int_t) (node->key - temp->key) < 0)
            ? &temp->left : &temp->right;

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}

void
ngx_rbtree_delete(ngx_rbtree_t *tree, ngx_rbtree_node_t *node)
{
    ngx_uint_t red;
    ngx_rbtree_node_t **root, *sentinel, *subst, *temp, *w;

    root = &tree->root;
    sentinel = tree->sentinel;

    /* 下面是获取temp节点值，temp保存的节点是准备替换节点node ；
     * subst是保存要被替换的节点的后继节点；
     */

    /* case1：若node节点没有左孩子（这里包含了存在或不存在右孩子的情况）*/
    if(node->left == sentinel) {
        temp = node->right;
        subst = node;

    } else if(node->right == sentinel) { /* case2：node节点存在左孩子，但是不存在右孩子 */
        temp = node->left;
        subst = node;
    } else {/* case3：node节点既有左孩子，又有右孩子 */
        subst = ngx_rbtree_min(node->right, sentinel);/* 获取node节点的后续节点 */

        if(subst->left != sentinel) {
            temp = subst->left;
        } else {
            temp = subst->right;
        }
    }
    //subst是转换后只有一个儿子的节点(若node有两个儿子则subst是node右子树上最小的节点，若node至多只有一个儿子

    //简单情形1: 待删除的节点为根节点且该根节点至多只有一个儿子，只用用根节点的儿子替代根节点并重绘新根为黑色即可
    /* 若被替换的节点subst是根节点，则temp直接替换subst称为根节点 */
    if (subst == *root) {
        *root = temp;

        ngx_rbt_black(temp);

        node->left = NULL;
        node->right = NULL;
        node->parent = NULL;
        node->key = 0;

        return;
    }

    /* red记录subst节点的颜色 */
    red = ngx_rbt_is_red(subst); //是subst为红色则red为1，否则red为0

    //将subst和其父节点断开连接(subst是最终要删除的节点)
    /* temp节点替换subst 节点 */
    if(subst == subst->parent->left) {
        subst->parent->left = temp;
    } else {
        subst->parent->right = temp;
    }

    //这是最开始要删除的节点node本身至多只有一个儿子那么subst == node
    /* 根据subst是否为node节点进行处理 */
    if (subst == node) {
        //重连subst的儿子(也是node的儿子)到subst的父节点
        temp->parent = subst->parent;

    } else {
        //node有两个儿子，那么subst是node右子树最小的节点(右子树最左)

        if (subst->parent == node) {
            temp->parent = subst;
        } else {
            //删除当前的右子树最小节点
            temp->parent = subst->parent;
        }

        //右子树最小值代替当前要删除的节点
        //将subst替换到node的位置上
        subst->left = node->left;
        subst->right = node->right;
        subst->parent = node->parent;
        //将node的颜色赋值给subst
        ngx_rbt_copy_color(subst, node);

        if(node == *root) {
            *root = subst;

        } else {
            if(node == node->parent->left) {
                node->parent->left = subst;
            } else {
                node->parent->right = subst;
            }
        }

        if(subst->left != sentinel) {
            subst->left->parent = subst;
        }

        if(subst->right != sentinel) {
            subst->right->parent = subst;
        }
    }

    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    node->key = 0;

    //如果要删除的点为红色，则没有破坏红黑树的特性
    if(red) {
        return;
    }

    //注意此后都是针对temp操作，因为是temp所在路径删除了一个黑色节点(这里是将subst黑色提到了原来node的位置故少
    // 了一个黑色节点)，需要对temp重新平衡

    while(temp != *root && ngx_rbt_black(temp)) {

        if(temp == temp->parent->left) {
            w = temp->parent->right;
            /* case A：temp兄弟节点为红色 */
            /* 解决办法：
             * 1、改变w节点及temp父亲节点的颜色；
             * 2、对temp父亲节的做一次左旋转，此时，temp的兄弟节点是旋转之前w的某个子节点，该子节点颜色为黑色；
             * 3、此时，case A已经转换为case B、case C 或 case D；
             */
            if (ngx_rbt_is_red(w)) {
                ngx_rbt_black(w); //解决： 兄弟节点修改为黑色
                ngx_rbt_red(temp->parent); //父节点修改红色
                ngx_rbtree_left_rotate(root, sentinel, temp->parent); //左旋父节点
                w = temp->parent->right;
            }
            //当前节点为黑色，兄弟节点为黑色，兄弟节点的俩子节点也为黑色
            /* case B：temp的兄弟节点w是黑色，且w的两个子节点都是黑色 */
            /* 解决办法：
             * 1、改变w节点的颜色；
             * 2、把temp的父亲节点作为新的temp节点；
             */
            if(ngx_rbt_is_black(w->left) && ngx_rbt_is_black(w->right)) {
                ngx_rbt_red(w); //解决：兄弟节点修改为红色
                //注意这里已经包括了wikipedia上的case4，即temp->parent红色的话退出while循环直接将其重绘为黑色
                temp = temp->parent;

            } else {//w的左儿子和右儿子颜色不同
                /* case C：temp的兄弟节点是黑色，且w的左孩子是红色，右孩子是黑色 */
                /* 解决办法：
                 * 1、将改变w及其左孩子的颜色；
                 * 2、对w节点进行一次右旋转；
                 * 3、此时，temp新的兄弟节点w有着一个红色右孩子的黑色节点，转为case D；
                 */
                if(ngx_rbt_is_black(w->right)) {
                    //case5:w的右儿子为黑色，w的左儿子必为红色，重绘w->right为黑色，并对w进行右旋然后进入case6
                    ngx_rbt_black(w->left);
                    ngx_rbt_red(w);
                    ngx_rbtree_right_rotate(root, sentinel, w);
                    w = temp->parent->right;
                }

                /* case D：temp的兄弟节点w为黑色，且w的右孩子为红色 */
                /* 解决办法：
                 * 1、将w节点设置为temp父亲节点的颜色，temp父亲节点设置为黑色；
                 * 2、w的右孩子设置为黑色；
                 * 3、对temp的父亲节点做一次左旋转；
                 * 4、最后把根节点root设置为temp节点；*/
                ngx_rbt_copy_color(w, temp->parent);
                ngx_rbt_black(temp->parent);
                ngx_rbt_black(w->right);
                ngx_rbtree_left_rotate(root, sentinel, temp->parent);
                temp = *root;//删除结束退出循环体

            }

        } else {//当前节点为右节点的情况
            w = temp->parent->left;

            if (ngx_rbt_is_red(w)) {
                ngx_rbt_black(w);
                ngx_rbt_red(temp->parent);
                ngx_rbtree_right_rotate(root, sentinel, temp->parent);
                w = temp->parent->left;
            }

            if (ngx_rbt_is_black(w->left) && ngx_rbt_is_black(w->right)) {
                ngx_rbt_red(w);
                temp = temp->parent;

            } else {
                if (ngx_rbt_is_black(w->left)) {
                    ngx_rbt_black(w->right);
                    ngx_rbt_red(w);
                    ngx_rbtree_left_rotate(root, sentinel, w);
                    w = temp->parent->left;
                }

                ngx_rbt_copy_color(w, temp->parent);
                ngx_rbt_black(temp->parent);
                ngx_rbt_black(w->left);
                ngx_rbtree_right_rotate(root, sentinel, temp->parent);
                temp = *root;
            }
        }
    }

    ngx_rbt_black(temp);
}


static ngx_inline void
ngx_rbtree_left_rotate(ngx_rbtree_node_t **root, ngx_rbtree_node_t *sentinel,
                       ngx_rbtree_node_t *node)
{//左旋：交换节点和其右儿子的位置
    ngx_rbtree_node_t  *temp;

    temp = node->right;
    node->right = temp->left;

    if (temp->left != sentinel) {
        temp->left->parent = node;
    }

    temp->parent = node->parent;

    if (node == *root) {
        *root = temp;

    } else if (node == node->parent->left) {
        node->parent->left = temp;

    } else {
        node->parent->right = temp;
    }

    temp->left = node;
    node->parent = temp;
}

static ngx_inline void
ngx_rbtree_right_rotate(ngx_rbtree_node_t **root, ngx_rbtree_node_t *sentinel,
                        ngx_rbtree_node_t *node)
{//右旋：交换节点和其左儿子的位置
    ngx_rbtree_node_t  *temp;

    temp = node->left;
    node->left = temp->right;

    if (temp->right != sentinel) {
        temp->right->parent = node;
    }

    temp->parent = node->parent;

    if (node == *root) {
        *root = temp;

    } else if (node == node->parent->right) {
        node->parent->right = temp;

    } else {
        node->parent->left = temp;
    }

    temp->right = node;
    node->parent = temp;
}

//后继
ngx_rbtree_node_t *
ngx_rbtree_next(ngx_rbtree_t *tree, ngx_rbtree_node_t *node)
{
    ngx_rbtree_node_t  *root, *sentinel, *parent;

    sentinel = tree->sentinel;

    if (node->right != sentinel) {
        return ngx_rbtree_min(node->right, sentinel);
    }

    root = tree->root;

    for ( ;; ) {
        parent = node->parent;

        if (node == root) {
            return NULL;
        }

        if (node == parent->left) {
            return parent;
        }

        node = parent;
    }
}