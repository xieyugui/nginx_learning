/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_radix_tree.h
* @date:      2017/12/27 下午2:49
* @desc:
*/

//
// Created by daemon.xie on 2017/12/27.
//

#ifndef NGX_RADIX_TREE_NGX_RADIX_TREE_H
#define NGX_RADIX_TREE_NGX_RADIX_TREE_H

#include <ngx_config.h>
#include <ngx_core.h>

#define NGX_RADIX_NO_VALUE (uintptr_t) -1

typedef struct ngx_radix_node_s ngx_radix_node_t;
//基数树的节点
struct ngx_radix_node_s {
    ngx_radix_node_t *right;
    ngx_radix_node_t *left;
    ngx_radix_node_t *parent;
    uintptr_t        value; //指向存储数据的指针
};

typedef struct {
    ngx_radix_node_t *root; //根节点
    ngx_pool_t *pool; //内存池，负责分配内存
    ngx_radix_node_t *free; //回收释放的节点，在添加新节点时，会首先查看free中是否有空闲可用的节点
    char *start; //已分配内存中还未使用内存的首地址
    size_t size; //已分配内存内中还未使用内存的大小
} ngx_radix_tree_t;

//创建基数树，preallocate是预分配树的层数
ngx_radix_tree_t *ngx_radix_tree_create(ngx_pool_t *pool, ngx_int_t preallocate);

//根据key值和掩码向基数树中插入value,返回值可能是NGX_OK,NGX_ERROR, NGX_BUSY
ngx_int_t ngx_radix32tree_insert(ngx_radix_tree_t *tree, uint32_t key, uint32_t mask,
    uintptr_t value);

//根据key值和掩码删除节点（value的值）
ngx_int_t ngx_radix32tree_delete(ngx_radix_tree_t *tree,
                                 uint32_t key, uint32_t mask);

//根据key值在基数树中查找返回value数据
uintptr_t ngx_radix32tree_find(ngx_radix_tree_t *tree, uint32_t key);

#if (NGX_HAVE_INET6)
ngx_int_t ngx_radix128tree_insert(ngx_radix_tree_t *tree,
                                  u_char *key, u_char *mask, uintptr_t value);
ngx_int_t ngx_radix128tree_delete(ngx_radix_tree_t *tree,
                                  u_char *key, u_char *mask);
uintptr_t ngx_radix128tree_find(ngx_radix_tree_t *tree, u_char *key);
#endif

#endif //NGX_RADIX_TREE_NGX_RADIX_TREE_H
