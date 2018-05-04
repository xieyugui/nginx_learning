/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_buf.h
* @date:      2017/12/4 上午11:04
* @desc:
*/

//
// Created by daemon.xie on 2017/12/4.
//

#ifndef NGX_BUF_NGX_BUF_H
#define NGX_BUF_NGX_BUF_H

#include <ngx_config.h>
#include <ngx_core.h>

typedef void * ngx_buf_tag_t;
typedef struct ngx_buf_s ngx_buf_t;

struct ngx_buf_s {
    u_char *pos; /* 待处理数据的开始标记  */
    u_char *last;  /* 待处理数据的结尾标记 */
    off_t file_pos; /* 处理文件时，待处理的文件开始标记  */
    off_t file_last; /* 处理文件时，待处理的文件结尾标记  */

    u_char *start; /* 缓冲区开始的指针地址 */
    u_char  *end; /* 缓冲区结尾的指针地址 */
    ngx_buf_tag_t tag; /* 缓冲区标记地址，是一个void类型的指针 */
    ngx_file_t *file; /* 引用的文件 */
    ngx_buf_t *shadow;

    //the buf's content could be changed
    unsigned temporary:1; /* 标志位，为1时，√√ */

    unsigned  memory:1; /* 标志位，为1时，内存只读 */

    unsigned mmap:1; /* 标志位，为1时，mmap映射过来的内存，不可修改 */

    unsigned recycled:1; /* 标志位，为1时，可回收 */
    unsigned in_file:1; /* 标志位，为1时，表示处理的是文件 */
    unsigned flush:1; /* 标志位，为1时，表示需要进行flush操作 */
    unsigned sync:1;  /* 标志位，为1时，表示可以进行同步操作，容易引起堵塞 */
    unsigned last_buf:1; /* 标志位，为1时，表示为缓冲区链表ngx_chain_t上的最后一块待处理缓冲区 */
    unsigned last_in_chain:1; /* 标志位，为1时，表示为缓冲区链表ngx_chain_t上的最后一块缓冲区 */

    unsigned last_shadow:1; /* 标志位，为1时，表示是否是最后一个影子缓冲区 */
    unsigned temp_file:1; /* 标志位，为1时，表示当前缓冲区是否属于临时文件 */

    int num;
};

/**
 * 缓冲区链表结构，放在pool内存池上面
 */
struct ngx_chain_s {
    ngx_buf_t *buf; //指向当前ngx_buf_t缓冲区
    ngx_chain_t *next; //指向下一个ngx_chain_t结构
};

typedef struct {
    ngx_int_t num; //缓冲区个数
    size_t size; //缓冲区大小
} ngx_bufs_t;

typedef struct ngx_output_chain_ctx_s ngx_output_chain_ctx_t;

typedef ngx_int_t (*ngx_output_chain_filter_pt)(void *ctx, ngx_chain_t *in);

typedef void (*ngx_output_chain_aio_pt)(ngx_output_chain_ctx_t *ctx, ngx_file_t *file);

//模块上下文（context）,context结构
struct ngx_output_chain_ctx_s {
    ngx_buf_t *buf; /* 保存临时的buf */
    ngx_chain_t *in; /* 保存了将要发送的chain */
    ngx_chain_t *free; /* 保存了已经发送完毕的chain，以便于重复利用 */
    ngx_chain_t *busy; /* 保存了还未发送的chain */

    unsigned sendfile:1; /* sendfile标记 */
    unsigned directio:1; /* directio标记 */
    unsigned unaligned:1;
    /* 是否需要在内存中保存一份(使用sendfile的话， 内存中没有文件的拷贝的，而我们有时需要处理文件，
                                                      此时就需要设置这个标记) */
    unsigned need_in_memory:1;
    /* 是否需要在内存中重新复制一份，不管buf是在内存还是文件,这样的话，后续模块可以直接修改这块内存 */
    unsigned need_in_temp:1;
    unsigned aio:1;

#if (NGX_HAVE_FILE_AIO || NGX_COMPAT)
    ngx_output_chain_aio_pt aio_handler;
#if (NGX_HAVE_AIO_SENDFILE || NGX_COMPAT)
    ssize_t (*aio_preload)(ngx_buf_t *file);
#endif
#endif

#if (NGX_THREADS || NGX_COMPAT)
    ngx_int_t (*thread_handler)(ngx_thread_task_t *task, ngx_file_t *file);

    ngx_thread_task_t *thread_task;
#endif
    off_t alignment;

    ngx_pool_t *pool;
    ngx_int_t allocated; /* 已经分配的buf个数 */
    ngx_bufs_t bufs; /* 对应loc conf中设置的bufs */
    //表示现在处于哪个模块（因为upstream也会调用output_chain)
    ngx_buf_tag_t tag;  /* 模块标记，主要用于buf回收 */
    /* 一般是ngx_http_next_filter,也就是继续调用filter链 */
    ngx_output_chain_filter_pt output_filter;
    /* 当前filter的上下文，这里是由于upstream也会调用output_chain */
    void *filter_ctx;

};

