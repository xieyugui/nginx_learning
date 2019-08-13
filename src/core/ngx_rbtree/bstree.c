/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui
* @software:  CLion
* @file:      bstree.c
* @date:      2017/12/20 下午3:02
* @desc:
*/

//
// Created by daemon.xie on 2017/12/20.
//
#include <stdlib.h>
#include <stdio.h>
#include "bstree.h"

void preorder_bstree(BSTree tree)
{
    if (tree != NULL) {
        printf("%d ",tree->key);
        preorder_bstree(tree->left);
        preorder_bstree(tree->right);
    }
}

void inorder_bstree(BSTree tree)
{
    if (tree != NULL) {
        inorder_bstree(tree->left);
        printf("%d ",tree->key);
        inorder_bstree(tree->right);
    }
}

void postorder_bstree(BSTree tree)
{
    if (tree != NULL) {
        postorder_bstree(tree->left);
        postorder_bstree(tree->right);
        printf("%d ", tree->key);
    }
}

Node* bstree_search(BSTree tree, int key)
{
    if(tree != NULL && tree->key == key) {
        return tree;
    }

    if(tree->key < key) {
        return bstree_search(tree->right, key);
    } else{
        return bstree_search(tree->left, key);
    }
}

Node* iterative_bstree_search(BSTree tree, int key)
{
    while(tree != NULL && tree->key != key) {
        if(tree->key < key) {
            tree = tree->right;
        } else{
            tree = tree->left;
        }
    }

    return tree;
}

Node* bstree_minimum(BSTree tree)
{
    if (tree == NULL) {
        return NULL;
    }

    while(tree != NULL && tree->left != NULL) {
        tree = tree->left;
    }

    return tree;

}

Node* bstree_maximum(BSTree tree)
{
    if (tree == NULL) {
        return NULL;
    }

    while(tree != NULL && tree->right != NULL) {
        tree = tree->right;
    }

    return tree;
}

/**
 * 查找x的前驱节点，（比当前节点小一点，也就是左子树中，最大的点）
 * 如果x存在左孩子，则"x的前驱节点"为"以其中孩子为根的子树的最大节点"
 * 如果x不存在左孩子
 *   x 为左孩子，若该结点是其父结点的左孩子，那么需要沿着其父结点一直向树的顶端寻找，直到找到一个结点P，P结点是其父结点Q的右孩子，
 *      那么Q就是该结点的前驱结点
 *   x 为右孩子，则为父节点
 */
Node* bstree_predecessor(Node *x)
{
    if(x != NULL && x->left != NULL) {
        return bstree_maximum(x->left);
    }

    Node *y = x->parent;

    while(y != NULL && y->left == x) {
        x=y;
        y = y->parent;
    }

    return y;
}

/**
 * 查找x的后继节点 (比当前节点大一点,也就是右子树中，最小的点)
 * 如果x存在右孩子，则"x的后继结点"为 "以其右孩子为根的子树的最小节点"。
 * 如果x没有右孩子：
 *   x为左孩子，则为父节点
 *   x为右孩子,若该结点是其父结点的右孩子，那么需要沿着其父结点一直向树的顶端寻找，直到找到一个结点P，P结点是其父结点Q的左孩子，
 *      那么Q就是该结点的后继结点
 */
Node* bstree_successor(Node *x)
{
    if(x != NULL && x->right != NULL) {
        return bstree_minimum(x->right);
    }

    Node *y = x->parent;

    while(y != NULL && y->right == x) {
        x = y;
        y = y->parent;
    }

    return y;
}

/**
 * 创建并返回该节点
 * @param key 键值
 * @param parent 父节点
 * @param left 左孩子
 * @param right 右孩子
 * @return
 */
Node* create_bstree_node(int key, Node *parent, Node *left, Node *right)
{
    Node *p;

    if ((p = (Node *)malloc(sizeof(Node))) == NULL)
        return NULL;
    p->key = key;
    p->parent = parent;
    p->left = left;
    p->right = right;

    return p;
}

Node* insert_bstree(BSTree tree, int key)
{
    Node *z;

    if( (z = create_bstree_node(key, NULL,NULL,NULL)) == NULL)
        return tree;

    Node *x = tree;
    Node *y = NULL;

    while(x != NULL) {
        y = x;
        if(x->key > z->key){
            x = x->left;
        }  else {
            x = x->right;
        }
    }

    z->parent = y;
    if (y == NULL) {
        tree = z;
    } else if (z->key < y->key) {
        y->left = z;
    } else {
        y->right = z;
    }

    return tree;
}

