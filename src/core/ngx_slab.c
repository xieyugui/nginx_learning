/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_slab.c
* @date:      2018/3/5 下午1:45
* @desc:
*/

//
// Created by daemon.xie on 2018/3/5.
//

#include <ngx_config.h>
#include <ngx_core.h>

/*
由于指针是4的倍数,那么后两位一定为0,此时我们可以利用指针的后两位做标记,充分利用空间.
在nginx的slab中,我们使用ngx_slab_page_s结构体中的指针pre的后两位做标记,用于指示该page页面的slot块数与ngx_slab_exact_size的关系.
当page划分的slot块小于32时候,pre的后两位为NGX_SLAB_SMALL.
当page划分的slot块等于32时候,pre的后两位为NGX_SLAB_EXACT
当page划分的slot大于32块时候,pre的后两位为NGX_SLAB_BIG
当page页面不划分slot时候,即将整个页面分配给用户,pre的后两位为NGX_SLAB_PAGE
*/
#define NGX_SLAB_PAGE_MASK   3
#define NGX_SLAB_PAGE        0
#define NGX_SLAB_BIG         1
#define NGX_SLAB_EXACT       2
#define NGX_SLAB_SMALL       3

#if (NGX_PTR_SIZE == 4)

#define NGX_SLAB_PAGE_FREE   0
//标记这是连续分配多个page，并且我不是首page，例如一次分配3个page,分配的page为[1-3]，
//  则page[1].slab=3  page[2].slab=page[3].slab=NGX_SLAB_PAGE_BUSY记录
#define NGX_SLAB_PAGE_BUSY   0xffffffff
#define NGX_SLAB_PAGE_START  0x80000000

#define NGX_SLAB_SHIFT_MASK  0x0000000f
#define NGX_SLAB_MAP_MASK    0xffff0000
#define NGX_SLAB_MAP_SHIFT   16

#define NGX_SLAB_BUSY        0xffffffff

#else /* (NGX_PTR_SIZE == 8) */

#define NGX_SLAB_PAGE_FREE   0
#define NGX_SLAB_PAGE_BUSY   0xffffffffffffffff
#define NGX_SLAB_PAGE_START  0x8000000000000000

#define NGX_SLAB_SHIFT_MASK  0x000000000000000f
#define NGX_SLAB_MAP_MASK    0xffffffff00000000
#define NGX_SLAB_MAP_SHIFT   32

#define NGX_SLAB_BUSY        0xffffffffffffffff

#endif

#define ngx_slab_slots(pool)                                                  \
    (ngx_slab_page_t *) ((u_char *) (pool) + sizeof(ngx_slab_pool_t)) //可用内存开始

//pre的后两位为 slab类型
#define ngx_slab_page_type(page)   ((page)->prev & NGX_SLAB_PAGE_MASK)

#define ngx_slab_page_prev(page)                                              \
    (ngx_slab_page_t *) ((page)->prev & ~NGX_SLAB_PAGE_MASK)

//获得page向对于page[0]的偏移量由于m_page和page数组是相互对应的,即m_page[0]管理page[0]页面,m_page[1]管理page[1]页面.
//所以获得page相对于m_page[0]的偏移量就可以根据start得到相应页面的偏移量.
//得到实际分配的页的起始地址
#define ngx_slab_page_addr(pool, page)                                        \
    ((((page) - (pool)->pages) << ngx_pagesize_shift)                         \
     + (uintptr_t) (pool)->start)

#if (NGX_DEBUG_MALLOC)

#define ngx_slab_junk(p, size)     ngx_memset(p, 0xA5, size)

#elif (NGX_HAVE_DEBUG_MALLOC)

#define ngx_slab_junk(p, size)                                                \
    if (ngx_debug_malloc)          ngx_memset(p, 0xA5, size)

#else

#define ngx_slab_junk(p, size)

#endif

static ngx_slab_page_t *ngx_slab_alloc_pages(ngx_slab_pool_t *pool,
                                             ngx_uint_t pages);
static void ngx_slab_free_pages(ngx_slab_pool_t *pool, ngx_slab_page_t *page,
                                ngx_uint_t pages);
static void ngx_slab_error(ngx_slab_pool_t *pool, ngx_uint_t level,
                           char *text);

//slab页面的大小,32位Linux中为4k,
//设置ngx_slab_max_size = 2048B。如果一个页要存放多个obj，则obj size要小于这个数值
static ngx_uint_t  ngx_slab_max_size;
/*
由于指针是4的倍数,那么后两位一定为0,此时我们可以利用指针的后两位做标记,充分利用空间.
在nginx的slab中,我们使用ngx_slab_page_s结构体中的指针pre的后两位做标记,用于指示该page页面的slot块数与ngx_slab_exact_size的关系.
当page划分的slot块小于32时候,pre的后两位为NGX_SLAB_SMALL.
 .....
*/
//划分32个slot块,每个slot的大小就是ngx_slab_exact_size
static ngx_uint_t  ngx_slab_exact_size;
//每个slot块大小的位移是ngx_slab_exact_shift
static ngx_uint_t  ngx_slab_exact_shift;//ngx_slab_exact_shift = 7，即128的位表示 2的7次方

