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

//目的是发送 in 中的数据，ctx 用来保存发送的上下文，因为发送通常情况下，不能一次完成
ngx_int_t
ngx_output_chain(ngx_output_chain_ctx_t *ctx, ngx_chain_t *in)
{
    off_t bsize;
    ngx_int_t rc, last;
    ngx_chain_t *cl, *out, **last_out;

    if(ctx->in == NULL && ctx->busy == NULL)
    {
        /*
         *当ctx-> in和ctx-> busy链的情况下的chain
         *为空，传入的链也是空的或有单个buf不需要复制
         * 也就是说我们能直接确定所有的in chain都不需要复制的时候，
         * 我们就可以直接调用output_filter来交给剩下的filter去处理
         */
        if (in == NULL) {
            return ctx->output_filter(ctx->filter_ctx, in);
        }
        //如果in 为最后一块数据，且不需要复制，则直接调用output_filter
        if (in->next == NULL && ngx_output_chain_as_is(ctx, in->buf))
        {
            return ctx->output_filter(ctx->filter_ctx, in);
        }
    }

    //如果有数据的话，就加入到ctx->in 的最后
    if (in) {
        if(ngx_output_chain_add_copy(ctx->pool, &ctx->in, in) == NGX_ERROR) {
            return NGX_ERROR;
        }
    }

    //out为我们最终需要传输的chain，也就是交给剩下的filter处理的chain
    out = NULL;
    //last_out为out的最后一个chain
    last_out = &out;
    last = NGX_NONE;

    for (;;) {
        while (ctx->in) {
            //取得当前chain的buf大小
            bsize = ngx_buf_size(ctx->in->buf);
            //跳过bsize为0的buf
            if(bsize == 0 && !ngx_buf_special(ctx->in->buf)) {
                ctx->in = ctx->in->next;

                continue;
            }
            //判断是否需要复制buf
            if (ngx_output_chain_as_is(ctx, ctx->in->buf)) {
                //如果不需要复制，则直接链接chain到out，然后继续循环
                cl = ctx->in;
                ctx->in = cl->next;

                *last_out = cl;
                last_out = &cl->next;
                cl->next = NULL;

                continue;
            }
            //到达这里，说明我们需要拷贝buf，这里buf最终都会被拷贝进ctx->buf中，
            //因此这里先判断ctx->buf是否为空
            if (ctx->buf == NULL) {
                //如果为空，则取得buf，这里要注意，一般来说如果没有开启directio的话，这个函数都会返回NGX_DECLINED的
                //DirectIO是write函数的一个选项，用来确定数据内容直接写到磁盘上，而非缓存中，保证即是系统异常了，也能保证紧要数据写到磁盘上
                rc = ngx_output_chain_align_file_buf(ctx, bszie);
                if(rc == NGX_ERROR) {
                    return NGX_ERROR;
                }

                if (rc != NGX_OK) {
                    //准备分配buf，首先在free中寻找可以重用的buf
                    if (ctx->free) {
                        //得到free bu
                        cl = ctx->free;
                        ctx->buf = cl->buf;
                        ctx->free = cl->next;
                        //将要重用的chain链接到ctx->poll中，以便于chain的重用.
                        ngx_free_chain(ctx->pool, cl);
                    } else if (out || ctx->allocated == ctx->bufs.num) {
                        //如果已经等于buf的个数限制，则跳出循环，发送已经存在的buf.这里可以看到如果out存在的话，
                        // nginx会跳出循环，然后发送out，等发送完会再次处理，这里很好的体现了nginx的流式处理
                        break;
                    } else if (ngx_output_chain_get_buf(ctx, bsize)) { //当没有可重用的buf的时候，用来分配buf的
                        return NGX_ERROR;
                    }
                }
            }
            //复制buf
            rc = ngx_output_chain_copy_buf(ctx);

            if(rc == NGX_ERROR) {
                return rc;
            }

            if(rc == NGX_AGAIN) {
                if(out) {
                    break;
                }

                return rc;
            }

            if(ngx_buf_size(ctx->in->buf) == 0) {
                ctx->in = ctx->in->next;
            }
            //创建一个缓冲区的链表结构
            cl = ngx_alloc_chain_link(ctx->pool);
            if (cl == NULL) {
                return NGX_ERROR;
            }

            //链接chain到out
            cl->buf = ctx->buf;
            cl->next = NULL;
            *last_out = cl;
            last_out = &cl->next;
            ctx->buf = NULL;
        }

        if (out == NULL && last != NGX_NONE) {

            if (ctx->in) {
                return NGX_AGAIN;
            }

            return last;
        }

        last = ctx->output_filter(ctx->filter_ctx, out);
        if (last == NGX_ERROR || last == NGX_DONE) {
            return last;
        }
        //update chain，这里主要是将处理完毕的chain放入到free，没有处理完毕的放到busy中
        ngx_chain_update_chains(ctx->pool, &ctx->free, &ctx->busy, &out,
                                ctx->tag);
        last_out = &out;
    }
}

