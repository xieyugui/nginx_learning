/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui
* @software:  CLion
* @file:      ngx_buf.c
* @date:      2017/12/4 上午11:04
* @desc:
*/

//
// Created by daemon.xie on 2017/12/4.
//

#include <ngx_config.h>
#include <ngx_core.h>

/**
 * 创建一个缓冲区。需要传入pool和buf的大小
 */
ngx_buf_t *
ngx_create_temp_buf(ngx_pool_t *pool, size_t size)
{
    ngx_buf_t *b;
    /* 最终调用的是内存池pool，开辟一段内存用作缓冲区，主要放置ngx_buf_t结构体 */
    b = ngx_calloc_buf(pool);
    if(b == NULL) {
        return NULL;
    }

    /* 分配缓冲区内存;  pool为内存池，size为buf的大小*/
    b->start = ngx_palloc(pool, size);
    if (b->start == NULL) {
        return NULL;
    }

    b->pos = b->start;
    b->last = b->start;
    b->end = b->last + size;
    b->temporary = 1; //内存可以修改

    return b;
}

/**
 * 创建一个缓冲区的链表结构
 */
ngx_chain_t *
ngx_alloc_chain_link(ngx_pool_t *pool)
{
    ngx_chain_t *cl;
    /*
     * 首先从内存池中去取ngx_chain_t，
     * 被清空的ngx_chain_t结构都会放在pool->chain 缓冲链上
     */
    cl = pool->chain;

    if(cl) {
        pool->chain = cl->next;
        return cl;
    }
    /* 如果取不到，则从内存池pool上分配一个数据结构  */
    cl = ngx_palloc(pool, sizeof(ngx_chain_t));
    if(cl == NULL) {
        return NULL;
    }

    return cl;
}

/**
 * 批量创建多个buf，并且用链表串起来
 */
ngx_chain_t *
ngx_create_chain_of_bufs(ngx_pool_t *pool, ngx_bufs_t *bufs)
{
    u_char *p;
    ngx_int_t i;
    ngx_buf_t *b;
    ngx_chain_t *chain, *cl, **ll;

    /* 在内存池pool上分配bufs->num个 buf缓冲区 ，每个大小为bufs->size */
    p = ngx_palloc(pool, bufs->size * bufs->num);
    if (p == NULL) {
        return NULL;
    }

    ll = &chain;
    /* 循环创建BUF，并且将ngx_buf_t挂载到ngx_chain_t链表上，并且返回链表*/
    for (i = 0; i < bufs->num; i++) {
        b = ngx_calloc_buf(pool);
        if(b == NULL) {
            return NULL;
        }
        b->pos = p;
        b->last = p;
        b->temporary = 1;

        b->start =p;
        p += bufs->size; //p往前增
        b->end = p;

        /* 分配一个ngx_chain_t */
        cl = ngx_alloc_chain_link(pool);
        if(cl == NULL) {
            return NULL;
        }

        /* 将buf，都挂载到ngx_chain_t链表上，最终返回ngx_chain_t链表 */
        cl->buf = b;
        *ll = cl;
        ll = &cl->next;
    }

    *ll = NULL;

    /* 最终得到一个分配了bufs->num的缓冲区链表  */
    return chain;
}

/**
 * 将其它缓冲区链表放到已有缓冲区链表结构的尾部
 */
ngx_int_t
ngx_chain_add_copy(ngx_pool_t *pool, ngx_chain_t **chain, ngx_chain_t *in)
{
    ngx_chain_t *cl; **ll;

    ll = chain;

    /* 找到缓冲区链表结尾部分，cl->next== NULL；cl = *chain既为指针链表地址*/
    for (cl = *chain; cl; cl = cl->next) {
        ll = &cl->next;
    }

    while (in) {
        cl = ngx_alloc_chain_link(pool);
        if(cl == NULL) {
            return NGX_ERROR;
        }

        cl->buf = in->buf;//in上的buf拷贝到cl上面
        *ll = cl; //并且放到chain链表上
        ll = &cl->next; //链表往下走
        in = in->next; //遍历，直到NULL
    }

    *ll = NULL;

    return NGX_OK;
}

ngx_chain_t *
ngx_chain_get_free_buf(ngx_pool_t *p, ngx_chain_t **free)
{
    ngx_chain_t *cl;
    /* 空闲列表中有数据，则直接返回 */
    if(*free) {
        cl = *free;
        *free = cl->next;
        cl->next = NULL;
        return cl;
    }

    /* 否则分配一个新的buf */
    cl = ngx_alloc_chain_link(p);
    if (cl == NULL) {
        return NULL;
    }

    cl->buf = ngx_calloc_buf(p);
    if (cl->buf == NULL) {
        return NULL;
    }

    cl->next = NULL;

    return cl;
}