void
ngx_slab_init(ngx_slab_pool_t *pool)
{
    u_char *p;
    size_t size;
    ngx_int_t m;
    ngx_uint_t i, n, pages;
    ngx_slab_page_t *slots, *page;
    /*
    假设每个page是4KB
    设置ngx_slab_max_size = 2048B。如果一个页要存放多个obj，则obj size要小于这个数值
    设置ngx_slab_exact_size = 128B。分界是否要在缓存区分配额外空间给bitmap
    ngx_slab_exact_shift = 7，即128的位表示
     */
    if (ngx_slab_max_size == 0) {
        ngx_slab_max_size = ngx_pagesize / 2;
        // 精确分配大小，8为一个字节的位数，sizeof(uintptr_t)为一个uintptr_t的字节，我们后面会根据这个size来判断使用不同的分配算法
        ngx_slab_exact_size = ngx_pagesize / (8 * sizeof(uintptr_t));
        for (n = ngx_slab_exact_size; n >>= 1; ngx_slab_exact_shift++) {
            /* void */
        }
    }

    pool->min_size = (size_t) 1 << pool->min_shift; //最小分配的空间是8byte

    //跳过ngx_slab_page_t的空间，也即跳过slab header
    slots = ngx_slab_slots(pool);

    //p指向m_slot数组
    p = (u_char *) slots;
    //slab所管理得内存的大小,slot数组+page页面的大小
    size = pool->end - p;

    ngx_slab_junk(p, size); // 内存写入垃圾字符

    /*
         (min_shift) (ngx_slab_exact_shift)  (ngx_pagesize_shift)
     shift  3           ...     8               ... 12
     size   8            ...    256             ... 4096
         (min_size)  (ngx_slab_exact_size)   (ngx_pagesize_size)
     */
    /*从最小块大小到页大小之间的分级数*/
    n = ngx_pagesize_shift - pool->min_shift; //12-3=9

    /*
    这些slab page是给大小为8，16，32，64，128，256，512，1024，2048byte的内存块 这些slab page的位置是在pool->pages的前面初始化
    共享内存的其实地址开始处数据:ngx_slab_pool_t + 9 * sizeof(ngx_slab_page_t)(slots_m[]) + pages * sizeof(ngx_slab_page_t)(pages_m[]) +pages*ngx_pagesize(这是实际的数据部分，
    每个ngx_pagesize都由前面的一个ngx_slab_page_t进行管理，并且每个ngx_pagesize最前端第一个obj存放的是一个或者多个int类型bitmap，用于管理每块分配出去的内存)
    */
    for (i = 0; i < n; i++) { //这9个slots[]由9 * sizeof(ngx_slab_page_t)指向
        /* only "next" is used in list head */
        slots[i].slab = 0;
        slots[i].next = &slots[i]; /*表明分级数组中还没有要管理的页*/
        slots[i].prev = 0;
    }

    //将p移动n*sizeof(ngx_slab_page_t)个字节,指向stats
    p += n * sizeof(ngx_slab_page_t);

    // 分配stats
    pool->stats = (ngx_slab_stat_t *) p;
    ngx_memzero(pool->stats, n * sizeof(ngx_slab_stat_t));

    //将p移动n*sizeof(ngx_slab_stat_t)个字节,指向m_page数组
    p += n * sizeof(ngx_slab_stat_t);

    size -= n * (sizeof(ngx_slab_page_t) + sizeof(ngx_slab_stat_t));

    //计算一共能够保存多个页，加上ngx_slab_page_t，是因为每一页都会有一个ngx_slab_page_t来表示相关信息
    pages = (ngx_uint_t) (size / (ngx_pagesize + sizeof(ngx_slab_page_t)));

    //指向m_page数组
    pool->pages = (ngx_slab_page_t *) p;
    //把每个缓存页最前面的sizeof(ngx_slab_page_t)字节对应的slab page归0
    ngx_memzero(pool->pages, pages * sizeof(ngx_slab_page_t));

    page = pool->pages;

    /* only "next" is used in list head */
    //初始化free，free.next是下次分配页时候的入口
    pool->free.slab = 0;
    pool->free.next = page;
    pool->free.prev = 0;

    //更新第一个slab page的状态，这儿slab成员记录了整个缓存区的页数目
    page->slab = pages; //第一个pages->slab指定了共享内存中除去头部外剩余页的个数
    page->next = &pool->free;
    page->prev = (uintptr_t) &pool->free;

    //因为对齐的原因,使得m_page数组和数据区域之间可能有些内存无法使用    ++++按照ngx_pagesize 页大小对齐
    pool->start = ngx_align_ptr(p + pages * sizeof(ngx_slab_page_t),
                                ngx_pagesize);


    //由于内存对齐操作(pool->start处内存对齐),可能导致pages减少,
    //所以要调整.m为调整后page页面的减小量.
    m = pages - (pool->end - pool->start) / ngx_pagesize;
    if (m > 0) {
        pages -= m;
        page->slab = pages;
    }

    //跳过pages * sizeof(ngx_slab_page_t)，也就是指向实际的数据页pages*ngx_pagesize
    pool->last = pool->pages + pages; //指向最后一块内存首地址
    pool->pfree = pages; //空闲页数

    pool->log_nomem = 1;
    pool->log_ctx = &pool->zero;
    pool->zero = '\0';
}

//由于是共享内存，所以在进程间需要用锁来保持同步
void *
ngx_slab_alloc(ngx_slab_pool_t *pool, size_t size)
{
    void  *p;

    ngx_shmtx_lock(&pool->mutex);

    p = ngx_slab_alloc_locked(pool, size);

    ngx_shmtx_unlock(&pool->mutex);

    return p;
}

