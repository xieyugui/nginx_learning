/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui
* @software:  CLion
* @file:      bstree.h
* @date:      2017/12/20 下午3:02
* @desc:
*/

//
// Created by daemon.xie on 2017/12/20.
//

#ifndef BINARY_TREE_BSTREE_H
#define BINARY_TREE_BSTREE_H

typedef struct BSTreeNode {
    int key;
    struct BSTreeNode *left;
    struct BSTreeNode *right;
    struct BSTreeNode *parent;
}Node, *BSTree;

//前序遍历二叉树
void preorder_bstree(BSTree tree);

//中序遍历
void inorder_bstree(BSTree tree);

//后序遍历
void postorder_bstree(BSTree tree);

//查找为key 的节点
Node* bstree_search(BSTree tree, int key);
//非递归查找
Node* iterative_bstree_search(BSTree tree, int key);

//查找最小节点
Node* bstree_minimum(BSTree tree);
//查找最大节点
Node* bstree_maximum(BSTree tree);

// 找结点(x)的前驱结点。即，查找"二叉树中数据值小于该结点"的"最大结点"。
Node* bstree_predecessor(Node *x);

// 找结点(x)的后继结点。即，查找"二叉树中数据值大于该结点"的"最小结点"。
Node* bstree_successor(Node *x);

//创建并返回该节点
Node* create_bstree_node(int key, Node *parent, Node *left, Node *right);

//插入节点
Node* insert_bstree(BSTree tree, int key);

// 删除结点(key为节点的值)，并返回根节点
Node* delete_bstree(BSTree tree, int key);

//销毁二叉树
void destroy_bstree(BSTree tree);

/*
 * 打印"二叉树"
 *
 * tree       -- 二叉树的节点
 * key        -- 节点的键值
 * direction  --  0，表示该节点是根节点;
 *               -1，表示该节点是它的父结点的左孩子;
 *                1，表示该节点是它的父结点的右孩子。
 */
void print_bstree(BSTree tree, int key, int direction);

#endif //BINARY_TREE_BSTREE_H
