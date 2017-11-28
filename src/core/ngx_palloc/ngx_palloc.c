/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_palloc.c
* @date:      2017/11/28 下午4:15
* @desc:
*/

//
// Created by daemon.xie on 2017/11/28.
//

#include <ngx_config.h>
#include <ngx_core.h>
#include "ngx_palloc.h"

static ngx_inline void *ngx_palloc_small(ngx_pool_t *pool, size_t size,
    ngx_uint_t align);
static void *ngx_palloc_block(ngx_pool_t *pool, size_t size);
static void *ngx_palloc_large(ngx_pool_t *pool, size_t size);

ngx_pool_t *
ngx_create_pool(size_t size, ngx_log_t *log)
{
    ngx_pool_t *p;

    /**
      * 相当于分配一块内存 ngx_alloc(size, log)
      */
    p = ngx_memalign(NGX_POOL_ALIGNMENT, size, log);
    if ( p == NULL ) {
        return NULL;
    }

    /**
     * Nginx会分配一块大内存，其中内存头部存放ngx_pool_t本身内存池的数据结构
     * ngx_pool_data_t  p->d 存放内存池的数据部分（适合小于p->max的内存块存储）
     * p->large 存放大内存块列表
     * p->cleanup 存放可以被回调函数清理的内存块（该内存块不一定会在内存池上面分配）
     */
    p->d.last = (u_char *) p + sizeof(ngx_pool_t);
    p->d.end = (u_char *)p + size;
    p->d.next = NULL;
    p->d.failed = 0;

    size = size - sizeof(ngx_pool_t);
    p->max = (size < NGX_MAX_ALLOC_FROM_POOL) ? size : NGX_MAX_ALLOC_FROM_POOL;

    /* 只有缓存池的父节点，才会用到下面的这些  ，子节点只挂载在p->d.next,并且只负责p->d的数据内容*/
    p->current = p;
    p->chain = NULL;
    p->large = NULL;
    p->cleanup = NULL;
    p->log = log;

    return p;
}

/**
 * 销毁内存池。
 */
void
ngx_destroy_pool(ngx_pool_t *pool)
{
    ngx_pool_t *p, *n;
    ngx_pool_large_t *l;
    ngx_pool_cleanup_t *c;
    /* 首先清理pool->cleanup链表 */
    for (c = pool->cleanup; c; c= c->next) {
        /* handler 为一个清理的回调函数 */
        if(c->handler) {
            c->handler(c->data);
        }
    }

    /* 清理pool->large链表（pool->large为单独的大数据内存块）  */
    for (l = pool->large; l; l=l->next) {
        if (l->alloc) {
            ngx_free(l->alloc);
        }
    }
    /* 对内存池的data数据区域进行释放 */
    for (p = pool, n = pool->d.next;;
            p = n, n= n->d.next) {
        ngx_free(p);

        if(n == NULL) {
            break;
        }
    }
}

/**
 * 重设内存池
 */
void
ngx_reset_pool(ngx_pool_t *pool)
{
    ngx_pool_t *p;
    ngx_pool_large_t *l;
    /* 清理pool->large链表（pool->large为单独的大数据内存块）  */
    for (l = pool->large; l; l = l->next) {
        if (l->alloc){
            ngx_free(l->alloc);
        }
    }
    /* 循环重新设置内存池data区域的 p->d.last；data区域数据并不擦除*/
    for (p = pool; p ; p = p->d.next) {
        p->d.last = (u_char *)p + sizeof(ngx_pool_t);
        p->d.failed = 0;
    }

    pool->current = pool;
    pool->chain = NULL;
    pool->large = NULL;
}

/**
 * 内存池分配一块内存，返回void类型指针
 */
void *
ngx_palloc(ngx_pool_t *pool, size_t size)
{
#if !(NGX_DEBUG_PALLOC)
    /* 判断每次分配的内存大小，如果超出pool->max的限制，则需要走大数据内存分配策略 */
    if (size <= pool->max) {
        return ngx_palloc_small(pool, size,1);
    }
#endif
    /* 走大数据分配策略 ，在pool->large链表上分配 */
    return ngx_palloc_large(pool, size);
}


/**
 * 内存池分配一块内存，返回void类型指针
 * 不考虑对齐情况
 */
void *
ngx_pnalloc(ngx_pool_t *pool, size_t size)
{
#if !(NGX_DEBUG_PALLOC)
    if (size <= pool->max) {
        return ngx_palloc_small(pool, size, 0);
    }
#endif

    return ngx_palloc_large(pool, size);
}