/**
对于给定size,从slab_pool中分配内存.

1.如果size大于等于一页,那么从m_page中查找,如果有则直接返回,否则失败.

2.如果size小于一页,如果链表中有空余slot块.

   (1).如果size大于ngx_slab_exact_size,

        a.设置slab的高16位(32位系统)存放solt对应的map,并且该对应为map的地位对应page中高位的slot块.例如1110对应为第1块slot是可用的,2-4块不可用.
         slab的低16为存储slot块大小的位移.

        b.设置m_page元素的pre指针为NGX_SLAB_BIG.

        c.如果page的全部slot都被使用了,那么将此页面从m_slot数组元素的链表中移除.

   (2).如果size等于ngx_slab_exact_size

        a.设置slab存储slot的map,同样slab中的低位对应高位置的slot.

        b.设置m_page元素的pre指针为NGX_SLAB_EXACT.

        c.如果page的全部slot都被使用了,那么将此页面从m_slot数组元素的链表中移除.

   (3).如果size小于ngx_slab_exact_size

        a.用page中的前几个slot存放slot的map,同样低位对应高位.

        b.设置m_page元素的pre指针为NGX_SLAB_SMALL.

        b.如果page的全部slot都被使用了,那么将此页面从m_slot数组元素的链表中移除.

 3.如果链表中没有空余的slot块,则在free链表中找到一个空闲的页面分配给m_slot数组元素中的链表.

   (1).如果size大于ngx_slab_exact_size,

        a.设置slab的高16位(32位系统)存放solt对应的map,并且该对应为map的地位对应page中高位的slot块.slab的低16为存储slot块大小的位移.

        b.设置m_page元素的pre指针为NGX_SLAB_BIG.

        c.将分配的页面链入m_slot数组元素的链表中.

   (2).如果size等于ngx_slab_exact_size

        a.设置slab存储slot的map,同样slab中的低位对应高位置的slot.

        b.设置m_page元素的pre指针为NGX_SLAB_EXACT.

        c.将分配的页面链入m_slot数组元素的链表中.

   (3).如果size小于ngx_slab_exact_size

        a.用page中的前几个slot存放slot的map,同样低位对应高位.

        b.设置m_page元素的pre指针为NGX_SLAB_SMALL.

        c.将分配的页面链入m_slot数组元素的链表中.

4.成功则返回分配的内存块,即指针p,否则失败,返回null.
 */