typedef struct {
    //ngx_http_upstream_connect中初始化赋值u->writer.last = &u->writer.out; last指针指向out，
    // 调用ngx_chain_writer后last指向存储在out中cl的最后一个节点的NEXT处
    ngx_chain_t *out; //还没有发送出去的待发送数据的头部
    //ngx_http_upstream_connect中初始化赋值u->writer.last = &u->writer.out; last指针指向out，
    // 调用ngx_chain_writer后last指向存储在out中cl的最后一个节点的NEXT处
    ngx_chain_t **last;//永远指向最后一个ngx_chain_t的next字段的地址。这样可以通过这个地址不断的在后面增加元素。
    ngx_connection_t *connection;//这个输出链表对应的连接
    ngx_pool_t *pool; //等于request对应的pool，见ngx_http_upstream_init_request
    off_t limit;
} ngx_chain_writer_ctx_t;

#define NGX_CHAIN_ERROR (ngx_chain_t *) NGX_ERROR

//是否在内存中
#define ngx_buf_in_memory(b) (b->temporary || b->memory || b->mmap)
//是否在内存中，且不是文件
#define ngx_buf_in_memory_only(b) (ngx_buf_in_memory(b) && !b->in_file)

//返回该buf是否是一个特殊的buf，只含有特殊的标志和没有包含真正的数据
#define ngx_buf_special(b) \
    ((b->flush || b->last_buf || b->sync) && !ngx_buf_in_memory(b) && !b->in_file)

//返回该buf是否是一个只包含sync标志而不包含真正数据的特殊buf
#defube ngx_buf_sync_only(b) \
    (b->sync && !ngx_buf_in_memory(b) && !b->in_file && !->flush && !b->last_buf)

//返回该buf所含数据的大小，不管这个数据是在文件里还是在内存里
#define ngx_buf_size(b) \
    (ngx_buf_memeory(b) ? (off_t) (b->last - b->pos):(b->file_last - b->file_pos))


/**
 * 创建一个缓冲区。需要传入pool和buf的大小
 * ngx_create_temp_buf直接从pool上创建一个缓冲区的buf，buf大小可以自定义。
 *  buf的数据结构和buf内存块都会被创建到pool内存池上
 */
ngx_buf_t *ngx_create_temp_buf(ngx_pool_t *pool, size_t size);

/**
 * 批量创建多个buf，并且用链表串起来
 */
ngx_chain_t *ngx_create_chain_of_bufs(ngx_pool_t *pool, ngx_bufs_t *bufs);

#define ngx_alloc_buf(pool) ngx_palloc(pool, sizeof(ngx_buf_t))
#define ngx_calloc_buf(pool) ngx_calloc(pool, sizeof(ngx_buf_t))

/**
 * 创建一个缓冲区的链表结构
 */
ngx_chain_t *ngx_alloc_chain_link(ngx_pool_t *pool);

//直接交还给缓存池  将释放之后的cl链接到pool->chain中
#define ngx_free_chain(pool, cl) cl->next = pool->chain; pool->chain = cl;

ngx_int_t ngx_output_chain(ngx_output_chain_ctx_t *ctx, ngx_chain_t *in);
ngx_int_t ngx_chain_writer(void *ctx, ngx_chain_t *in);

/**
 * 将其它缓冲区链表放到已有缓冲区链表结构的尾部
 */
ngx_int_t ngx_chain_add_copy(ngx_pool_t *pool, ngx_chain_t **chain, ngx_chain_t *in);

/**
 * 在空闲的buf链表上，获取一个未使用的buf链表
 */
ngx_chain_t *ngx_chain_get_free_buf(ngx_pool_t *p, ngx_chain_t **free);

/**
 * 释放BUF
 * 1. 如果buf不为空，则不释放
 * 2. 如果cl->buf->tag标记不一样，则直接还给Nginx的pool->chain链表
 * 3. 如果buf为空，并且需要释放，则直接释放buf，并且放到free的空闲列表上
 */
void ngx_chain_update_chains(ngx_pool_t *p, ngx_chain_t **free, ngx_chain_t **busy,
        ngx_chain_t **out, ngx_buf_tag_t tag);

off_t ngx_chain_coalesce_file(ngx_chain_t **in, off_t limit);

//计算本次调用ngx_writev发送出去的send字节在in链表中所有数据的那个位置
ngx_chain_t *ngx_chain_update_sent(ngx_chain_t *in, off_t sent);

#endif //NGX_BUF_NGX_BUF_H
