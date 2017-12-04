/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_output_chain.c
* @date:      2017/12/4 下午8:20
* @desc:
*/

//
// Created by daemon.xie on 2017/12/4.
//

#include <ngx_core.h>
#include <ngx_config.h>
#include <ngx_event.h>

#if 0
#define NGX_SENDFILE_LIMIT 4096
#endif


#define NGX_NONE 1

static ngx_inline ngx_int_t
    ngx_output_chain_as_is(ngx_output_chain_ctx_t *ctx, ngx_buf_t *buf);
#if (NGX_HAVE_AIO_SENDFILE)
static ngx_int_t ngx_output_chain_aio_setup(ngx_output_chain_ctx_t *ctx, ngx_file_t *file);
#endif

static ngx_int_t ngx_output_chain_add_copy(ngx_pool_t *pool, ngx_chain_t **chain,
        ngx_chain_t *in);

static ngx_int_t ngx_output_chain_align_file_buf(ngx_output_chain_ctx_t *ctx,
                                                 off_t bsize);

static ngx_int_t ngx_output_chain_get_buf(ngx_output_chain_ctx_t *ctx, off_t bsize);

static ngx_int_t ngx_output_chain_copy_buf(ngx_output_chain_ctx_t *ctx);



