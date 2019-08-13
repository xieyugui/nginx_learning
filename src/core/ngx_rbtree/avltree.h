/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui
* @software:  CLion
* @file:      avltree.h
* @date:      2017/12/21 上午11:20
* @desc:
 *
 * https://blog.csdn.net/hm108106/article/details/72736075 比较详细
*/

//
// Created by daemon.xie on 2017/12/21.
//

#ifndef BINARY_TREE_AVLTREE_H
#define BINARY_TREE_AVLTREE_H
//// AVL树中任何节点的两个子树的高度最大差别为1
typedef struct AVLTreeNode {
    int key;
    int height;
    struct AVLTreeNode *left;
    struct AVLTreeNode *right;
}Node, *AVLTree;

//获取avl树的高度
int avltree_height(AVLTree tree);

//前序遍历AVL树
void preorder_avltree(AVLTree tree);

//中序遍历AVL树
void inorder_avltree(AVLTree tree);

//后序遍历AVL树
void postorder_avltree(AVLTree tree);

void print_avltree(AVLTree tree, int key, int direction);

//（递归）查找AVL树x中键值为key的节点
Node* avltree_search(AVLTree x, int key);

// (非递归实现)查找"AVL树x"中键值为key的节点
Node* iterative_avltree_search(AVLTree x, int key);

//查找最小节点：返回tree为根节点的avl树的最小节点
Node* avltree_minimum(AVLTree tree);

//查找最大节点
Node* avltree_maximum(AVLTree tree);

/*
 * LL：左左对应的情况(右单旋转)。
 *
 * 返回值：旋转后的根节点
 */
Node* left_left_rotation(AVLTree k2);

/*
 * RR：右右对应的情况(左单旋转)。
 *
 * 返回值：旋转后的根节点
 */
Node* right_right_rotation(AVLTree k1);

/*
 * LR：左右对应的情况(左旋+右旋转)。
 *
 * 返回值：旋转后的根节点
 */
Node* left_right_rotation(AVLTree k3);

/*
 * RL：右左对应的情况(右旋+左旋转)。
 *
 * 返回值：旋转后的根节点
 */
Node* right_left_rotation(AVLTree k1);

/*
 * 创建AVL树结点。
 *
 * 参数说明：
 *     key 是键值。
 *     left 是左孩子。
 *     right 是右孩子。
 */
Node* avltree_create_node(int key, Node *left, Node* right);

//将节点插入avl树中，返回根节点
Node* avltree_insert(AVLTree tree, int key);

//删除节点，返回根节点
Node* delete_node(AVLTree tree, Node *z);
Node* avltree_delete(AVLTree tree, int key);

//销毁AVL树
void destroy_avltree(AVLTree tree);


#endif //BINARY_TREE_AVLTREE_H
