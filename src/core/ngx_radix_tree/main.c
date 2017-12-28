#include <stdio.h>
#include <stdlib.h>
#include <ngx_core.h>

void travel_radix_tree(ngx_radix_node_t *root)
{
    if (root->left != NULL)
    {
        travel_radix_tree(root->left);
    }

    if (root->value != NGX_RADIX_NO_VALUE)
    {
        ngx_int_t* value = root->value;
        printf("%d\n", *value);
    }

    if (root->right != NULL)
    {
        travel_radix_tree(root->right);
    }
}

int main() {

    ngx_int_t data[64];
    ngx_int_t i;
    for(i = 0; i < 64; i++)
    {
        data[i] = rand()%10000;
    }

    ngx_pool_t *pool = ngx_create_pool(1024, NULL);
    if (pool == NULL) {
        printf("create pool failed!\n");
        exit(1);
    }

    ngx_pagesize = getpagesize();
    printf("pagesize = %d\n", ngx_pagesize);

    /*创建基数树，prealloc=0时，只创建结构体ngx_radix_tree_t,没有创建任何基数树节点*/
    ngx_radix_tree_t *tree = ngx_radix_tree_create(pool, -1);
    if(tree == NULL) {
        printf("create radix tree failed!\n");
        exit(1);
    }

    ngx_uint_t deep = 5; //树的最大深度为4
    ngx_uint_t mask = 0;
    ngx_uint_t inc  = 0x80000000;
    ngx_uint_t key = 0;
    ngx_uint_t cunt = 0;

    while (deep--)
    {
        key    = 0;
        mask >>= 1;
        mask  |= 0x80000000;
        do
        {
            if (NGX_OK != ngx_radix32tree_insert(tree, key, mask, &data[cunt]))
            {
                printf("insert error\n");
                exit(1);
            }

            key += inc;

            ++cunt;
            if (cunt > 63)
            {
                cunt = 63;
            }
        }while(key);

        inc >>= 1;
    }

    /*先序打印数据*/
    travel_radix_tree(tree->root);
    printf("\n");

    //查找
    ngx_uint_t tkey  = 0x58000000;
    ngx_int_t* value = ngx_radix32tree_find(tree, 0x58000000);
    if (value != NGX_RADIX_NO_VALUE)
    {
        printf("find the value: %d with the key = %x\n", *value, tkey);
    }
    else
    {
        printf("not find the the value with the key = %x\n", tkey);
    }

    /*删除测试*/
    if (NGX_OK == ngx_radix32tree_delete(tree, tkey, mask))
    {
        printf("delete the the value with the key = %x is succeed\n", tkey);
    }
    else
    {
        printf("delete the the value with the key = %x is failed\n", tkey);
    }

    value = ngx_radix32tree_find(tree, 0x58000000);
    if (value != NGX_RADIX_NO_VALUE)
    {
        printf("find the value: %d with the key = %x\n", *value, tkey);
    }
    else
    {
        printf("not find the the value with the key = %x\n", tkey);
    }

    return 0;
}