void *
ngx_slab_alloc_locked(ngx_slab_pool_t *pool, size_t size)
{//这儿假设page_size是4KB
    size_t s;
    uintptr_t  p, n, m, mask, *bitmap;
    ngx_uint_t i, slot, shift, map;
    ngx_slab_page_t *page, *prev, *slots;

    if (size > ngx_slab_max_size) {//如果是large obj, size >= 2048B

        ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, ngx_cycle->log, 0,
                       "slab alloc: %uz", size);

        //分配1个或多个内存页
        page = ngx_slab_alloc_pages(pool, (size >> ngx_pagesize_shift)
                                          + ((size % ngx_pagesize) ? 1 : 0));

        if (page) {
            //由于m_page和page数组是相互对应的,即m_page[0]管理page[0]页面,m_page[1]管理page[1]页面.
            //所以获得page相对于m_page[0]的偏移量就可以根据start得到相应页面的偏移量.
            p = ngx_slab_page_addr(pool, page);

        } else {
            p = 0;
        }

        goto done;
    }

    //申请内存小于一页
    //计算使用哪个slots[]，也就是需要分配的空间是多少  例如size=9,则会使用slot[1]，也就是16字节
    if (size > pool->min_size) {
        shift = 1;
        for (s = size - 1; s >>= 1; shift++) { /* void */ }
        slot = shift - pool->min_shift; //获得m_slot数组的下标

    } else {
        // 小于最小可分配大小的都放到一个slot里面
        shift = pool->min_shift;
        // 因为小于最小分配的，所以就放在第一个slot里面
        slot = 0;// 分配槽的下标
    }

    //状态统计
    pool->stats[slot].reqs++;

    ngx_log_debug2(NGX_LOG_DEBUG_ALLOC, ngx_cycle->log, 0,
                   "slab alloc: %uz slot: %ui", size, slot);

    //取得分级数组的起始位置
    slots = ngx_slab_slots(pool);
    //获取对应的slot的用于取chunk的页
    page = slots[slot].next;

    //如果之前已经有此类大小obj且那个已经分配的内存缓存页还未满  9 个ngx_slab_page_t都还没有用完
    if (page->next != page) {

        //小obj，size < 128B，更新内存缓存页中的bitmap，并返回待分配的空间在缓存的位置
        if (shift < ngx_slab_exact_shift) {
            //计算具体的page页的存放位置 得到bitmap起始存放的位置
            bitmap = (uintptr_t *) ngx_slab_page_addr(pool, page);

            /*
               例如要分配的size为54字节，则在前面计算出的shift对应的字节数应该是64字节，由于一个页面全是64字节obj大小，所以一共有64
               个64字节的obj，64个obj至少需要64位来表示每个obj是否已使用，因此至少需要64位(也就是8字节，2个int),所以至少要暂用一个64
               字节obj来存储该bitmap信息，第一个64字节obj实际上只用了8字节，其他56字节未用
               */
            /*
             * 计算出到底需要多少个（map个）int来作为bitmap来标识这些内存块空间的使用情况
             * ngx_pagesize = getpagesize(); = 4096 byte, ngx_pagesize_shift = 12
             * 如：shift = 3，则:一页（4096byte）可分为512个 8byte内存块，需要 map = 16 个int(16 * 4 * 8 bit)来作为bitmap
             *    shift = 4, 则:一页（4096byte）可分为256个16byte内存块，需要 map =  8 个int( 8 * 4 * 8 bit)来作为bitmap
             *    shift = 5, 则:一页（4096byte）可分为128个32byte内存块，需要 map =  4 个int( 4 * 4 * 8 bit)来作为bitmap
             */
            //计算需要多少个字节来存储bitmap  ((512 = 4096 >> 3)  / 64)  = 8
            map = (ngx_pagesize >> shift) / (sizeof(uintptr_t) * 8);

            for (n = 0; n < map; n++) {

                //如果page的obj块空闲,也就是bitmap指向的32个obj是否都已经被分配出去了
                if (bitmap[n] != NGX_SLAB_BUSY) {
                    //如果整个page页的所有slab已经用完，则会在后面的ngx_slab_alloc_pages从新获取空间并划分slab，然后分配
                    // 当m 最大值时，i 的值时31(32或者64位)
                    for (m = 1, i = 0; m; m <<= 1, i++) { //如果obj块被占用,继续查找  从这32个obj中找出第一个未被分配出去的obj
                        if (bitmap[n] & m) { // 1& 1  如果是1 表示已经被使用
                            continue;
                        }

                        //找到了，该bitmap对应的第n个(注意是位移操作后的m)未使用，使用他，同时置位该位，
                        // 表示该bitmp[n]已经不能再被分配，因为已经本次分配出去了 置1
                        bitmap[n] |= m;

                        //该bit所处的整个bitmap中的第几位(例如需要3个bitmap表示所有的slab块，
                        // 则现在是第三个bitmap的第一位，则i=64+64+1-1,bit从0开始)
                        i = (n * sizeof(uintptr_t) * 8 + i) << shift;

                        p = (uintptr_t) bitmap + i; //跳到要分配内存地址的位置

                        //状态统计
                        pool->stats[slot].used++;

                        //如果该32位图在这次取到最后第31位(0-31)的时候，前面的bitmap[n] |= m后;使其刚好NGX_SLAB_BUSY，也就是位图填满
                        if (bitmap[n] == NGX_SLAB_BUSY) {
                            for (n = n + 1; n < map; n++) {

                                if (bitmap[n] != NGX_SLAB_BUSY) {
                                    goto done;
                                }
                            }
                            //说明整页都已经满了
                            //& ~NGX_SLAB_PAGE_MASK这个的原因是需要恢复原来的地址，因为低两位在第一次获取空间的时候，做了特殊意义处理
                            prev = ngx_slab_page_prev(page);
                            //pages_m[i]和slot_m[i]取消对应的引用关系，因为该pages_m[i]指向的页page已经用完了
                            prev->next = page->next;
                            //slot_m[i]结构的next和prev指向自己
                            page->next->prev = page->prev;

                            //page的next和prev指向NULL，表示不再可用来分配slot[]对应的obj
                            page->next = NULL;
                            page->prev = NGX_SLAB_SMALL;
                        }

                        goto done;
                    }
                }
            }
        } else if (shift == ngx_slab_exact_shift) {
            //size == 128B，因为一个页可以放32个，用slab page的slab成员来标注每块内存的占用情况，不需要另外在内存缓存区分配bitmap，
            //  并返回待分配的空间在缓存的位置

            //如果整个page页的所有slab已经用完，则会在后面的ngx_slab_alloc_pages从新获取空间并划分slab，然后分配
            for (m = 1, i = 0; m; m <<= 1, i++) {
                if (page->slab & m) {
                    continue;
                }

                //标记第m个slab现在分配出去了
                page->slab |= m;

                //执行完page->slab |= m;后，有可能page->slab == NGX_SLAB_BUSY，表示最后一个obj已经分配出去了
                if (page->slab == NGX_SLAB_BUSY) {
                    //pages_m[i]和slot_m[i]取消对应的引用关系，因为该pages_m[i]指向的页page已经用完了
                    prev = ngx_slab_page_prev(page);
                    prev->next = page->next;
                    page->next->prev = page->prev;

                    page->next = NULL;
                    page->prev = NGX_SLAB_EXACT;
                }

                //返回该obj对应的地址
                p = ngx_slab_page_addr(pool, page) + (i << shift);

                pool->stats[slot].used++;

                goto done;
            }
        } else { /* shift > ngx_slab_exact_shift */
            /**
             使用 page->slab 的高16位存储 bitmap，低 4 位存储 slab 所属 slot 的 shift 值。所以，第二 行的 n 值含义是此 slot 的 slab
             可以切分出多少个 chunk。一个 chunk 状态 又需占用 bitmap 中的一个 bit，在 bitmap 全满时的状态就是 (1 << n) - 1
             */
            //size > 128B，也是更新slab page的slab成员，但是需要预先设置slab的部分bit，因为一个页的obj数量小于32个，并返回待分配的空间在缓存的位置
            mask = ((uintptr_t) 1 << (ngx_pagesize >> shift)) - 1;
            //slab和slot块是逆序对应的,即slab的高位对应page低位的slot块
            mask <<= NGX_SLAB_MAP_SHIFT;

            // i++ [0,15] i 代表第几个chunk
            for (m = (uintptr_t) 1 << NGX_SLAB_MAP_SHIFT, i = 0;
                 m & mask;
                 m <<= 1, i++)
            {
                if (page->slab & m) { //该位对应的块不可用
                    continue;
                }

                page->slab |= m; //设置对应块为已使用
                //该页的slot块已经全部使用,就从m_slot中移除
                if ((page->slab & NGX_SLAB_MAP_MASK) == mask) {
                    prev = ngx_slab_page_prev(page);
                    prev->next = page->next;
                    page->next->prev = page->prev;

                    page->next = NULL;
                    page->prev = NGX_SLAB_BIG;
                }

                //找到对应的page页面的slot块
                p = ngx_slab_page_addr(pool, page) + (i << shift);

                pool->stats[slot].used++;

                goto done;
            }
        }

        ngx_slab_error(pool, NGX_LOG_ALERT, "ngx_slab_alloc(): page is busy");
        ngx_debug_point();
    }

    //如果当前slot 的内存块都被用完了，则重新分出一页加入到0m_slot数组对应元素中
    page = ngx_slab_alloc_pages(pool, 1);

    if (page) {
        //根据slot块数设置map
        if (shift < ngx_slab_exact_shift) {
            //slot块的map存储在page的slot中定位到对应的page
            //page页的起始地址的一个uintptr_t类型4字节用来存储bitmap信息
            bitmap = (uintptr_t *) ngx_slab_page_addr(pool, page);

            //计算bitmap需要多少个slot obj块
            //4096 >> 3 == 512     512/64 == 8
            n = (ngx_pagesize >> shift) / ((1 << shift) * 8);

            if (n == 0) {
                n = 1;//至少需要一个4M页面大小的一个obj(2<<shift字节)来存储bitmap信息
            }

            //设置对应的slot块的map,对于存放map的slot设置为1,表示已使用并且设置第一个可用的slot块(不是用于记录map的slot块)
            //计算N * bitmap 需要占用的内存大小,并赋值给bitmap[0]
            //标记为1,因为本次即将使用.
            /* "n" elements for bitmap, plus one requested */
            bitmap[0] = ((uintptr_t) 2 << n) - 1;

            //计算所有obj的位图需要多少个uintptr_t存储。例如每个obj大小为64字节，则4K里面有64个obj，也就需要8字节，两个bitmap
            map = (ngx_pagesize >> shift) / (sizeof(uintptr_t) * 8);

            for (i = 1; i < map; i++) {
                bitmap[i] = 0; //从第二个bitmap开始的所有位先清0
            }

            page->slab = shift; //该页的一个obj对应的字节移位数大小
            //指向上面的slots_m[i],例如obj大小64字节，则指向slots[2]   slots[0-8] -----8-2048
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_SMALL;//标记该页中存储的是小与128的obj

            slots[slot].next = page;

            //TODO 统计该slot块，目前还有多少可用chunk？
            pool->stats[slot].total += (ngx_pagesize >> shift) - n;

            //返回对应地址.  例如为64字节obj，则返回的start为第二个开始处obj，下次分配从第二个开始获取地址空间obj
            p = ngx_slab_page_addr(pool, page) + (n << shift);

            pool->stats[slot].used++;

            goto done;

        } else if (shift == ngx_slab_exact_shift) {
            //slab设置为1   page->slab存储obj的bitmap,例如这里为1，表示说第一个obj分配出去了
            // 4K有32个128字节obj,因此一个slab位图刚好可以表示这32个obj
            page->slab = 1;
            page->next = &slots[slot];
            //用指针的后两位做标记,用NGX_SLAB_SMALL表示slot块小于ngx_slab_exact_shift
            /*
                设置slab的高16位(32位系统)存放solt对应的map,并且该对应为map的地位对应page中高位的slot块.slab的低16为存储slot块大小的位移.
               */
            page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_EXACT;

            slots[slot].next = page;

            pool->stats[slot].total += sizeof(uintptr_t) * 8;

            //返回对应地址
            p = ngx_slab_page_addr(pool, page);

            pool->stats[slot].used++;

            goto done;

        } else { /* shift > ngx_slab_exact_shift */

            //大于128，也就是至少256,4K最多也就16个256，因此只需要slab的高16位表示obj位图即可
            page->slab = ((uintptr_t) 1 << NGX_SLAB_MAP_SHIFT) | shift;
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_BIG;

            slots[slot].next = page;

            pool->stats[slot].total += ngx_pagesize >> shift;

            p = ngx_slab_page_addr(pool, page);

            pool->stats[slot].used++;

            goto done;
        }
    }

    p = 0;

    pool->stats[slot].fails++;

