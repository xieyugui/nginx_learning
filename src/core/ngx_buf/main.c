#include <stdio.h>
#include <ngx_config.h>
#include <ngx_core.h>

int main() {
    ngx_pool_t *pool;
    pool = ngx_create_pool(1024, NULL);

    ngx_buf_t* b = ngx_pcalloc(pool, sizeof(ngx_buf_t));
    b->start = (u_char*)ngx_pcalloc(pool, 128);
    b->pos = b->start;
    b->last = b->start;
    b->end = b->last + 128;
    b->temporary = 1;
    //ngx_buf_t *b = ngx_create_temp_buf(pool, 128);
    ngx_chain_t out;
    out.buf = b;
    out.next = NULL;

//    return ngx_http_output_filter(r, &out);

    return 0;
}