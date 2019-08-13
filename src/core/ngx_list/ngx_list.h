/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui
* @software:  CLion
* @file:      ngx_list.h
* @date:      2017/12/3 下午8:26
* @desc:
*/

//
// Created by daemon.xie on 2017/12/3.
//

#ifndef NGX_LIST_NGX_LIST_H
#define NGX_LIST_NGX_LIST_H

#include <ngx_config.h>
#include <ngx_core.h>

typedef struct ngx_list_part_s ngx_list_part_t;

struct ngx_list_part_s {
    void *elts;//指向该节点的数据区(该数据区中可存放nalloc个大小为size的元素)
    ngx_uint_t nelts;//已经存放的元素个数
    ngx_list_part_t *next;//指向链表的下一个part
};

typedef struct {
    ngx_list_part_t *last;//指向链表最后一个节点(part)
    ngx_list_part_t part;//链表的第一个part
    size_t size;//链表中每个元素的大小
    ngx_uint_t nalloc;//每一个part的容量
    ngx_pool_t *pool;//内存池
} ngx_list_t;

//用于创建一个链表
ngx_list_t *ngx_list_create(ngx_pool_t *pool, ngx_uint_t n, size_t size);

static ngx_inline ngx_int_t
ngx_list_init(ngx_list_t *list, ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    list->part.elts = ngx_palloc(pool, n*size);//从内存池申请空间后，让elts指向可用空间
    if (list->part.elts == NULL) {
        return NGX_ERROR;
    }

    list->part.nelts = 0;
    list->part.next = NULL;
    list->last = &list->part;// last开始的时候指向首节点
    list->size = size;
    list->nalloc = n;
    list->pool = pool;

    return NGX_OK;
}

//向链表中push一个元素
void *ngx_list_push(ngx_list_t *list);

#endif //NGX_LIST_NGX_LIST_H