done:

    ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, ngx_cycle->log, 0,
                   "slab alloc: %p", (void *) p);

    return (void *) p;
}


void *
ngx_slab_calloc_locked(ngx_slab_pool_t *pool, size_t size)
{
    void  *p;

    p = ngx_slab_alloc_locked(pool, size);
    if (p) {
        ngx_memzero(p, size);
    }

    return p;
}


void
ngx_slab_free(ngx_slab_pool_t *pool, void *p)
{
    ngx_shmtx_lock(&pool->mutex);

    ngx_slab_free_locked(pool, p);

    ngx_shmtx_unlock(&pool->mutex);
}

/**
根据给定的指针p,释放相应内存块.

1.找到p对应的内存块和对应的m_page数组元素,

2.根据m_page数组元素的pre指针确定页面类型

     (1).如果NGX_SLAB_SMALL类型,即size小于ngx_slab_exact_size

        a.设置相应slot块为可用

        b.设如果整个页面都可用,则将页面归入free中

    (2).如果NGX_SLAB_EXACT类型,即size等于ngx_slab_exact_size

        a.设置相应slot块为可用

        b.设如果整个页面都可用,则将页面归入free中

    (3).如果NGX_SLAB_BIG类型,即size大于ngx_slab_exact_size

        a.设置相应slot块为可用

        b.设如果整个页面都可用,则将页面归入free中

     (4).如果NGX_SLAB_PAGE类型,即size大小大于等于一个页面

        a.设置相应页面块为可用

        b.将页面归入free中
 */