/**
 *
 * 在分配小块内存时，就算不断的寻找是否存在符合条件的内存大小，若存在，则将内存块地址返回，
 * 并将d.last往后移动分配的内存大小，即完成了内存分配。若不存在，则利用ngx_palloc_block方法去生成一个新的内存块。
 */
static ngx_inline void *
ngx_palloc_small(ngx_pool_t *pool, size_t size, ngx_uint_t align)
{
    u_char *m;
    ngx_pool_t *p;

    p = pool->current;

    do {
        m = p->d.last;

        if(align) {
            /* 对齐操作,会损失内存，但是提高内存使用速度 */
            m = ngx_align_ptr(m, NGX_ALIGNMENT);
        }

        if ((size_t) (p->d.end - m) >= size) {
            p->d.last = m + size;

            return m;
        }

        p = p ->d.next;

    } while(p);
    /* 如果没有缓存池空间没有可以容纳大小为size的内存块，则需要重新申请一个缓存池pool节点 */
    return ngx_palloc_block(pool, size);
}

/**
 * 申请一个新的缓存池 ngx_pool_t
 * 新的缓存池会挂载在主缓存池的 数据区域 （pool->d->next）
 */
static void *
ngx_palloc_block(ngx_pool_t *pool, size_t size)
{
    u_char *m;
    size_t psize;
    ngx_pool_t *p, *new;

    psize = (size_t) (pool->d.end - (u_char *) pool);
    /* 申请新的块 */
    m = ngx_memalign(NGX_POOL_ALIGNMENT, psize, pool->log);
    if (m == NULL) {
        return NULL;
    }

    new = (ngx_pool_t *) m;

    new->d.end = m + psize;
    new->d.next = NULL;
    new->d.failed = 0;
    /* 分配size大小的内存块，返回m指针地址 */
    m += sizeof(ngx_pool_data_t);
    m = ngx_align_ptr(m, NGX_ALIGNMENT);
    new->d.last = m + size;
    /**
      * 缓存池的pool数据结构会挂载子节点的ngx_pool_t数据结构
      * 子节点的ngx_pool_t数据结构中只用到pool->d的结构，只保存数据
      * 每添加一个子节点，p->d.failed就会+1，当添加超过4个子节点的时候，
      * pool->current会指向到最新的子节点地址
      *
      * 这个逻辑主要是为了防止pool上的子节点过多，导致每次ngx_palloc循环pool->d.next链表
      * 将pool->current设置成最新的子节点之后，每次最大循环4次，不会去遍历整个缓存池链表
      */
    for (p = pool->current; p->d.next; p = p->d.next) {
        if (p->d.failed++ > 4) {
            pool->current = p->d.next;
        }
    }

    p->d.next = new;

    return m;
}

/**
 * 当分配的内存块大小超出pool->max限制的时候,需要分配在pool->large上
 */
static void *
ngx_palloc_large(ngx_pool_t *pool, size_t size)
{
    void *p;
    ngx_uint_t n;
    ngx_pool_large_t *large;

    // 分配 size 大小的内存
    p = ngx_alloc(size, pool->log);
    if(p == NULL) {
        return NULL;
    }

    n = 0;
    // 在 pool 的 large 链中寻找存储区为空的节点，把新分配的内存区首地址赋给它
    for (large = pool->large; large; large = large->next) {
        // 找到 large 链末尾，在其后插入之，并返回给外部使用
        if (large->alloc == NULL) {
            large->alloc = p;
            return p;
        }
        // 查看的 large 节点超过 3 个，不再尝试和寻找，由下面代码实现创建新 large 节点的逻辑
        if(n++ > 3) {
            break;
        }
    }
    // 创建 large 链的一个新节点，如果失败则释放刚才创建的 size 大小的内存，并返回 NULL
    large = ngx_palloc_small(pool, sizeof(ngx_pool_large_t), 1);
    if(large == NULL) {
        ngx_free(p);
        return NULL;
    }

    large->alloc = p;
    large->next = pool->large;
    pool->large = large;

    return p;
}

