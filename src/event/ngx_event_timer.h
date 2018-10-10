
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 * https://www.kancloud.cn/digest/understandingnginx/202602
 * https://github.com/chronolaw/annotated_nginx/blob/master/nginx/src/event/ngx_event_timer.h
 */


#ifndef _NGX_EVENT_TIMER_H_INCLUDED_
#define _NGX_EVENT_TIMER_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>

// 无限制时间，也是-1
#define NGX_TIMER_INFINITE  (ngx_msec_t) -1
// 允许有300毫秒的误差
#define NGX_TIMER_LAZY_DELAY  300

/* 定时器事件初始化 (初始化一个红黑树结构)*/
ngx_int_t ngx_event_timer_init(ngx_log_t *log);

// 在红黑树里查找最小值，即最左边的节点，得到超时的时间差值
// 如果时间已经超过了，那么时间差值就是0
/* 找出定时器红黑树最左边的节点 */
ngx_msec_t ngx_event_find_timer(void);

// 遍历定时器红黑树，找出所有过期的事件，调用handler处理超时
void ngx_event_expire_timers(void);
// 检查红黑树里是否还有定时器
ngx_int_t ngx_event_no_timers_left(void);

// 定时器红黑树，键值是超时时间（毫秒时间戳）
extern ngx_rbtree_t  ngx_event_timer_rbtree;

/**
定时事件的超时检测
当需要对某个事件进行超时检测时，只需要将该事件添加到定时器红黑树中即可，由函数 ngx_event_add_timer，
将一个事件从定时器红黑树中删除由函数ngx_event_del_timer 实现
 */
/* 从定时器中移除事件 */
static ngx_inline void
ngx_event_del_timer(ngx_event_t *ev)
{
    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "event timer del: %d: %M",
                    ngx_event_ident(ev->data), ev->timer.key);

    /* 从红黑树中移除指定事件的节点对象 */
    ngx_rbtree_delete(&ngx_event_timer_rbtree, &ev->timer);

#if (NGX_DEBUG)
    ev->timer.left = NULL;
    ev->timer.right = NULL;
    ev->timer.parent = NULL;
#endif

    // 事件的标志清零
    ev->timer_set = 0;
}

/* 将事件添加到定时器中 */
static ngx_inline void
ngx_event_add_timer(ngx_event_t *ev, ngx_msec_t timer)
{
    ngx_msec_t      key;
    ngx_msec_int_t  diff;

    // 红黑树key是毫秒时间戳
    // 当前时间加上超时的时间
    key = ngx_current_msec + timer;

    // 如果之前已经加入了定时器红黑树
    // 那么就重新设置时间
    if (ev->timer_set) {

        /*
         * Use a previous timer value if difference between it and a new
         * value is less than NGX_TIMER_LAZY_DELAY milliseconds: this allows
         * to minimize the rbtree operations for fast connections.
         */

        // 计算一下旧超时时间与新超时时间的差值
        diff = (ngx_msec_int_t) (key - ev->timer.key);

        // 减少对红黑树的操作，加快速度提高性能
        if (ngx_abs(diff) < NGX_TIMER_LAZY_DELAY) {
            ngx_log_debug3(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                           "event timer: %d, old: %M, new: %M",
                            ngx_event_ident(ev->data), ev->timer.key, key);
            return;
        }

        // 删除定时器，然后重新加入
        ngx_del_timer(ev);
    }

    // 设置事件的key
    ev->timer.key = key;

    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "event timer add: %d: %M:%M",
                    ngx_event_ident(ev->data), timer, ev->timer.key);

    // 加入红黑树
    ngx_rbtree_insert(&ngx_event_timer_rbtree, &ev->timer);
    // 设置事件的定时器标志
    ev->timer_set = 1;
}


#endif /* _NGX_EVENT_TIMER_H_INCLUDED_ */