void
ngx_slab_free_locked(ngx_slab_pool_t *pool, void *p)
{
    size_t            size;
    uintptr_t         slab, m, *bitmap;
    ngx_uint_t        i, n, type, slot, shift, map;
    ngx_slab_page_t  *slots, *page;

    ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, ngx_cycle->log, 0, "slab free: %p", p);

    //判断异常情况
    if ((u_char *) p < pool->start || (u_char *) p > pool->end) {
        ngx_slab_error(pool, NGX_LOG_ALERT, "ngx_slab_free(): outside of pool");
        goto fail;
    }

    //计算释放的page的偏移
    n = ((u_char *) p - pool->start) >> ngx_pagesize_shift;
    //获取对应slab_page管理结构的位置
    page = &pool->pages[n];
    slab = page->slab;
    //获取page的类型
    type = ngx_slab_page_type(page);

    switch (type) {
        //page类型为小于128时
        case NGX_SLAB_SMALL:

            //此时chunk的大小存放于slab中  放在低4位
            shift = slab & NGX_SLAB_SHIFT_MASK;
            //计算chunk的大小
            size = (size_t) 1 << shift;

            //由于对齐,p的地址一定是slot大小的整数倍
            if ((uintptr_t) p & (size - 1)) {
                goto wrong_chunk;
            }

            //这段特别巧妙，由于前面对页进行了内存对齐的处理，因此下面的式子可直接
            //求出p位于的chunk偏移，即是page中的第几个chunk.
            n = ((uintptr_t) p & (ngx_pagesize - 1)) >> shift;

            //计算chunk位于bitmap管理的chunk的偏移，注意对2的n次方的取余操作的实现
            m = (uintptr_t) 1 << (n % (sizeof(uintptr_t) * 8));
            //计算p指向的chunk位于第几个bitmap中
            n /= sizeof(uintptr_t) * 8;
            //计算bitmap的起始位置
            bitmap = (uintptr_t *)
                    ((uintptr_t) p & ~((uintptr_t) ngx_pagesize - 1));

            //判断是否处于free状态
            if (bitmap[n] & m) {
                slot = shift - pool->min_shift;

                //将page插入到对应slot分级数组管理的slab链表中，位于头部
                if (page->next == NULL) {
                    //找到对应的m_slot的元素
                    slots = ngx_slab_slots(pool);

                    page->next = slots[slot].next;
                    //链入对应的slot中
                    slots[slot].next = page;

                    //设置m_page的pre
                    page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_SMALL;
                    page->next->prev = (uintptr_t) page | NGX_SLAB_SMALL;
                }
                //置释放标记
                bitmap[n] &= ~m;
                //计算chunk的个数
                n = (ngx_pagesize >> shift) / ((1 << shift) * 8);

                if (n == 0) {
                    n = 1;
                }

                //检查首个bitmap对bitmap占用chunk的标记情况
                if (bitmap[0] & ~(((uintptr_t) 1 << n) - 1)) {
                    goto done;
                }

                //计算bitmap的个数
                map = (ngx_pagesize >> shift) / (sizeof(uintptr_t) * 8);

                //check 一下bitmap 是否被使用过
                for (i = 1; i < map; i++) {
                    if (bitmap[i]) {
                        goto done;
                    }
                }

                //如果释放后page中没有在使用的chunk，则进行进一步的回收，改用slab_page进行管理
                ngx_slab_free_pages(pool, page, 1);

                pool->stats[slot].total -= (ngx_pagesize >> shift) - n;

                goto done;
            }

            goto chunk_already_free;

        case NGX_SLAB_EXACT:

            //p所对应的slot块在slab(slot位图)中的位置
            m = (uintptr_t) 1 <<
                              (((uintptr_t) p & (ngx_pagesize - 1)) >> ngx_slab_exact_shift);
            size = ngx_slab_exact_size;

            //如果p为page中的slot块,那么一定是size整数倍
            if ((uintptr_t) p & (size - 1)) {
                goto wrong_chunk;
            }

            if (slab & m) {
                slot = ngx_slab_exact_shift - pool->min_shift;

                //如果整个页面中的slot块都被使用,则设置当前块为0,并将该页面链入m_slot中
                if (slab == NGX_SLAB_BUSY) {
                    //定位m_slot数组
                    slots = ngx_slab_slots(pool);

                    //设置page[]元素和slot[]的对应关系，通过prev和next指向
                    page->next = slots[slot].next;
                    slots[slot].next = page;

                    //设置m_page元素对应的pre
                    page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_EXACT;
                    page->next->prev = (uintptr_t) page | NGX_SLAB_EXACT;
                }

                //将slot块对应位置设置为0
                page->slab &= ~m;

                //page页面中有正在使用的slot块,因为slab位图不为0
                if (page->slab) {
                    goto done;
                }

                //page页面中所有slot块都没有使用
                ngx_slab_free_pages(pool, page, 1);

                pool->stats[slot].total -= sizeof(uintptr_t) * 8;

                goto done;
            }

            goto chunk_already_free;

        case NGX_SLAB_BIG:

            //slab的高16位是slot块的位图,低16位用于存储slot块大小的偏移
            shift = slab & NGX_SLAB_SHIFT_MASK;
            size = (size_t) 1 << shift;

            if ((uintptr_t) p & (size - 1)) {
                goto wrong_chunk;
            }

            //找到该slot块在位图中的位置.这里要注意一下,
            //位图存储在slab的高16位,所以要+16(即+ NGX_SLAB_MAP_SHIFT)
            m = (uintptr_t) 1 << ((((uintptr_t) p & (ngx_pagesize - 1)) >> shift)
                                  + NGX_SLAB_MAP_SHIFT);

            //该slot块确实正在被使用
            if (slab & m) {
                slot = shift - pool->min_shift;

                //如果整个页面中的所有obj块都被使用,则该页page[]和slot[]没有对应关系,因此需要把页page[]和slot[]对应关系加上
                if (page->next == NULL) {
                    //定位m_slot数组
                    slots = ngx_slab_slots(pool);

                    page->next = slots[slot].next;
                    slots[slot].next = page;

                    page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_BIG;
                    page->next->prev = (uintptr_t) page | NGX_SLAB_BIG;
                }

                //设置slot块对应的位图位置为0,即可用
                page->slab &= ~m;

                //如果page页中有slot块还在被使用
                if (page->slab & NGX_SLAB_MAP_MASK) {
                    goto done;
                }

                ngx_slab_free_pages(pool, page, 1);

                pool->stats[slot].total -= ngx_pagesize >> shift;

                goto done;
            }

            goto chunk_already_free;

        case NGX_SLAB_PAGE: //用户归还整个页面

            if ((uintptr_t) p & (ngx_pagesize - 1)) {//p是也对齐的，检查下
                goto wrong_chunk;
            }

            if (!(slab & NGX_SLAB_PAGE_START)) {
                ngx_slab_error(pool, NGX_LOG_ALERT,
                               "ngx_slab_free(): page is already free");
                goto fail;
            }

            if (slab == NGX_SLAB_PAGE_BUSY) {
                //说明是连续分配多个page的非首个page，不能直接释放，不许这几个page一起释放，因此p指针指向必须是首page
                ngx_slab_error(pool, NGX_LOG_ALERT,
                               "ngx_slab_free(): pointer to wrong page");
                goto fail;
            }

            //计算页面对应的m_page槽
            n = ((u_char *) p - pool->start) >> ngx_pagesize_shift;
            //计算归还page的个数
            size = slab & ~NGX_SLAB_PAGE_START;
            //归还页面
            ngx_slab_free_pages(pool, &pool->pages[n], size);

            ngx_slab_junk(p, size << ngx_pagesize_shift);

            return;
    }

    /* not reached */

    return;

