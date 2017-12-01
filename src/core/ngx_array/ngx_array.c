/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_array.c
* @date:      2017/12/1 下午3:20
* @desc:
*/

//
// Created by daemon.xie on 2017/12/1.
//

#include <ngx_config.h>
#include <ngx_core.h>

ngx_arr_t *
ngx_array_create(ngx_pool_t *p, ngx_uint_t n, size_t size)
{
    ngx_array_t *a;
    a = ngx_palloc(p, sizeof(ngx_array_t));
    if (a == NULL) {
        return NULL;
    }

    if(ngx_array_init(a, p, n, size) != NGX_OK) {
        return NULL;
    }

    return a;
}

void
ngx_array_destroy(ngx_array_t *a)
{
    ngx_pool_t *p;

    p = a->pool;

    //1：销毁数组存储元素的内存，即数据区的内存
    if((u_char *) a->elts + a->size * a->nalloc == p->d.last) {
        p->d.last -= a->size * a->nalloc;
    }
    //2：销毁数组本身的内存，即结构体array本身的内存
    if((u_char *) a + sizeof(ngx_array_t) == p->d.last) {
        p->d.last = (u_char *)a;
    }
}

void *
ngx_array_push(ngx_array_t *a)
{
    void *elt, *new;
    size_t size;
    ngx_pool_t *p;

    //数组已满
    if (a->nelts == a->nalloc) {
        size = a->size * a->nalloc;

        p = a->pool;
        //如果p的剩余空间>=一个数组元素的空间，就分配一个空间给数组
        if((u_char *) a->elts + size == p->d.last
           && p->d.last + a->size <= p->d.end) {
            //调整pool的last，即修改下一次可分配空间的其实地址
            p->d.last += a->size;
            a->nalloc++;
        } else {
            //申请新的空间，大小是原来的2倍，假如pool的内存不足够分配一个新的数组元素
            new = ngx_palloc(p, 2 * size);
            if(new == NULL) {
                return NULL;
            }
            //把原有的元素拷贝到新分配的内存区
            ngx_memcpy(new, a->elts, size);
            a->elts = new;
            a->nalloc *= 2;
        }
    }
    //新增加元素的地址
    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts++;//数组中元素的个数加1
    //返回新增加元素的地址
    return elt;
}

void *
ngx_array_push_n(ngx_array_t *a, ngx_uint_t n)
{
    void        *elt, *new;
    size_t       size;
    ngx_uint_t   nalloc;
    ngx_pool_t  *p;

    size = n * a->size;

    if (a->nelts + n > a->nalloc) {

        /* the array is full */

        p = a->pool;

        if ((u_char *) a->elts + a->size * a->nalloc == p->d.last
            && p->d.last + size <= p->d.end)
        {
            /*
             * the array allocation is the last in the pool
             * and there is space for new allocation
             */

            p->d.last += size;
            a->nalloc += n;

        } else {
            /* allocate a new array */

            nalloc = 2 * ((n >= a->nalloc) ? n : a->nalloc);

            new = ngx_palloc(p, nalloc * a->size);
            if (new == NULL) {
                return NULL;
            }

            ngx_memcpy(new, a->elts, a->nelts * a->size);
            a->elts = new;
            a->nalloc = nalloc;
        }
    }

    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts += n;

    return elt;
}