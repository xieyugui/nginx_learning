/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui
* @software:  CLion
* @file:      avltree.c
* @date:      2017/12/21 上午11:20
* @desc:
*/

//
// Created by daemon.xie on 2017/12/21.
//
#include <stdio.h>
#include <stdlib.h>
#include "avltree.h"

#define HEIGHT(p) ((p==NULL)? -1 : (((Node *)(p))->height))
#define Max(a, b) ( (a) > (b) ? (a) : (b) )

/*
 * 获取AVL树的高度
 */
int avltree_height(AVLTree tree)
{
    return HEIGHT(tree);
}

/*
 * 前序遍历AVL树
 */
void preorder_avltree(AVLTree tree)
{
    if (tree != NULL) {
        printf("%d ", tree->key);
        preorder_avltree(tree->left);
        preorder_avltree(tree->right);
    }
}

/*
 * 中序遍历AVL树
 */
void inorder_avltree(AVLTree tree)
{
    if (tree != NULL) {
        inorder_avltree(tree->left);
        printf("%d ", tree->key);
        inorder_avltree(tree->right);
    }
}

/*
 * 后序遍历AVL树
 */
void postorder_avltree(AVLTree tree)
{
    if (tree != NULL) {
        postorder_avltree(tree->left);
        postorder_avltree(tree->right);
        printf("%d ", tree->key);
    }
}

/*
 * 打印"AVL树"
 *
 * tree       -- AVL树的节点
 * key        -- 节点的键值
 * direction  --  0，表示该节点是根节点;
 *               -1，表示该节点是它的父结点的左孩子;
 *                1，表示该节点是它的父结点的右孩子。
 */
void print_avltree(AVLTree tree, int key, int direction)
{
    if(tree != NULL) {
        if(direction == 0) {
            printf("%2d is root\n", tree->key, key);
        } else {
            printf("%2d is %2d's %6s child\n", tree->key, key, direction==1?"right" : "left");
        }

        print_avltree(tree->left, tree->key, -1);
        print_avltree(tree->right, tree->key, 1);
    }
}

/*
 *（递归）查找AVL树x中键值为key的节点
 */
Node* avltree_search(AVLTree x, int key)
{
    if(x == NULL || x->key == key) {
        return x;
    }

    if(x->key > key) {
        return avltree_search(x->left, key);
    } else {
        return avltree_search(x->right, key);
    }

}

/*
 * (非递归实现)查找"AVL树x"中键值为key的节点
 */
Node* iterative_avltree_search(AVLTree x, int key)
{
    while(x != NULL && x->key != key){
        if(x->key > key){
            x = x->left;
        } else {
            x = x->right;
        }
    }

    return x;
}

/*
 * 查找最小节点：返回tree为根节点的avl树的最小节点
 */
Node* avltree_minimum(AVLTree tree)
{
    if(tree == NULL) {
        return NULL;
    }

    while(tree->left != NULL) {
        tree = tree->left;
    }

    return tree;
}

/*
 * 查找最大节点
 */
Node* avltree_maximum(AVLTree tree)
{
    if (tree == NULL) {
        return NULL;
    }

    while(tree->right != NULL) {
        tree = tree->right;
    }

    return tree;
}

/*
 * LL：左左对应的情况(右旋转)。
 *
 * 返回值：旋转后的根节点
 */
Node* left_left_rotation(AVLTree k2)
{
    AVLTree k1;

    k1 = k2->left;
    k2->left = k1->right;
    k1->right = k2;

    k1->height = Max(HEIGHT(k1->left), k2->height) +1;
    k2->height = Max(HEIGHT(k2->left), HEIGHT(k2->right)) +1;

    return k1;
}

/*
 * RR：右右对应的情况(左旋转)。
 *
 * 返回值：旋转后的根节点
 */
Node* right_right_rotation(AVLTree k1)
{
    AVLTree k2;

    k2 = k1->right;
    k1->right = k2->left;
    k2->left = k1;

    k1->height = Max( HEIGHT(k1->left), HEIGHT(k1->right)) + 1;
    k2->height = Max( HEIGHT(k2->right), k1->height) + 1;

    return k2;
}

/*
 * LR：左右对应的情况(左旋+右旋转)。
 *
 * 返回值：旋转后的根节点
 */
Node* left_right_rotation(AVLTree k3)
{
    k3->left = right_right_rotation(k3->left);

    return left_left_rotation(k3);
}

/*
 * RL：右左对应的情况(右旋+左旋转)。
 *
 * 返回值：旋转后的根节点
 */
Node* right_left_rotation(AVLTree k1)
{
    k1->right = left_left_rotation(k1->right);

    return right_right_rotation(k1);
}

/*
 * 创建AVL树结点。
 *
 * 参数说明：
 *     key 是键值。
 *     left 是左孩子。
 *     right 是右孩子。
 */
Node* avltree_create_node(int key, Node *left, Node* right)
{
    Node *p;

    if((p = (Node *)malloc(sizeof(Node))) == NULL) {
        return NULL;
    }

    p->key = key;
    p->height = 0;
    p->left = left;
    p->right = right;

    return p;
}

/*
 * 将节点插入avl树中，返回根节点
 */
