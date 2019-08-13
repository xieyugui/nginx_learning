/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui
* @software:  CLion
* @file:      ngx_array.h
* @date:      2017/12/1 下午3:20
* @desc:
*/

//
// Created by daemon.xie on 2017/12/1.
//

#include <ngx_config.h>
#include <ngx_core.h>

#ifndef NGX_ARRAY_NGX_ARRAY_H
#define NGX_ARRAY_NGX_ARRAY_H

typedef struct {
    void *elts; //用来描述数组使用的内存块的起始地址
    ngx_uint_t nelts; //用来描述当前内存块已存在的元素个数
    size_t  size; //用来描述数组元素的大小
    ngx_uint_t nalloc;//用来描述内存块最多能容纳的数组元素个数，因此，内存块的结束地址= elts+nalloc*size
    ngx_pool_t  *pool; //表示ngx_array_t使用的内存所在的内存池
} ngx_array_t;

//创建一个动态数组，数组的大小为n，每个元素的大小为size
ngx_array_t *ngx_array_create(ngx_pool_t *p, ngx_uint_t n, size_t size);

//销毁已分配的动态数组元素空间和动态数组对象
void ngx_array_destroy(ngx_array_t *a);

//向数组中添加一个元素，返回这个新元素的地址，如果数组空间已经用完，数组会自动扩充空间
void *ngx_array_push(ngx_array_t *a);

//向数组中添加n个元素，返回这n个元素中第一个元素的地址
void *ngx_array_push_n(ngx_array_t *a, ngx_uint_t n);

//和create函数的功能差不多，只不过这个array不能为空，返回值为是否初始化成功
static ngx_inline ngx_int_t
ngx_array_init(ngx_array_t *array, ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    array->nelts = 0;
    array->size = size;
    array->nalloc = 0;
    array->pool = pool;
    //数组数据起始地址
    array->elts = ngx_palloc(pool, size * n);

    if(array->elts == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}
#endif //NGX_ARRAY_NGX_ARRAY_H
