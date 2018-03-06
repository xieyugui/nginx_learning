/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_slab.h
* @date:      2018/3/5 下午1:45
* @desc:
 * https://github.com/nginx/nginx/blob/branches/stable-1.12/src/core/ngx_slab.h
 * https://www.cnblogs.com/doop-ymc/p/3412572.html
 * https://github.com/MeteorKL/nginx
 * http://ialloc.org/posts/2013/07/17/ngx-notes-slab-allocator/
*/

//
// Created by daemon.xie on 2018/3/5.
//

#ifndef NGINX_LEARNING_NGX_SLAB_H
#define NGINX_LEARNING_NGX_SLAB_H

#include <ngx_config.h>
#include <ngx_core.h>

typedef struct ngx_slab_page_s ngx_slab_page_t;

struct ngx_slab_page_s {
    /**
    当需要分配新的页的时候，分配N个页ngx_slab_page_s结构中第一个页的slab表示这次一共分配了多少个页
        标记这是连续分配多个page，并且我不是首page，例如一次分配3个page,分配的page为[1-3]，则page[1].slab=3
        page[2].slab=page[3].slab=NGX_SLAB_PAGE_BUSY记录
    如果OBJ<128一个页中存放的是多个obj(例如128个32字节obj),则slab记录里面的obj的大小，见ngx_slab_alloc_locked
    如果obj移位大小为ngx_slab_exact_shift，也就是obj128字节，page->slab = 1;
        page->slab存储obj的bitmap,例如这里为1，表示说第一个obj分配出去了   见ngx_slab_alloc_locked
    如果obj移位大小为ngx_slab_exact_shift，也就是obj>128字节，page->slab = ((uintptr_t) 1 << NGX_SLAB_MAP_SHIFT) | shift;
        大于128，也就是至少256�,4K最多也就16个256，因此只需要slab的高16位表示obj位图即可
    当分配某些大小的obj的时候(一个缓存页存放多个obj)，slab表示被分配的缓存的占用情况(是否空闲)，以bit位来表示
     slab:slab为使用较为复杂的一个字段，有以下四种使用情况
    　　a.存储为些结构相连的pages的数目(slab page管理结构)
    　　b.存储标记chunk使用情况的bitmap(size = exact_size)
    　　c.存储chunk的大小(size < exact_size)
    　　d.存储标记chunk的使用情况及chunk大小(size > exact_size)
     */
    uintptr_t slab; //ngx_slab_init中初始赋值为共享内存中剩余页的个数
    ngx_slab_page_t *next; //在分配较小obj的时候，next指向slab page在pool->pages的位置
    uintptr_t prev; //上一个page页
};

typedef struct {
    ngx_uint_t total;
    ngx_uint_t used;
    ngx_uint_t reqs; // 请求到的次数，分配到的次数
    ngx_uint_t fails; // 分配失败次数
} ngx_slab_stat_t;

typedef struct {
    ngx_shmtx_sh_t lock; //mutex的锁

    size_t min_size; //内存缓存obj最小的大小，一般是1个byte /* 最小分配单元 */
    //slab pool以shift来比较和计算所需分配的obj大小、每个缓存页能够容纳obj个数以及所分配的页在缓存空间的位置
    size_t min_shift; //ngx_init_zone_pool中默认为3  /* 最小分配单元对应的位移 */

    ngx_slab_page_t *pages;  /* 页数组 */
    ngx_slab_page_t  *last; // 最后一块内存
    ngx_slab_page_t   free; // 空闲内存

    ngx_slab_stat_t  *stats;
    ngx_uint_t        pfree; // 内存块个数

    u_char           *start; //数据区的起始地址
    u_char           *end; //数据区的结束地址

    ngx_shmtx_t       mutex; //共享内存锁

    u_char           *log_ctx;
    u_char            zero;

    unsigned          log_nomem:1;

    void             *data; // 具体使用场景业务的结构体
    void             *addr; // 共享内存起始地址，也就是本结构体起始地址
} ngx_slab_pool_t;

// 初始化slab池
void ngx_slab_init(ngx_slab_pool_t *pool);

// 未加锁的
void *ngx_slab_alloc(ngx_slab_pool_t *pool, size_t size);

// 在调用前已加锁，分配指定大小空间
void *ngx_slab_alloc_locked(ngx_slab_pool_t *pool, size_t size);
void *ngx_slab_calloc(ngx_slab_pool_t *pool, size_t size);
void *ngx_slab_calloc_locked(ngx_slab_pool_t *pool, size_t size);

// 释放空间
void ngx_slab_free(ngx_slab_pool_t *pool, void *p);
void ngx_slab_free_locked(ngx_slab_pool_t *pool, void *p);

#endif //NGINX_LEARNING_NGX_SLAB_H
