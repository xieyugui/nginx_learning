/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui
* @software:  CLion
* @file:      ngx_queue.c
* @date:      2017/11/29 下午10:35
* @desc:
*/

//
// Created by daemon.xie on 2017/11/29.
//

#include <ngx_core.h>
#include <ngx_config.h>

/**
 * 在队列中找中间元素
 * 1 2 3 4 5   中间元素为3
 * 1 2         中间元素为2
 */
ngx_queue_t  *
ngx_queue_middle(ngx_queue_t *queue)
{
    ngx_queue_t *middle, *next;

    middle = ngx_queue_head(queue);

    //如果队列里只有一个元素就直接返回该元素
    if(middle == ngx_queue_last(queue)) {
        return middle;
    }

    //返回链表第一个元素
    next = ngx_queue_head(queue);

    for (;;) {
        middle = ngx_queue_next(middle);

        next = ngx_queue_next(next);

        //返回链表最后一个元素
        if(next == ngx_queue_last(queue)) {//偶数个节点，在此返回后半个队列的第一个节点
            return middle;
        }

        next = ngx_queue_next(next);

        if(next == ngx_queue_last(queue)) {//奇数个节点，在此返回中间节点
            return middle;
        }

    }
}

//采用稳定的的插入排序算法来进行排序
void
ngx_queue_sort(ngx_queue_t *queue,
    ngx_int_t (*cmp)(const ngx_queue_t *, const ngx_queue_t *))
{
    ngx_queue_t *q, *prev, *next;

    q = ngx_queue_head(queue);

    if (q == ngx_queue_last(queue)) {
        return;
    }

    for (q = ngx_queue_next(q); q != ngx_queue_sentinel(queue); q = next) {
        prev = ngx_queue_prev(q);
        next = ngx_queue_next(q);

        ngx_queue_remove(q);

        do {
            if (cmp(prev, q) <= 0) {
                break;
            }

            prev = ngx_queue_prev(prev);

        } while (prev != ngx_queue_sentinel(queue));

        ngx_queue_insert_after(prev, q);
    }
}