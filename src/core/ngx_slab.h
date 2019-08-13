/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui
* @software:  CLion
* @file:      ngx_slab.h
* @date:      2018/3/5 下午1:45
* @desc:
 * http://blog.csdn.net/zgwhugr0216/article/details/29185103
 * https://my.oschina.net/u/2310891/blog/672539
 * https://github.com/nginx/nginx/blob/branches/stable-1.12/src/core/ngx_slab.h
 * https://github.com/MeteorKL/nginx
 * http://ialloc.org/posts/2013/07/17/ngx-notes-slab-allocator/       ++++++推荐
 * https://raw.githubusercontent.com/MeteorKL/nginx/master/ngx_slab.png
 *
 * https://blog.gobyoung.com/2016/nginx-slab-code-read/ ++
 * https://titenwang.github.io/2016/08/02/slab-shared-memory/ ++
 * http://www.cnblogs.com/doop-ymc/p/3412572.html ++
*/

//
// Created by daemon.xie on 2018/3/5.
//

#ifndef NGINX_LEARNING_NGX_SLAB_H
#define NGINX_LEARNING_NGX_SLAB_H

#include <ngx_config.h>
#include <ngx_core.h>

typedef struct ngx_slab_page_s ngx_slab_page_t; /*用于表示page页内存管理单元和slot分级管理单元*/

struct ngx_slab_page_s {
    /**
    前文 动态内存管理 中描述 过，Nginx 的 slab allcator 基本上相当于为了进行共享内存的管理而开发的一个近似 通用的内存管理器。本文将要详细分析一下 slab allocator 的创建、内存申请和释放 的实现过程。
    slab allocator 代码中用了相当多的位操作，很大一部分操作和 slab allocator 的 分级相关。从 2^3 bytes开始，到 pagesize/2 bytes 为止，提供 2^3, 2^4, 2^5, ..., 2^(ngx_pagesize_shift - 1) 等 ngx_pagesize_shift - 3 个内存片段 大小等级。
    在分配时、Nginx 的 slab allocator 使用 BEST FIT 策略， 从和申请的内存块大小 最接近的 slot 中分配一个 chunk。同时，一个 slab 大小为了个页内存块大小 (通过 getpagesize 调用获得，一般为 4K)。
    这种分级机制，会造成部分内存浪费 (internal fragmentation)，但是大大降低了管理的 复杂性，代码实现起来也比较简单。也算是一种典型的拿空间换时间的样例了。
    为了方便叙述，我们将
    用于表示内存大小分级的 ngx_slab_pool_t 称为 slot。整个分级区域称为 slots 数组。
    用于实际内存分配的 pagesize 大小的内存块称为 slab。整个可用内存块区域称为 slab 数组。
    用于管理 slab 的 ngx_slab_pool_t 称为 slab 管理结构体。整个管理结构 体区域称为 slab 管理结构体数组。
    从 slab 从切分出来的用于完于内存请求的内存块称为 chunk。用于跟踪整个 slab 中 chunk 的使用情况的结构称为 bitmap。
    slot 的分级是按照 2 的幂次方来的，而用来计算 2 的幂次方时，移位操作又是最给 力的。我们将每个 slot 对应的幂值称为 shift，那么这个 slot 能够分配的 chunk 大小就是 1 << shift。这个 slot 申请来的 slab 可以切分出 1 << (ngx_pagesize_shift - shift) 个 chunk。
     *
    当需要分配新的页的时候，分配N个页ngx_slab_page_s结构中第一个页的slab表示这次一共分配了多少个页
        标记这是连续分配多个page，并且我不是首page，例如一次分配3个page,分配的page为[1-3]，则page[1].slab=3
        page[2].slab=page[3].slab=NGX_SLAB_PAGE_BUSY记录  # NGX_SLAB_PAGE_BUSY 改页已被分配使用
    如果OBJ<128一个页中存放的是多个obj(例如128个32字节obj),则slab记录里面的obj的大小，见ngx_slab_alloc_locked
    如果obj移位大小为ngx_slab_exact_shift，也就是obj128字节，page->slab = 1;
        page->slab存储obj的bitmap,例如这里为1，表示说第一个obj分配出去了   见ngx_slab_alloc_locked
    如果obj移位大小为ngx_slab_exact_shift，也就是obj>128字节，page->slab = ((uintptr_t) 1 << NGX_SLAB_MAP_SHIFT) | shift;
        大于128，也就是至少256,4K最多也就16个256，因此只需要slab的高16位表示obj位图即可
    当分配某些大小的obj的时候(一个缓存页存放多个obj)，slab表示被分配的缓存的占用情况(是否空闲)，以bit位来表示
     slab:slab为使用较为复杂的一个字段，有以下四种使用情况
    　　a.存储为些结构相连的pages的数目(slab page管理结构)
    　　b.存储标记chunk使用情况的bitmap(size = exact_size)
    　　c.存储chunk的大小(size < exact_size)
    　　d.存储标记chunk的使用情况及chunk大小(size > exact_size)
     */
    uintptr_t slab; /* 不同场景下用于：存储 bitmap、shift 值、
                            或者后续连续的空闲 slab 个数*/
    ngx_slab_page_t *next;  /* 双链表的前向指针，同时存储 slab 属性信息 */
    uintptr_t prev; //上一个page页
};

typedef struct {
    ngx_uint_t total;
    ngx_uint_t used;
    ngx_uint_t reqs; // 请求到的次数，分配到的次数
    ngx_uint_t fails; // 分配失败次数
} ngx_slab_stat_t;

/*整个内存区的管理结构*/
typedef struct {
    ngx_shmtx_sh_t lock; //mutex的锁

    size_t min_size; //最小分配空间，默认为8字节 /* 最小 chunk 字节数 */
    //slab pool以shift来比较和计算所需分配的obj大小、每个缓存页能够容纳obj个数以及所分配的页在缓存空间的位置
    size_t min_shift; //ngx_init_zone_pool中默认为3  /* 最小分配单元对应的位移 */ /* 最小 chunk 字节数是 2 的多少次幂 */

    ngx_slab_page_t *pages;  /* 页数组 */  /* slab 的管理结构体数组首地址 */
    ngx_slab_page_t  *last; // 指向最后一块内存内存首地址
    ngx_slab_page_t   free; // 空闲页链表 /* 空闲的 slab 管理结构体链表 */

    ngx_slab_stat_t  *stats;
    ngx_uint_t        pfree; // 空闲页的数量

    u_char           *start; //page数组的开始地址
    u_char           *end; //page数组的最后字节

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
/*alloc 和 calloc的区别在于是否在分配的同时将内存清零*/
void *ngx_slab_alloc_locked(ngx_slab_pool_t *pool, size_t size);
void *ngx_slab_calloc(ngx_slab_pool_t *pool, size_t size);
void *ngx_slab_calloc_locked(ngx_slab_pool_t *pool, size_t size);

// 释放空间
void ngx_slab_free(ngx_slab_pool_t *pool, void *p);
void ngx_slab_free_locked(ngx_slab_pool_t *pool, void *p);

#endif //NGINX_LEARNING_NGX_SLAB_H