//主要用来判断是否需要复制buf。返回1,表示不需要拷贝，否则为需要拷贝
static ngx_inline ngx_int_t
ngx_output_chain_as_is(ngx_output_chain_ctx_t *ctx, ngx_buf_t *buf)
{
    ngx_uint_t sendfile;
    //是否为specialbuf，是的话返回1,也就是不用拷贝
    if(ngx_buf_special(buf)) {
        return 1;
    }
    //如果buf在文件中，并且使用了directio的话，需要拷贝buf
    if (buf->in_file && buf->file->directio) {
        return 0;
    }
    //sendfile标记
    sendfile = ctx->sendfile;

    if(!sendfile) {
        if(!ngx_buf_in_memory(buf)) {
            return 0;
        }

        buf->in_file = 0;
    }
    //如果需要内存中有一份拷贝，而并不在内存中，此时返回0，表示需要拷贝
    if (ctx->need_in_memory && !ngx_buf_in_memory(buf)) {
        return 0;
    }

    //如果需要内存中有拷贝,并且存在于内存中或者mmap中，则返回0
    if (ctx->need_in_temp && (buf->memory || buf->mmap)) {
        return 0;
    }

    return 1;
}

//复制in到chain的结尾
static ngx_int_t
ngx_output_chain_add_copy(ngx_pool_t *pool, ngx_chain_t **chain, ngx_chain_t *in)
{
    ngx_chain_t *cl, **ll;
#if (NGX_SENDFILE_LIMIT)
    ngx_buf_t    *b, *buf;
#endif

    ll = chain;
    //遍历到链表最后
    for (cl = *chain; cl; cl = cl->next) {
        ll = &cl->next;
    }

    while (in) {
        //创建一个缓冲区的链表结构
        cl = ngx_alloc_chain_link(pool);
        if (cl == NULL) {
            return NGX_ERROR;
        }

#if (NGX_SENDFILE_LIMIT)

#else
        cl->buf = in->buf;
        in = in->next;
#endif
        cl->next = NULL;
        *ll = cl;
        ll = &cl->next;
    }

    return NGX_OK;
}

