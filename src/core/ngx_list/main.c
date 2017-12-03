#include <stdio.h>
#include <ngx_core.h>
#include <ngx_config.h>
#include <ngx_string.h>

typedef struct {
    size_t len;
    u_char *data;
} ngx_str_t;

#define N 5

int main() {
    ngx_pool_t *pool;
    int i;
    char str[] = "hello nginx!";
    ngx_list_t *l;

    pool = ngx_create_pool(1024, NULL);
    l = ngx_list_create(pool, N, sizeof(ngx_str_t));

    for(i = 0; i < 10; i++) {
        ngx_str_t *pstr = ngx_list_push(l);
        char *buf = ngx_palloc(pool, 6 * N);
        sprintf(buf, "My Id is %d,%s", i+1, str);

        pstr->len = strlen(buf);
        pstr->data = buf;

    }
    ngx_destroy_pool(pool);

    return 0;
}