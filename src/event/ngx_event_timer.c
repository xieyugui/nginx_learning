
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>

// 定时器红黑树，键值是超时时间（毫秒时间戳）
ngx_rbtree_t              ngx_event_timer_rbtree;
// 定时器红黑树的哨兵节点
static ngx_rbtree_node_t  ngx_event_timer_sentinel;

/*
 * the event timer rbtree may contain the duplicate keys, however,
 * it should not be a problem, because we use the rbtree to find
 * a minimum timer value only
 */
// 初始化定时器
ngx_int_t
ngx_event_timer_init(ngx_log_t *log)
{
    ngx_rbtree_init(&ngx_event_timer_rbtree, &ngx_event_timer_sentinel,
                    ngx_rbtree_insert_timer_value);

    return NGX_OK;
}

// 在红黑树里查找最小值，即最左边的节点，得到超时的时间差值
// 如果时间已经超过了，那么时间差值就是0
// 意味着在红黑树里已经有事件超时了，必须立即处理
// timer >0 红黑树里即将超时的事件的时间
// timer <0 表示红黑树为空，即无超时事件
// timer==0意味着在红黑树里已经有事件超时了，必须立即处理
// timer==0，epoll就不会等待，收集完事件立即返回
ngx_msec_t
ngx_event_find_timer(void)
{
    ngx_msec_int_t      timer;
    ngx_rbtree_node_t  *node, *root, *sentinel;

    // 红黑树是空的，没有任何定时事件
    if (ngx_event_timer_rbtree.root == &ngx_event_timer_sentinel) {
        return NGX_TIMER_INFINITE;
    }

    // 获取定时器红黑树的根和哨兵
    root = ngx_event_timer_rbtree.root;
    sentinel = ngx_event_timer_rbtree.sentinel;

    // 取最小节点
    node = ngx_rbtree_min(root, sentinel);

    // 减去当前时间戳，获得时间差值
    timer = (ngx_msec_int_t) (node->key - ngx_current_msec);

    // 如果时间已经超过了，那么时间差值就是0
    // 意味着在红黑树里已经有事件超时了，必须立即处理
    return (ngx_msec_t) (timer > 0 ? timer : 0);
}

// 遍历定时器红黑树，找出所有过期的事件，调用handler处理超时
void
ngx_event_expire_timers(void)
{
    ngx_event_t        *ev;
    ngx_rbtree_node_t  *node, *root, *sentinel;

    // 红黑树的哨兵
    sentinel = ngx_event_timer_rbtree.sentinel;

    for ( ;; ) {
        // 红黑树的根
        root = ngx_event_timer_rbtree.root;

        /* 若定时器红黑树为空，则直接返回，不做任何处理 */
        if (root == sentinel) {
            return;
        }

        /* 找出定时器红黑树最左边的节点，即最小的节点，同时也是最有可能超时的事件对象 */
        node = ngx_rbtree_min(root, sentinel);

        /* node->key > ngx_current_msec */

        // 与当前时间进行比较，>0即还没有超时
        // 没有了超时事件，循环退出
        if ((ngx_msec_int_t) (node->key - ngx_current_msec) > 0) {
            return;
        }

        /* 获取超时的具体事件 */
        ev = (ngx_event_t *) ((char *) node - offsetof(ngx_event_t, timer));

        ngx_log_debug2(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                       "event timer del: %d: %M",
                       ngx_event_ident(ev->data), ev->timer.key);

        /* 将已超时事件对象从现有定时器红黑树中移除 */
        ngx_rbtree_delete(&ngx_event_timer_rbtree, &ev->timer);

#if (NGX_DEBUG)
        ev->timer.left = NULL;
        ev->timer.right = NULL;
        ev->timer.parent = NULL;
#endif

        // 定时器标志清零
        ev->timer_set = 0; /* 0表示不受监控 */

        // 设置超时标志
        ev->timedout = 1; /* 1表示已经超时 */

        /* 调用已超时事件的处理函数对该事件进行处理 */
        ev->handler(ev);
    }
}


ngx_int_t
ngx_event_no_timers_left(void)
{
    ngx_event_t        *ev;
    ngx_rbtree_node_t  *node, *root, *sentinel;

    sentinel = ngx_event_timer_rbtree.sentinel;
    root = ngx_event_timer_rbtree.root;

    if (root == sentinel) {
        return NGX_OK;
    }

    for (node = ngx_rbtree_min(root, sentinel);
         node;
         node = ngx_rbtree_next(&ngx_event_timer_rbtree, node))
    {
        ev = (ngx_event_t *) ((char *) node - offsetof(ngx_event_t, timer));

        if (!ev->cancelable) {
            return NGX_AGAIN;
        }
    }

    /* only cancelable timers left */

    return NGX_OK;
}
