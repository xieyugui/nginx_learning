/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_radix_tree.c
* @date:      2017/12/27 下午2:49
* @desc:
*/

//
// Created by daemon.xie on 2017/12/27.
//

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_radix_tree.h>

//为基数树申请节点
static ngx_radix_node_t *ngx_radix_alloc(ngx_radix_tree_t *tree);

ngx_radix_tree_t *
ngx_radix_tree_create(ngx_pool_t *pool, ngx_int_t preallocate)
{
    uint32_t key, mask, inc;
    ngx_radix_tree_t *tree;

    tree = ngx_palloc(pool, sizeof(ngx_radix_tree_t));
    if(tree == NULL) {
        return NULL;
    }

    tree->pool = pool;
    tree->free = NULL;
    tree->start = NULL;
    tree->size = 0;

    tree->root = ngx_radix_alloc(tree);
    if(tree->root == NULL) {
        return NULL;
    }

    tree->root->right = NULL;
    tree->root->left = NULL;
    tree->root->parent = NULL;
    tree->root->value = NGX_RADIX_NO_VALUE;

    //如果需要的预分配结点为0个，完成返回
    if(preallocate == 0) {
        return tree;
    }

    //如果预分配为-1，则按系统的页大小预分配页，以下为根据页面大小，确定preallocate
    if(preallocate == -1) {
        switch(ngx_pagesize / sizeof(ngx_radix_node_t)) {
            case 128:
                preallocate = 6;
                break;
            case 256:
                preallocate = 7;
                break;

            default:
                preallocate = 8;
        }
    }

    mask = 0;
    //inc 的二进制形式为 1000 0000 0000 0000 0000 0000 0000 0000，逐渐向右移动
    inc = 0x80000000;

    //加入preallocate=7,最终建的基数树的节点总个数为2^(preallocate+1)-1，每一层个数为2^(7-preallocate)
    //二叉树 计算节点的方法
    //循环如下：
    //preallocate  =      7         6        5         4         3         2        1
    //mask(最左8位)=      10000000  11000000 11100000  11110000  11111000  11111100 11111110
    //inc          =     10000000  01000000 00100000  00010000  00001000  00000100 00000010
    //增加节点个数    =      2         4        8         16        32        64       128
    while(preallocate--) {
        key = 0;
        mask >>= 1;
        mask |= 0x80000000;

        do {
            if (ngx_radix32tree_insert(tree, key, mask, NGX_RADIX_NO_VALUE) != NGX_OK){
                return NULL;
            }

            key += inc;
        } while (key);

        inc >>= 1;
    }

    return tree;
}

ngx_int_t
ngx_radix32tree_insert(ngx_radix_tree_t *tree, uint32_t key, uint32_t mask,
                       uintptr_t value)
{
    uint32_t bit;
    ngx_radix_node_t *node, *next;

    bit = 0x80000000;//从最左位开始，判断key值

    node = tree->root;
    next = tree->root;

    while(bit & mask) {//定位key对应的节点
        if(key & bit) {//如果是1，则右边
            next = node->right;
        } else {
            next = node->left;
        }

        if(next = NULL) {
            break;
        }

        bit >>= 1;
        node = next;
    }

    if(next) { //假设next不为空
        if (node->value != NGX_RADIX_NO_VALUE) {
            return NGX_BUSY;
        }

        node->value = value;
        return NGX_OK;
    }

    //假设next为中间节点。且为空，继续查找且申请路径上为空的节点
    //比方找key=1000111。在找到10001时next为空，那要就要申请三个节点分别存10001,100011,1000111,
    //1000111最后一个节点为key要插入的节点

    while (bit & mask) { //没有到达最深层，继续向下申请节点
        next = ngx_radix_alloc(tree);
        if(next == NULL) {
            return NGX_ERROR;
        }

        next->right = NULL;
        next->left = NULL;
        next->parent = node;
        next->value = NGX_RADIX_NO_VALUE;

        if(key & bit) {
            node->right = next;
        } else {
            node->left = next;
        }

        bit >>= 1;
        node = next;
    }

    node->value = value;

    return NGX_OK;
}

ngx_int_t
ngx_radix32tree_delete(ngx_radix_tree_t *tree, uint32_t key, uint32_t mask)
{
    uint32_t bit;
    ngx_radix_node_t *node;

    bit = 0x80000000;
    node = tree->root;
    //依据key和掩码查找
    while (node && (bit & mask)) {
        if(key & bit) {
            node = node->right;
        } else {
            node = node->left;
        }

        bit >>= 1;
    }

    //未找到
    if(node == NULL) {
        return NGX_ERROR;
    }

    //node不为叶节点直接把value置为空
    if(node->right || node->left) {
        if(node->value != NGX_RADIX_NO_VALUE) {
            node->value = NGX_RADIX_NO_VALUE;
            return NGX_OK;
        }

        return NGX_ERROR;
    }

    //node为叶子节点，直接放到free区域
    for ( ;; ) {
        //删除叶子节点
        if(node->parent->right == node) {
            node->parent->right = NULL;

        } else {
            node->parent->left = NULL;
        }

        //直接将free链表 链接到 node 之后，然后再讲node 赋值给free
        node->right = tree->free;
        tree->free = node;

        //假如删除node以后。父节点是叶子节点，就继续删除父节点，一直到node不是叶子节点
        node = node->parent;

        if (node->right || node->left) { //node不为叶子节点
            break;
        }

        if (node->value != NGX_RADIX_NO_VALUE) {//node的value不为空
            break;
        }

        if(node->parent == NULL) {//node的parent为空
            break;
        }
    }

    return NGX_OK;
}

