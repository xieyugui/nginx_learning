#include <stdio.h>
#include <stdlib.h>

/**
 * 简单的二叉树
 */

typedef struct node{
    char data;
    struct node *left, *right;
}Node;

Node *create_binary_tree() {
    char ch;
    scanf("%c", &ch);
    Node *cnode;
    if (ch == '#') {
        cnode =  NULL;
    } else {
        cnode = (Node *)malloc(sizeof(Node));
        cnode->data = ch;
        printf("%c\n", cnode->data);
        cnode->left = create_binary_tree();
        cnode->right = create_binary_tree();
    }
    return cnode;
}

void print_preface(Node *root) {
    if(root) {
        printf("%c ", root->data);
        print_preface(root->left);
        print_preface(root->right);
    }
}

void print_order(Node *root) {
    if(root) {
        print_order(root->left);
        printf("%c ", root->data);
        print_order(root->right);
    }
}

void print_after(Node *root) {
    if(root) {
        print_after(root->left);
        print_after(root->right);
        printf("%c ", root->data);
    }
}

int main() {

    Node *root;
    printf("创建二叉树\n");
    //ABD##E##CF##G##
    root = create_binary_tree();

    printf("前序遍历\n");
    print_preface(root);
    printf("\n");

    printf("中序遍历\n");
    print_order(root);
    printf("\n");

    printf("后序遍历\n");
    print_after(root);

    return 0;
}