#include <stdio.h>
#include <ngx_core.h>
#include <ngx_config.h>

#define N 5

typedef struct node
{
    int id;
    char buf[32];
}Node;

int main() {
    ngx_pool_t *pool;
    ngx_array_t *a;
    Node *ptmp;
    int i;

    pool = ngx_create_pool(1024, NULL);

    a = ngx_array_create(pool, N, sizeof(Node));

    for (i = 0; i<8; i++) {
        ptmp = ngx_array_push(a);
        ptmp->id = i + 1;
        sprintf(ptmp->buf, "My Id is %d", ptmp->id);
    }
    ptmp = ngx_array_push_n(a, 2);// 添加两个元素
    ptmp->id = i+1;
    sprintf(ptmp->buf, "My Id is %d", ptmp->id);
    ++ptmp;
    ptmp->id = i+2;
    sprintf(ptmp->buf, "My Id is %d", ptmp->id);

    ngx_array_destroy(a);
    ngx_destroy_pool(pool);

    return 0;
}