uintptr_t
ngx_radix32tree_find(ngx_radix_tree_t *tree, uint32_t key)
{
    uint32_t bit;
    uintptr_t value;
    ngx_radix_node_t *node;

    bit = 0x80000000;
    value = NGX_RADIX_NO_VALUE;
    node = tree->root;

    while (node) {
        if (node->value != NGX_RADIX_NO_VALUE) {
            value = node->value;
        }

        if(key & bit) {
            node = node->right;
        } else {
            node = node->right;
        }

        bit >>= 1;
    }

    return value;
}

#if (NGX_HAVE_INET6)

ngx_int_t
ngx_radix128tree_insert(ngx_radix_tree_t *tree, u_char *key, u_char *mask,
                        uintptr_t value)
{
    u_char             bit;
    ngx_uint_t         i;
    ngx_radix_node_t  *node, *next;

    i = 0;
    bit = 0x80;

    node = tree->root;
    next = tree->root;

    while (bit & mask[i]) {
        if (key[i] & bit) {
            next = node->right;

        } else {
            next = node->left;
        }

        if (next == NULL) {
            break;
        }

        bit >>= 1;
        node = next;

        if (bit == 0) {
            if (++i == 16) {
                break;
            }

            bit = 0x80;
        }
    }

    if (next) {
        if (node->value != NGX_RADIX_NO_VALUE) {
            return NGX_BUSY;
        }

        node->value = value;
        return NGX_OK;
    }

    while (bit & mask[i]) {
        next = ngx_radix_alloc(tree);
        if (next == NULL) {
            return NGX_ERROR;
        }

        next->right = NULL;
        next->left = NULL;
        next->parent = node;
        next->value = NGX_RADIX_NO_VALUE;

        if (key[i] & bit) {
            node->right = next;

        } else {
            node->left = next;
        }

        bit >>= 1;
        node = next;

        if (bit == 0) {
            if (++i == 16) {
                break;
            }

            bit = 0x80;
        }
    }

    node->value = value;

    return NGX_OK;
}


ngx_int_t
ngx_radix128tree_delete(ngx_radix_tree_t *tree, u_char *key, u_char *mask)
{
    u_char             bit;
    ngx_uint_t         i;
    ngx_radix_node_t  *node;

    i = 0;
    bit = 0x80;
    node = tree->root;

    while (node && (bit & mask[i])) {
        if (key[i] & bit) {
            node = node->right;

        } else {
            node = node->left;
        }

        bit >>= 1;

        if (bit == 0) {
            if (++i == 16) {
                break;
            }

            bit = 0x80;
        }
    }

    if (node == NULL) {
        return NGX_ERROR;
    }

    if (node->right || node->left) {
        if (node->value != NGX_RADIX_NO_VALUE) {
            node->value = NGX_RADIX_NO_VALUE;
            return NGX_OK;
        }

        return NGX_ERROR;
    }

    for ( ;; ) {
        if (node->parent->right == node) {
            node->parent->right = NULL;

        } else {
            node->parent->left = NULL;
        }

        node->right = tree->free;
        tree->free = node;

        node = node->parent;

        if (node->right || node->left) {
            break;
        }

        if (node->value != NGX_RADIX_NO_VALUE) {
            break;
        }

        if (node->parent == NULL) {
            break;
        }
    }

    return NGX_OK;
}


uintptr_t
ngx_radix128tree_find(ngx_radix_tree_t *tree, u_char *key)
{
    u_char             bit;
    uintptr_t          value;
    ngx_uint_t         i;
    ngx_radix_node_t  *node;

    i = 0;
    bit = 0x80;
    value = NGX_RADIX_NO_VALUE;
    node = tree->root;

    while (node) {
        if (node->value != NGX_RADIX_NO_VALUE) {
            value = node->value;
        }

        if (key[i] & bit) {
            node = node->right;

        } else {
            node = node->left;
        }

        bit >>= 1;

        if (bit == 0) {
            i++;
            bit = 0x80;
        }
    }

    return value;
}

#endif

static ngx_radix_node_t *
ngx_radix_alloc(ngx_radix_tree_t *tree)
{
    ngx_radix_node_t *p;

    //free中有可利用的空间节点
    if(tree->free){
        p = tree->free;
        tree->free = tree->free->right;
        return p;
    }

    if(tree->size < sizeof(ngx_radix_node_t)) {
        tree->start = ngx_pmemalign(tree->pool, ngx_pagesize, ngx_pagesize);
        if(tree->start == NULL) {
            return NULL;
        }

        tree->size = ngx_pagesize;
    }

    p = (ngx_radix_node_t *) tree->start;
    tree->start += sizeof(ngx_radix_node_t);
    tree->size -= sizeof(ngx_radix_node_t);

    return p;
}