void *
ngx_pmemalign(ngx_pool_t *pool, size_t size, size_t alignment)
{
    void              *p;
    ngx_pool_large_t  *large;

    // 创建一块 size 大小的内存，内存以 alignment 字节对齐
    p = ngx_memalign(alignment, size, pool->log);
    if (p == NULL) {
        return NULL;
    }

    //创建一个 large 节点
    large = ngx_palloc_small(pool, sizeof(ngx_pool_large_t), 1);
    if (large == NULL) {
        ngx_free(p);
        return NULL;
    }

    // 将这个新的 large 节点交付给 pool 的 large 字段
    large->alloc = p;
    large->next = pool->large;
    pool->large = large;

    return p;
}

/**
 * 大内存块释放  pool->large
 */
ngx_int_t
ngx_pfree(ngx_pool_t *pool, void *p)
{
    ngx_pool_large_t  *l;
    /* 在pool->large链上循环搜索，并且只释放内容区域，不释放ngx_pool_large_t数据结构*/
    for (l = pool->large; l; l = l->next) {
        if (p == l->alloc) {
            ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, pool->log, 0,
                           "free: %p", l->alloc);
            ngx_free(l->alloc);
            l->alloc = NULL;

            return NGX_OK;
        }
    }

    return NGX_DECLINED;
}

//封装 palloc 为 pcalloc，实现分配内存并初始化为 0
void *
ngx_pcalloc(ngx_pool_t *pool, size_t size)
{
    void *p;

    p = ngx_palloc(pool, size);
    if(p) {
        ngx_memzero(p, size);
    }

    return p;
}

/**
 * 分配一个可以用于回调函数清理内存块的内存
 * 内存块仍旧在p->d或p->large上
 *
 * ngx_pool_t中的cleanup字段管理着一个特殊的链表，该链表的每一项都记录着一个特殊的需要释放的资源。
 * 对于这个链表中每个节点所包含的资源如何去释放，是自说明的。这也就提供了非常大的灵活性。
 * 意味着，ngx_pool_t不仅仅可以管理内存，通过这个机制，也可以管理任何需要释放的资源，
 * 例如，关闭文件，或者删除文件等等的。下面我们看一下这个链表每个节点的类型
 *
 * 一般分两种情况：
 * 1. 文件描述符
 * 2. 外部自定义回调函数可以来清理内存
 */
ngx_pool_cleanup_t *
ngx_pool_cleanup_add(ngx_pool_t *p, size_t size)
{
    ngx_pool_cleanup_t *c;

    c = ngx_palloc(p, sizeof(ngx_pool_cleanup_t));

    if (size) {
        c->data = ngx_palloc(p, size);
        if (c->data == NULL) {
            return NULL;
        }

    } else {
        c->data = NULL;
    }

    c->handler = NULL;
    c->next = p->cleanup;
    p->cleanup = c;
}

/**
 * 关闭文件回调函数
 * ngx_pool_run_cleanup_file方法执行的时候，用了此函数作为回调函数的，都会被清理
 */
void
ngx_pool_cleanup_file(void *data)
{
    ngx_pool_cleanup_file_t  *c = data;

    if(ngx_close_file(c->fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, c->log, ngx_errno,
                      ngx_close_file_n " \"%s\" failed", c->name);
    }
}

/**
 * 删除文件回调函数
 */
void ngx_pool_delete_file(void *data)
{
    ngx_pool_cleanup_file_t  *c = data;

    ngx_err_t  err;

    if (ngx_delete_file(c->name) == NGX_FILE_ERROR) {
        err = ngx_errno;

        if (err != NGX_ENOENT) {
            ngx_log_error(NGX_LOG_CRIT, c->log, err,
                          ngx_delete_file_n " \"%s\" failed", c->name);
        }
    }

    if (ngx_close_file(c->fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, c->log, ngx_errno,
                      ngx_close_file_n " \"%s\" failed", c->name);
    }
}

/**
 * 清除 p->cleanup链表上的内存块（主要是文件描述符）
 * 回调函数：ngx_pool_cleanup_file
 * 查找指定的 fd，且其 handler 为 ngx_pool_cleanup_file，执行相应动作
 * 这里面有一个遍历的操作
 */
void
ngx_pool_run_cleanup_file(ngx_pool_t *p, ngx_fd_t fd)
{
    ngx_pool_cleanup_t       *c;
    ngx_pool_cleanup_file_t  *cf;

    for(c = p->cleanup; c; c->next) {
        if (c->handler == ngx_pool_cleanup_file) {
            cf = c->data;

            if(cf->fd == fd) {
                c->handler(cf);
                c->handler = NULL;
                return ;
            }
        }
    }
}
