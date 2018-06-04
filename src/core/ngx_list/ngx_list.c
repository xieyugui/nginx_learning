/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_list.c
* @date:      2017/12/3 下午8:27
* @desc:
*/

//
// Created by daemon.xie on 2017/12/3.
//

#include <ngx_config.h>
#include <ngx_core.h>

ngx_list_t *
ngx_list_create(ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    ngx_list_t *list;

    list = ngx_palloc(pool, sizeof(ngx_list_t));
    if(list == NULL) {
        return NULL;
    }

    if(ngx_list_init(list, pool, n, size) != NGX_OK) {
        return NULL;
    }

    return list;
}

void *
ngx_list_push(ngx_list_t *l)
{
    void *elt;
    ngx_list_part_t *last;

    last = l->last;

    //如果part 的已经使用的nelts 等于每一个part的容量 就说明已经满了
    if (last->nelts == l->nalloc) {
        last = ngx_palloc(l->pool, sizeof(ngx_list_part_t));
        if(last == NULL) {
            return NULL;
        }

        last->elts = ngx_palloc(l->pool, l->nalloc * l->size);
        if(last->elts == NULL) {
            return NULL;
        }

        last->nelts = 0;
        last->next = NULL;

        l->last->next = last;
        l->last = last;

    }

    elt = (char *) last->elts + l->size * last->nelts;
    last->nelts++;

    return elt;
}