done:

    pool->stats[slot].used--;

    ngx_slab_junk(p, size);

    return;

wrong_chunk:

    ngx_slab_error(pool, NGX_LOG_ALERT,
                   "ngx_slab_free(): pointer to wrong chunk");

    goto fail;

chunk_already_free:

    ngx_slab_error(pool, NGX_LOG_ALERT,
                   "ngx_slab_free(): chunk is already free");

fail:

    return;
}

/*
返回一个slab page，这个slab page之后会被用来确定所需分配的空间在内存缓存的位置
例如总共有6个page[]，ngx_slab_init中page[0]的next和prev都指向free，free的next也指向page[0]。当调用ngx_slab_alloc_pages向获取3个pages的时候
则前三个pages(page[0], page[1], page[2])会被分配好，最末尾page[5]的prev指向page[3],并且page[3]的slab指定现在只有6-3=3个page可以分配了，
然后page[3]的next和prev指向free,free的next和prev也指向page[3]，也就是下次只能从page[3]开始获取页
*/ //分配一个页面,并将页面从free中摘除.

/*
-------------------------------------------------------------------
| page1  | page2 | page3 | page4| page5) | page6 | page7 | page8 |
--------------------------------------------------------------------
初始状态: pool->free指向page1,page1指向poll->free,其他的page的next和prev默认指向NULL,也就是pool->free指向整个page体，page1->slab=8
1.假设第一次ngx_slab_alloc_pages获取两个page，则page1和page2会分配出去，page1为这两个page的首page1->slab只出这两个连续page是一起分配的,
    page2->slab = NGX_SLAB_PAGE_BUSY;表示这是跟随page1一起分配出去的，并且本page不是首page。这时候pool->free指向page3,并表示page3->slab=6，
    表示page3开始还有6个page可以使用
2.假设第二次又获取了3个page(page3-5),则page3是首page,page3->slab=3,同时page4,page5->slab = NGX_SLAB_PAGE_BUSY;表示这是跟随page1一
    起分配出去的，并且本page不是首page。这时候pool->free指向page6,并表示page6->slab=3，表示page6开始还有3个page可以使用
3. 同理再获取1个page,page6。这时候pool->free指向page7,并表示page7->slab=2，表示page7开始还有2个page可以使用
4. 现在释放第1步page1开始的2个page，则在ngx_slab_free_pages中会把第3步后剩余的两个未分配的page7(实际上是把page7开始的2个page标识为1个大page)
   和page1(page1实际上这时候标识的是第一步中的page1开始的2个page)和pool->free形成一个双向链表环，可以见

                      pool->free
                    ----------  /
 -----------------\|         | --------------------------------------
|  ----------------|         | -----------------------------------   |
|  |                 -----------                                  |  |
|  |                                                              |  |
|  |     page1 2                                   page7 2        |  |
|  |  \ ----------                          \  -----------   /    |  |
|   --- |    |    |----------------------------|    |     | -------  |
------- |    |    |--------------------------- |    |     | ----------
        ----------  \                           ---------
5.当释放ngx_slab_free_pagespage3开始的3个page页后，page3也会连接到双向环表中，链如pool->free与page1[i]之间，注意这时候的page1和page2是紧靠一起的page
  但没有对他们进行合并，page1->slab还是=2  page2->slab还是=3。并没有把他们合并为一个整体page->slab=5,如果下次想alloc一个4page的空间，是
  分配不成功的
*/
static ngx_slab_page_t *
ngx_slab_alloc_pages(ngx_slab_pool_t *pool, ngx_uint_t pages)
{
    ngx_slab_page_t  *page, *p;

    //初始化的时候pool->free.next默认指向第一个pool->pages
    //从pool->free.next开始，每次取(slab page) page = page->next
    for (page = pool->free.next; page != &pool->free; page = page->next) {//如果一个可用page页都没有，就不会进入循环体

        /*
        本个slab page剩下的缓存页数目>=需要分配的缓存页数目pages则可以分配，否则继续遍历free,直到下一个首page及其后连续page数和大于等于需要分配的pages数，才可以分配
        slab是首次分配page开始的slab个页的时候指定的，在释放的时候slab还是首次分配时候的slab，不会变，也就是说释放page后不会把相邻的两个page页的slab数合并，
        例如首次开辟page1开始的3个page页空间，page1->slab=3,紧接着开辟page2开始的2个page页空间，page2->slab=2,当连续释放page1和page2对应的空间后，他们还是
        两个独立的page[]空间，slab分别是2和3,而不会把这两块连续空间进行合并为1个(也就是新的page3,page3首地址等于page2，并且page3->slab=3+2=5)
        */
        //找到了合适的slab
        if (page->slab >= pages) {

            //对于大于请求page数的情况，会将前pages个切分出去,page[pages]刚好为将
            //pages个切分出去后，逗留下的第一个，正好作为构建新结点的第一个。下面
            //其实是一个双向链表插入及删除节点时的操作
            if (page->slab > pages) {
                page[page->slab - 1].prev = (uintptr_t) &page[pages];

                //更新从本个slab page开始往下第pages个slab page的缓存页数目为本个slab page数目减去pages
                //让下次可以从page[pages]开始分配的页的next和prev指向pool->free,只要页的next和prev指向了free，则表示可以从该页开始分配页page
                page[pages].slab = page->slab - pages;
                page[pages].next = page->next;//该可用页的next指向pool->free.next
                page[pages].prev = page->prev;//该可用页的prev指向pool->free.next
                // ++ pages 和free 相互指向
                p = (ngx_slab_page_t *) page->prev;
                //更新pool->free.next = &page[pages]，下次从第pages个slab page开始进行上面的for()循环遍历
                p->next = &page[pages];
                //指向下次可以分配页的页地址
                page->next->prev = (uintptr_t) &page[pages];

            } else {
                //恰好等于时，不用进行切分直接删除节点  然free自己指向自己
                p = (ngx_slab_page_t *) page->prev;
                p->next = page->next;
                page->next->prev = page->prev;
            }

            //NGX_SLAB_PAGE_START标记page是分配的pages个页的第一个页，并在第一个页page中记录出其后连续的pages个页是一起分配的
            //修改page对应的状态 //更新被分配的page slab中的第一个的slab成员，即页的个数和占用情况
            page->slab = pages | NGX_SLAB_PAGE_START;
            page->next = NULL;
            //page页面不划分slot时候,即将整个页面分配给用户,pre的后两位为NGX_SLAB_PAGE
            page->prev = NGX_SLAB_PAGE;

            pool->pfree -= pages;

            if (--pages == 0) { //pages为1。则直接返回该page
                return page;
            }

            //对于pages大于1的情况，还处理非第一个page的状态，修改为BUSY
            for (p = page + 1; pages; pages--) {
                p->slab = NGX_SLAB_PAGE_BUSY;
                //标记这是连续分配多个page，并且我不是首page，例如一次分配3个page,分配的page为[1-3]，则page[1].slab=3
                // page[2].slab=page[3].slab=NGX_SLAB_PAGE_BUSY记录
                p->next = NULL;
                p->prev = NGX_SLAB_PAGE;
                p++;
            }

            return page;
        }
    }

    if (pool->log_nomem) {
        ngx_slab_error(pool, NGX_LOG_CRIT,
                       "ngx_slab_alloc() failed: no memory");
    }

    //没有找到空余的页
    return NULL;
}