/**
 * 释放BUF
 * 1. 如果buf不为空，则不释放
 * 2. 如果cl->buf->tag标记不一样，则直接还给Nginx的pool->chain链表
 * 3. 如果buf为空，并且需要释放，则直接释放buf，并且放到free的空闲列表上
 */
void
ngx_chain_update_chains(ngx_pool_t *p, ngx_chain_t **free, ngx_chain_t **busy,
       ngx_chain_t **out, ngx_buf_tag_t tag)
{
    ngx_chain_t *cl;

    /* *busy 指向OUT，将已经输出的out放到busy链表上 */
    if(out) {
        if(*busy == NULL) {
            *busy = *out;
        } else {
            for (cl = *busy; cl->next; cl = cl->next) {}

            cl->next = *out;
        }

        *out = NULL;
    }

    while (*busy) {
        cl = *busy;

        /* 如果buf不为空，则继续遍历  待处理的数据是否都已经处理完成*/
        if (ngx_buf_size(cl->buf) != 0) {
            break;
        }

        /* 如果标识一样，则释放这个BUF  注意 这里的释放与下面回收意义不一样*/
        if (cl->buf->tag != tag) {
            *busy = cl->next;
            ngx_free_chain(p, cl);
            continue;
        }

        /* 直接将buf使用的部分回归到 起点指针地址 */
        cl->buf->pos = cl->buf->start;
        cl->buf->last = cl->buf->start;

        /* 并且将cl放到free列表上 */
        *busy = cl->next;
        cl->next = *free;
        *free = cl;
    }
}

//限速传输,返回总共传输了多少
//合并in链中与第一个节点相邻的文件buf，并且合并长度限制在limit范围内
off_t
ngx_chain_coalesce_file(ngx_chain_t **in, off_t limit)
{
    off_t total, size, aligned, fprev;
    ngx_fd_t fd;
    ngx_chain_t *cl;

    total = 0;

    cl = *in;
    fd = cl->buf->file->fd;

    do {
        //待处理文件总长度
        size = cl->buf->file_last - cl->buf->file_pos;

        if(size > limit - total) {
            size = limit - total;

            //为了对齐，重新计算大小
            aligned = (cl->buf->file_pos + size + ngx_pagesize - 1)
                      & ~((off_t) ngx_pagesize - 1);

            if (aligned <= cl->buf->file_last) {
                size = aligned - cl->buf->file_pos;
            }

            total += size;
            break;
        }

        total +=size;
        fprev = cl->buf->file_pos + size;
        cl = cl->next;

    } while(cl && cl->buf->in_file && total < limit
            && fd == cl->buf->file->fd
            && fprev == cl->buf->file_pos);

    *in = cl;

    return total;
}

//根据已成功发送数据的大小sent更新in链，并返回下一次需要处理的节点
ngx_chain_t *
ngx_chain_update_sent(ngx_chain_t *in, off_t sent)
{
    off_t size;

    for (; in; in = in->next) {
        //又遍历一次这个链接，为了找到那块只成功发送了一部分数据的内存块，从它继续开始发送。
        //返回该buf是否是一个特殊的buf，只含有特殊的标志和没有包含真正的数据
        if (ngx_buf_special(in->buf)) {
            continue;
        }

        if (sent == 0) {
            break;
        }

        size = ngx_buf_size(in->buf);

        if (sent >= size) { //说明该in->buf数据已经全部发送出去
            sent -= size; //标记后面还有多少数据是我发送过的

            if(ngx_buf_in_memory(in->buf)) { //说明该in->buf数据已经全部发送出去
                in->buf->pos = in->buf->last; //清空这段内存。继续找下一个
            }

            if(in->buf->in_file) {
                in->buf->file_pos = in->buf->file_last;
            }

            continue;
        }

        //说明发送出去的最后一字节数据的下一字节数据在in->buf->pos+send位置，下次从这个位置开始发送
        if (ngx_buf_in_memory(in->buf)) {
            in->buf->pos += (size_t) sent; //这块内存没有完全发送完毕，悲剧，下回得从这里开始。
        }

        if (in->buf->in_file) {
            in->buf->file_pos += sent;
        }

        break;

    }

    return in; //下次从这个in开始发送in->buf->pos
}