Node* avltree_insert(AVLTree tree, int key)
{
    if(tree == NULL) {
        tree = avltree_create_node(key, NULL, NULL);
        if(tree == NULL) {
            printf("ERROR: create avltree node failed!\n");
            return NULL;
        }
    } else if(key < tree->key) {
        tree->left = avltree_insert(tree->left, key);
        // 插入节点后，若AVL树失去平衡，则进行相应的调节
        if (HEIGHT(tree->left) - HEIGHT(tree->right) == 2){
            if(key < tree->left->key) {
                tree = left_left_rotation(tree);
            } else {
                tree = left_right_rotation(tree);
            }
        }

    } else if(key > tree->key) {
        tree->right = avltree_insert(tree->right, key);
        // 插入节点后，若AVL树失去平衡，则进行相应的调节
        if (HEIGHT(tree->right) - HEIGHT(tree->left) == 2){
            if(key > tree->right->key) {
                tree = right_right_rotation(tree);
            } else {
                tree = right_left_rotation(tree);
            }
        }
    } else {
        printf("ERROR: cannot insert same value node!\n");
    }

    tree->height = Max(HEIGHT(tree->left), HEIGHT(tree->right)) + 1;

    return tree;
}

/*
 * 删除节点，返回根节点
 */
Node* delete_node(AVLTree tree,  Node *z)
{
    //根为空 或者 没有要删除的节点，直接返回NULL
    if (tree == NULL || z == NULL) {
        return NULL;
    }

    if (z->key < tree->key) {
        tree->left = avltree_delete(tree->left, z);
        // 删除节点后，若AVL树失去平衡，则进行相应的调节
        if(HEIGHT(tree->right) - HEIGHT(tree->left) == 2) {
            Node *r = tree->right;
            if(HEIGHT(r->left) > HEIGHT(r->right)) {
                tree = right_left_rotation(tree);
            } else {
                tree = right_right_rotation(tree);
            }
        }
    } else if (z->key > tree->key) {// 待删除的节点在"tree的右子树"中
        tree->right = avltree_delete(tree->right, z);
        // 删除节点后，若AVL树失去平衡，则进行相应的调节
        if (HEIGHT(tree->left) - HEIGHT(tree->right) == 2){
            Node *l =  tree->left;
            if (HEIGHT(l->right) > HEIGHT(l->left)) {
                tree = left_right_rotation(tree);
            } else {
                tree = left_left_rotation(tree);
            }

        }
    } else { // tree是对应要删除的节点。
        // tree的左右孩子都非空
        if(tree->left != NULL && tree->right != NULL) {
            if (HEIGHT(tree->left) > HEIGHT(tree->right))
            {
                // 如果tree的左子树比右子树高；
                // 则(01)找出tree的左子树中的最大节点
                //   (02)将该最大节点的值赋值给tree。
                //   (03)删除该最大节点。
                // 这类似于用"tree的左子树中最大节点"做"tree"的替身；
                // 采用这种方式的好处是：删除"tree的左子树中最大节点"之后，AVL树仍然是平衡的。
                Node *max = avltree_maximum(tree->left);
                tree->key = max->key;
                tree->left = delete_node(tree->left, max);
            }
            else
            {
                // 如果tree的左子树不比右子树高(即它们相等，或右子树比左子树高1)
                // 则(01)找出tree的右子树中的最小节点
                //   (02)将该最小节点的值赋值给tree。
                //   (03)删除该最小节点。
                // 这类似于用"tree的右子树中最小节点"做"tree"的替身；
                // 采用这种方式的好处是：删除"tree的右子树中最小节点"之后，AVL树仍然是平衡的。
                Node *min = avltree_maximum(tree->right);
                tree->key = min->key;
                tree->right = delete_node(tree->right, min);
            }
        } else {
            Node *tmp = tree;
            tree = tree->left ? tree->left : tree->right;
            free(tmp);
        }
    }

    return tree;
}

Node* avltree_delete(AVLTree tree, int key)
{
   Node *z;

   if ((z = avltree_search(tree, key)) != NULL)
       tree = delete_node(tree, z);
   return tree;
}

/*
 * 销毁AVL树
 */
void destroy_avltree(AVLTree tree)
{
    if (tree == NULL) {
        return ;
    }

    if(tree->left != NULL) {
        destroy_avltree(tree->left);
    }
    if(tree->right != NULL) {
        destroy_avltree(tree->right);
    }

    free(tree);
}

/*########################test#######################*/
static int arr[]= {3,2,1,4,5,6,7,16,15,14,13,12,11,10,8,9};
#define TBL_SIZE(a) ( (sizeof(a)) / (sizeof(a[0])) )

void main()
{
    int i,ilen;
    AVLTree root=NULL;

    printf("== 高度: %d\n", avltree_height(root));
    printf("== 依次添加: ");
    ilen = TBL_SIZE(arr);
    for(i=0; i<ilen; i++)
    {
        printf("%d ", arr[i]);
        root = avltree_insert(root, arr[i]);
    }

    printf("\n== 前序遍历: ");
    preorder_avltree(root);

    printf("\n== 中序遍历: ");
    inorder_avltree(root);

    printf("\n== 后序遍历: ");
    postorder_avltree(root);
    printf("\n");

    printf("== 高度: %d\n", avltree_height(root));
    printf("== 最小值: %d\n", avltree_minimum(root)->key);
    printf("== 最大值: %d\n", avltree_maximum(root)->key);
    printf("== 树的详细信息: \n");
    print_avltree(root, root->key, 0);


    i = 8;
    printf("\n== 删除根节点: %d", i);
    root = avltree_delete(root, i);

    printf("\n== 高度: %d", avltree_height(root));
    printf("\n== 中序遍历: ");
    inorder_avltree(root);
    printf("\n== 树的详细信息: \n");
    print_avltree(root, root->key, 0);

    // 销毁二叉树
    destroy_avltree(root);
}
/*########################test#######################*/