//释放page页开始的pages个页面
/**
    指针page必须指向所要释放的页的首地址，pages是将要释放的页数量。
 该函数被ngx_slab_free_locked函数调用前已经有检查page指针是否指向页开端
 */
static void
ngx_slab_free_pages(ngx_slab_pool_t *pool, ngx_slab_page_t *page,
                    ngx_uint_t pages)
{
    ngx_slab_page_t  *prev, *join;

    pool->pfree += pages;

    //计算结点后部跟的page的数目
    page->slab = pages--;

    //对跟的page的page管理结构slab_page进行清空
    if (pages) {
        /*如果待释放的内存空间不止一页，则需要将后续的页管理单元恢复为初始化状态*/
        ngx_memzero(&page[1], pages * sizeof(ngx_slab_page_t));
    }

    //如果page后面还跟有其他已经使用的节点，则将其连接至page的前一个结点
    /*根据前面几篇的内容可以知道，当页内存管理单元挂接在slot分级链表下时，page->next是不为空的，这时需要先将其从链表中摘除*/
    if (page->next) {
        //解除slot[]和page[]的关联，让slot[]的next和prev指向slot[]自身
        prev = ngx_slab_page_prev(page);
        prev->next = page->next;
        page->next->prev = page->prev;
    }

    //join指向将要删除的N页后面的一页
    /* 连续页最后一片的下一片 */
    join = page + page->slab;

    if (join < pool->last) {//join不是pool中最后一个page

        //取出join指向的页的类型
        if (ngx_slab_page_type(join) == NGX_SLAB_PAGE) {
            /* 如果连续页连着的也是空白页首页，也在free链表中，那么就可以合并起来  */
            if (join->next != NULL) {
                pages += join->slab;
                //把join的页数合并入page，连在一起
                page->slab += join->slab;

                //join脱离链表
                prev = ngx_slab_page_prev(join);
                prev->next = join->next;
                join->next->prev = join->prev;

                join->slab = NGX_SLAB_PAGE_FREE;
                join->next = NULL;
                join->prev = NGX_SLAB_PAGE;
            }
        }
    }

    /* 接下来是合并前面的页，被释放的非第一个页 */
    if (page > pool->pages) {
        //找到page上一个相邻的数组元素
        join = page - 1;

        if (ngx_slab_page_type(join) == NGX_SLAB_PAGE) {

            /* 如果前一个页是非首页，那么移到它的首页（就是prev 保存的，见后文） */
            if (join->slab == NGX_SLAB_PAGE_FREE) {
                //join是free状态，则找到join的上一个节点join->prev
                join = ngx_slab_page_prev(join);
            }

            /* 同理，将自己合并到前一个空白页的后面 */
            if (join->next != NULL) {
                pages += join->slab;
                //把page以后续的页合并入join
                join->slab += page->slab;
                //join从链表中分享出来
                prev = ngx_slab_page_prev(join);
                prev->next = join->next;
                join->next->prev = join->prev;
                //page原来指向的页变成中间页，所以对应的next和prev都清除
                page->slab = NGX_SLAB_PAGE_FREE;
                page->next = NULL;
                page->prev = NGX_SLAB_PAGE;
                //join合并入page,并且join成为头部
                page = join;
            }
        }
    }

    /* 将页头保存到最后一页的prev中，以合并时快速索引 */
    if (pages) { //例如一次alloc 3个page，分配是page[3 4 5],则page[5]的prev指向page[3]
        page[pages].prev = (uintptr_t) page;
    }

    //将page重新归于，slab_page的管理结构之下。放于管理结构的头部
    //这里释放后就会把之前free指向的可用page页与释放的page页以及pool->free形成一个环形链表
    //page变成空闲页，需要重新接入free指向的空闲节点，并且放在free指向的头部
    /* 将空白页插到free 链表最前头 */
    page->prev = (uintptr_t) &pool->free;
    page->next = pool->free.next;

    page->next->prev = (uintptr_t) page;

    pool->free.next = page;
}


static void
ngx_slab_error(ngx_slab_pool_t *pool, ngx_uint_t level, char *text)
{
    ngx_log_error(level, ngx_cycle->log, 0, "%s%s", text, pool->log_ctx);
}
