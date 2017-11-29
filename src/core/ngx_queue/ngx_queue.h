/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_queue.h
* @date:      2017/11/29 下午10:35
* @desc:
*/

//
// Created by daemon.xie on 2017/11/29.
//

#include <ngx_config.h>
#include <ngx_core.h>

#ifndef NGX_QUEUE_NGX_QUEUE_H
#define NGX_QUEUE_NGX_QUEUE_H

typedef struct ngx_queue_s ngx_queue_t;

/**
 * 链表的数据结构非常简单，ngx_queue_s会挂载到实体
 * 结构上。然后通过ngx_queue_s来做成链表
 */
struct ngx_queue_s {
    ngx_queue_t *prev; //前一个元素
    ngx_queue_t *next; //后一个元素
};

//初始化队列
#define ngx_queue_init(q) (q)->prev = q; (q)->next = q

//判断队列是否为空
#define ngx_queue_empty(h) (h == (h)->prev)

//向链表H后面插入一个x的Q，支持中间插入
#define ngx_queue_insert_head(h,x) \
    (x)->next = (h)->next;  \
    (x)->next->prev = x;    \
    (h)->next = x

#define ngx_queue_inster_after ngx_queue_insert_head

//在尾节点之后插入新节点
#define ngx_queue_insert_tail(h, x)     \
    (x)->prev = (h)->prev;  \
    (x)->prev->next = x;    \
    (x)->next = h;  \
    (h)->prev = x;

//返回链表第一个元素
#define ngx_queue_head(h) (h)->next

//返回链表最后一个元素
#define ngx_queue_last(h) (h)->prev

//返回链表本身地址
#define ngx_queue_sentinel(h) (h)

//返回节点q的下一个元素
#define ngx_queue_next(q) (q)->next

//返回节点q的前一个元素
#define ngx_queue_prev(q) (q)->prev

//从链表中删除该元素
#define ngx_queue_remove(x) \
    (x)->next->prev = (x)->prev;    \
    (x)->prev->next = (x)->next

/**
 * 拆分链表，h是链表容器，q是链表h中的一个元素
 * 分拆成两个链表，前半部分h不包括q
 * 后半部分链表q是n的首个元素
 */
#define ngx_queue_split(h, q, n)    \
    (n)->prev = (h)->prev;  \
    (n)->prev->next = n;    \
    (n)->next = q;  \
    (h)->prev = (q)->prev;  \
    (h)->prev->next = h;    \
    (q)->prev  = n;

//合并链表，将n链表添加到h链表尾部
#define ngx_queue_add(h, n) \
    (h)->prev->next = (n)->next;    \
    (n)->next->prev = (h)->prev;    \
    (h)->prev = (n)->prev;  \
    (h)->prev->next = h;

//使用offsetof宏，根据已知的一个已经分配空间的结构体对象指针a中的某个成员b的地址，来获取该结构体指针对象a地址
//获取队列节点数据
#define ngx_queue_data(q, type, link)   \
    (type *) ((u_char *) q - offsetof(type, link))

/**
 * 返回链表中心元素，如链表共有N个元素，ngx_queue_middle
 * 的方法将返回第N/2+1个元素
 */
ngx_queue_t *ngx_queue_middle(ngx_queue_t *queue);

//排序队列(稳定的插入排序)
void ngx_queue_sort(ngx_queue_t *queue,
    ngx_int_t (*cmp)(const ngx_queue_t *, const ngx_queue_t *));

#endif //NGX_QUEUE_NGX_QUEUE_H