//Node *insert_bstree(BSTree tree, int key)
//{
//    if(tree == NULL)// no root
//        tree = create_bstree_node(key,NULL,NULL,NULL);
//    else if(tree->key > key)
//        tree->left = insert_bstree(tree->left, key);
//    else if(tree->key < key)
//        tree->right = insert_bstree(tree->right, key);
//    return tree;
//}

/**
 在二叉查找树中删除一个给定的结点p有三种情况
(1)  结点p无左右子树，则直接删除该结点，修改父节点相应指针
(2)  结点p有左子树（右子树），则把p的左子树（右子树）接到p的父节点上
(3)  左右子树同时存在，则有三种处理方式
    a.找到结点p的中序直接前驱结点s，把结点s的数据转移到结点p，然后删除结点s，由于结点s为p的左子树中最右的结点，因而s无右子树，
        删除结点s可以归结到情况(2)。严蔚敏数据结构P230-231就是该处理方式。
    b.找到结点p的中序直接后继结点s，把结点s的数据转移到结点p，然后删除结点s，由于结点s为p的右子树总最左的结点，因而s无左子树，
        删除结点s可以归结到情况(2)。算法导论第2版P156-157该是该处理方式。
    c.找到p的中序直接前驱s，将p的左子树接到父节点上，将p的右子树接到s的右子树上，然后删除结点p。？？
 */
Node* delete_bstree(BSTree tree, int key)
{
    Node *z;

    if ((z = bstree_search(tree, key)) == NULL)
        return tree;

    Node *x=NULL;
    Node *y=NULL;

    if ((z->left == NULL) || (z->right == NULL) )
        y = z;
    else//查找后继(或者因为左右子树都存在，所以直接选左子树最大，或者右子树最小) 都行
        y = bstree_successor(z);

    if (y->left != NULL)
        x = y->left;
    else
        x = y->right;

    if (x != NULL)
        x->parent = y->parent;

    if (y->parent == NULL)
        tree = x;
    else if (y == y->parent->left)//判断是左孩子，还是右孩子
        y->parent->left = x;
    else
        y->parent->right = x;

    if (y != z)
        z->key = y->key;

    if (y!=NULL)
        free(y);

    return tree;
}


void destroy_bstree(BSTree tree)
{
    if(tree == NULL) {
        return ;
    }

    if(tree->left != NULL) {
        destroy_bstree(tree->left);
    }

    if(tree->right != NULL) {
        destroy_bstree(tree->right);
    }

    free(tree);
}

void print_bstree(BSTree tree, int key, int direction)
{
    if(tree != NULL)
    {
        if(direction==0)    // tree是根节点
            printf("%2d is root\n", tree->key);
        else                // tree是分支节点
            printf("%2d is %2d's %6s child\n", tree->key, key, direction==1?"right" : "left");

        print_bstree(tree->left, tree->key, -1);
        print_bstree(tree->right,tree->key,  1);
    }
}


/*##########################test###################################*/

static int arr[]= {1,5,4,3,2,6};
#define TBL_SIZE(a) ( (sizeof(a)) / (sizeof(a[0])) )

void main()
{
    int i, ilen;
    BSTree root=NULL;

    printf("== 依次添加: ");
    ilen = TBL_SIZE(arr);
    for(i=0; i<ilen; i++)
    {
        printf("%d ", arr[i]);
        root = insert_bstree(root, arr[i]);
    }

    printf("\n== 前序遍历: ");
    preorder_bstree(root);

    printf("\n== 中序遍历: ");
    inorder_bstree(root);

    printf("\n== 后序遍历: ");
    postorder_bstree(root);
    printf("\n");

    printf("== 最小值: %d\n", bstree_minimum(root)->key);
    printf("== 最大值: %d\n", bstree_maximum(root)->key);
    printf("== 树的详细信息: \n");
    print_bstree(root, root->key, 0);

    printf("\n== 删除根节点: %d", arr[3]);
    root = delete_bstree(root, arr[3]);

    printf("\n== 中序遍历: ");
    inorder_bstree(root);
    printf("\n");

    // 销毁二叉树
    destroy_bstree(root);
}

/*################################################################*/
