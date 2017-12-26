#include <stdio.h>
#include <ngx_rbtree.h>

typedef struct rbtree_node {
    ngx_rbtree_node_t node;
    ngx_uint_t num;
}RBTreeNode;

static int arr[]= {1,6,8,11,13,15,17,22,25,27};
#define TBL_SIZE(a) ( (sizeof(a)) / (sizeof(a[0])) )

int main() {
    ngx_rbtree_t rbtree;
    ngx_rbtree_node_t sentinel;
    int i, ilen;
    RBTreeNode rbn[10];

    ngx_rbtree_init(&rbtree, &sentinel, ngx_rbtree_insert_value);
    ilen = TBL_SIZE(arr);
    for(i=0; i<ilen; i++)
    {
        rbn[i].num = arr[i];
        rbn[i].node.key = rbn[i].num;
        ngx_rbtree_insert(&rbtree, &rbn[i].node);
    }

    //查找红黑树中最小节点
    ngx_rbtree_node_t *tmpnode = ngx_rbtree_min(rbtree.root, &sentinel);
    printf("the min key node num val:%u\n", ((RBTreeNode *)(tmpnode))->num );

    //查找13节点
    ngx_uint_t lookupkey = 13;
    tmpnode = rbtree.root;
    RBTreeNode *lknode;
    while(tmpnode != &sentinel )
    {
        if(lookupkey != tmpnode->key)
        {
            tmpnode = (lookupkey < tmpnode->key)?tmpnode->left:tmpnode->right;
            continue;
        }
        lknode = (RBTreeNode *)tmpnode;
        break;
    }

    //删除num为13的节点
    if(lknode != NULL)
        ngx_rbtree_delete(&rbtree, &lknode->node);

    return 0;
}