static ngx_int_t
ngx_output_chain_align_file_buf(ngx_output_chain_ctx_t *ctx, off_t bsize)
{//调用该函数的前提是需要重新分配空间，ngx_output_chain_as_is返回0
    size_t size;
    ngx_buf_t *in;

    in = ctx->in->buf;

    if (in->file == NULL || !in->file->directio) {
        //如果没有启用direction,则直接返回，实际空间在该函数外层ngx_output_chain_get_buf中创建
        return NGX_DECLINED;
    }

    /* 数据在文件里面，并且程序有走到了 b->file->directio = of.is_directio;这几个模块，
        并且文件大小大于directio xxx中的大小 */
    ctx->directio = 1;

    size = (size_t) (in->file_pos - (in->file_pos & ~(ctx->alignment - 1)));

    if (size == 0) {

        if(bsize >= (off_t) ctx->bufs.size) {
            return NGX_DECLINED;
        }

        size = (size_t) bsize;
    } else {
        size = (size_t) ctx->alignment - size;

        if ((off_t) size > bsize) {
            size = (size_t) bsize;
        }
    }

    ctx->buf = ngx_create_temp_buf(ctx->pool, size);
    if (ctx->buf == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;

}

/*
 * ngx_output_chain_get_buf,这个函数是当没有可重用的buf的时候，用来分配buf的。

这里只有一个要注意的，那就是如果当前的buf是位于最后一个chain的话，会有特殊处理。这里特殊处理有两个地方，一个是buf的recycled域，一个是将要分配的buf的大小。

先来说recycled域，这个域表示我们当前的buf是需要被回收的。而我们知道nginx一般情况下(比如非last buf)是会缓存一部分buf，然后再发送的(默认是1460字节)，而设置了recycled的话，我们就不会让它缓存buf，也就是尽量发送出去，然后以供我们回收使用。

因此如果是最后一个buf的话，一般来说我们是不需要设置recycled域的，否则的话，需要设置recycled域。因为不是最后一个buf的话，我们可能还会需要重用一些buf，而buf只有被发送出去的话，我们才能重用。

然后就是size的大小。这里会有两个大小，一个是我们需要复制的buf的大小，一个是nginx.conf中设置的size。如果不是最后一个buf，则我们只需要分配我们设置的buf的size大小就行了。如果是最后一个buf，则就处理不太一样
 */
static ngx_int_t
ngx_output_chain_get_buf(ngx_output_chain_ctx_t *ctx, off_t bsize)
{
    size_t size;
    ngx_buf_t *b, *in;
    ngx_uint_t recycled;

    in = ctx->in->buf;
    //可以看到这里分配的buf，每个的大小都是我们在nginx.conf中设置的size
    size = ctx->bufs.size;
    recycled = 1;
    //如果当前的buf是属于最后一个chain的时候。这里我们要特殊处理。
    if (in->last_in_chain) {//表示为缓冲区链表ngx_chain_t上的最后一块缓冲区

        if (bsize < (off_t) size) {

            /*
             * allocate a small temp buf for a small last buf
             * or its small last part
             */

            size = (size_t) bsize;
            recycled = 0;

        } else if (!ctx->directio
                   && ctx->bufs.num == 1
                   && (bsize < (off_t) (size + size / 4)))
        {
            /*
             * allocate a temp buf that equals to a last buf,
             * if there is no directio, the last buf size is lesser
             * than 1.25 of bufs.size and the temp buf is single
             */

            size = (size_t) bsize;
            recycled = 0;
        }
    }
    //开始分配buf内存.
    b = ngx_calloc_buf(ctx->pool);
    if (b == NULL) {
        return NGX_ERROR;
    }

    //在该函数外层的前面ngx_output_chain_align_file_buf会置directio为1
    if (ctx->directio) {
        //directio需要对齐
        /*
         * allocate block aligned to a disk sector size to enable
         * userland buffer direct usage conjunctly with directio
         */

        b->start = ngx_pmemalign(ctx->pool, size, (size_t) ctx->alignment);
        if (b->start == NULL) {
            return NGX_ERROR;
        }

    } else {
        b->start = ngx_palloc(ctx->pool, size);
        if (b->start == NULL) {
            return NGX_ERROR;
        }
    }

    b->pos = b->start;
    b->last = b->start;
    b->end = b->last + size;
    b->temporary = 1;
    b->tag = ctx->tag;
    b->recycled = recycled;

    ctx->buf = b;//该函数获取到的内存保存到ctx->buf中
    ctx->allocated++;

    return NGX_OK;

}

//? 后面再看
static ngx_int_t
ngx_output_chain_copy_buf(ngx_output_chain_ctx_t *ctx)
{//将ctx->in->buf的缓冲拷贝到ctx->buf上面去。  注意是从新分配了数据空间，用来存储原来的in->buf中的数据，实际上现在就有两份相同的数据了
// (buf指向相同的内存空间)
    off_t size;
    //signed size_t
    ssize_t n;
    ngx_buf_t   *src, *dst;
    ngx_uint_t   sendfile;

    src = ctx->in->buf;
    dst = ctx->buf;

    size = ngx_buf_size(src);
    size = ngx_min(size, dst->end - dst->pos);

    sendfile = ctx->sendfile && !ctx->directio;

#if (NGX_SENDFILE_LIMIT)

    if (src->in_file && src->file_pos >= NGX_SENDFILE_LIMIT) {
        sendfile = 0;
    }

#endif

    if (ngx_buf_in_memory(src)) {
        ngx_memcpy(dst->pos, src->pos, (size_t) size);
        src->pos += (size_t) size;
        dst->last += (size_t) size;

        if (src->in_file) {

            if (sendfile) {
                dst->in_file = 1;
                dst->file = src->file;
                dst->file_pos = src->file_pos;
                dst->file_last = src->file_pos + size;

            } else {
                dst->in_file = 0;
            }

            src->file_pos += size;

        } else {
            dst->in_file = 0;
        }

        if (src->pos == src->last) {
            dst->flush = src->flush;
            dst->last_buf = src->last_buf;
            dst->last_in_chain = src->last_in_chain;
        }

    } else {

#if (NGX_HAVE_ALIGNED_DIRECTIO)

        if (ctx->unaligned) {
            if (ngx_directio_off(src->file->fd) == NGX_FILE_ERROR) {
                ngx_log_error(NGX_LOG_ALERT, ctx->pool->log, ngx_errno,
                              ngx_directio_off_n " \"%s\" failed",
                              src->file->name.data);
            }
        }

#endif

#if (NGX_HAVE_FILE_AIO)
        if (ctx->aio_handler) {
            n = ngx_file_aio_read(src->file, dst->pos, (size_t) size,
                                  src->file_pos, ctx->pool);
            if (n == NGX_AGAIN) {
                ctx->aio_handler(ctx, src->file);
                return NGX_AGAIN;
            }

        } else
#endif
#if (NGX_THREADS)
        if (ctx->thread_handler) {
            src->file->thread_task = ctx->thread_task;
            src->file->thread_handler = ctx->thread_handler;
            src->file->thread_ctx = ctx->filter_ctx;

            n = ngx_thread_read(src->file, dst->pos, (size_t) size,
                                src->file_pos, ctx->pool);
            if (n == NGX_AGAIN) {
                ctx->thread_task = src->file->thread_task;
                return NGX_AGAIN;
            }

        } else
#endif
        {
            n = ngx_read_file(src->file, dst->pos, (size_t) size,
                              src->file_pos);
        }

#if (NGX_HAVE_ALIGNED_DIRECTIO)

        if (ctx->unaligned) {
            ngx_err_t  err;

            err = ngx_errno;

            if (ngx_directio_on(src->file->fd) == NGX_FILE_ERROR) {
                ngx_log_error(NGX_LOG_ALERT, ctx->pool->log, ngx_errno,
                              ngx_directio_on_n " \"%s\" failed",
                              src->file->name.data);
            }

            ngx_set_errno(err);

            ctx->unaligned = 0;
        }

#endif

        if (n == NGX_ERROR) {
            return (ngx_int_t) n;
        }

        if (n != size) {
            ngx_log_error(NGX_LOG_ALERT, ctx->pool->log, 0,
                          ngx_read_file_n " read only %z of %O from \"%s\"",
                          n, size, src->file->name.data);
            return NGX_ERROR;
        }

        dst->last += n;

        if (sendfile) {
            dst->in_file = 1;
            dst->file = src->file;
            dst->file_pos = src->file_pos;
            dst->file_last = src->file_pos + n;

        } else {
            dst->in_file = 0;
        }

        src->file_pos += n;

        if (src->file_pos == src->file_last) {
            dst->flush = src->flush;
            dst->last_buf = src->last_buf;
            dst->last_in_chain = src->last_in_chain;
        }
    }

    return NGX_OK;

}

//向后端发送请求的调用过程ngx_http_upstream_send_request_body->ngx_output_chain->ngx_chain_writer
ngx_int_t
ngx_chain_writer(void *data, ngx_chain_t *in)
{
    ngx_chain_writer_ctx_t *ctx = data;

    off_t size;
    ngx_chain_t *cl,*ln,*chain;
    ngx_connection_t *c;

    c = ctx->connection;
    /*下面的循环，将in里面的每一个链接节点，添加到ctx->filter_ctx所指的链表中。并记录这些in的链表的大小。*/
    for (size = 0; in; in->next) {
        size += ngx_buf_size(in->buf);

        cl = ngx_alloc_chain_link(ctx->pool);
        if (cl == NULL) {
            return NGX_ERROR;
        }

        cl->buf = in->buf;
        cl->next = NULL;
        *ctx->last = cl;
        ctx->last = &cl->next;
    }

    for (cl = ctx->out; cl; cl = cl->next) {
        size += ngx_buf_size(cl->buf);
    }

    if (size == 0 && !c->buffered) {//啥数据都么有，不用发了都
        return NGX_OK;
    }
    //调用writev将ctx->out的数据全部发送出去。如果没法送完，则返回没发送完毕的部分。记录到out里面
    //在ngx_event_connect_peer连接上游服务器的时候设置的发送链接函数ngx_send_chain=ngx_writev_chain。
    //ngx_send_chain->ngx_writev_chain  到后端的请求报文是不会走filter过滤模块的，
    // 而是直接调用ngx_writev_chain->ngx_writev发送到后端
    chain = c->send_chain(c, ctx->out, ctx->limit);

    if (chain == NGX_CHAIN_ERROR) {
        return NGX_ERROR;
    }
    //把ctx->out中已经全部发送出去的in节点从out链表摘除放入free中，重复利用
    for (cl = ctx->out; cl && cl != chain; /* void */) {
        ln = cl;
        cl = cl->next;
        ngx_free_chain(ctx->pool, ln);
    }

    //ctx->out上面现在只剩下还没有发送出去的in节点了
    ctx->out = chain;

    if (ctx->out == NULL) { //说明已经ctx->out链中的所有数据已经全部发送完成
        ctx->last = &ctx->out;

        if (!c->buffered) {
            return NGX_OK;
        }
    }
    //如果上面的chain = c->send_chain(c, ctx->out, ctx->limit)后，out中还有数据则返回NGX_AGAIN等待再次事件触发调度
    return NGX_AGAIN;
}







