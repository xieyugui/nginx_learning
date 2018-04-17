/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_palloc.h
* @date:      2017/11/28 下午4:14
* @desc:
 *
 * https://raw.githubusercontent.com/MeteorKL/nginx/master/ngx_pool.png
 *
 * 具体图可以参考该作者画的
 *
*/

//
// Created by daemon.xie on 2017/11/28.
//

#ifndef NGX_PALLOC_NGX_PALLOC_H
#define NGX_PALLOC_NGX_PALLOC_H

#include <ngx_config.h>
#include <ngx_core.h>

//每次能从pool分配的最大内存块大小，ngx_pagesize在X86下一般是4096，即4k，也就是说每次能从pool分配的最大内存块大小为4095字节，将近4k
#define  NGX_MAX_ALLOC_FROM_POOL (ngx_pagesize - 1)

//pool 默认大小
#define NGX_DEFAULT_POOL_SIZE (16 * 1024)

// 内存池的内存对齐值，即分配的内存大小是该值的倍数
#define NGX_POOL_ALIGNMENT 16

//TODO  ????
#define NGX_MIN_POOL_SIZE ngx_align((sizeof(ngx_pool_t) + 2 * sizeof(ngx_pool_large_t)),NGX_POOL_ALIGNMENT)

typedef void (*ngx_pool_cleanup_pt)(void *data);

typedef struct ngx_pool_cleanup_s ngx_pool_cleanup_t;

struct ngx_pool_cleanup_s {
    ngx_pool_cleanup_pt handler;
    void  *data;
    ngx_pool_cleanup_t *next;
};

typedef struct ngx_pool_large_s ngx_pool_large_t;

struct ngx_pool_large_s {/*大块内存数据结构*/
    /*其实是一个头插法的单链表，每次分配一个大块内存都将列表节点插入到这个链表的表头*/
    ngx_pool_large_t *next;
    void *alloc; /*大块内存是直接用malloc来分配的，alloc就是用来保存分配到的内存地址*/
};

/*一个内存池是由多个pool节点组成的链，这个结构用来链接各个pool节点和保存pool节点可用的内存区域起止地址*/
typedef struct {
    u_char *last; // 数据存储的已用区尾地址
    u_char *end; // 数据存储区的尾地址
    ngx_pool_t *next; // 下一个内存池地址
    ngx_uint_t failed; // 失败次数
} ngx_pool_data_t;

struct ngx_pool_s {
    ngx_pool_data_t d; // 数据区
    size_t  max; // 内存池的最大存储空间
    ngx_pool_t *current;  // 内存池
    ngx_chain_t *chain; //缓冲区链表
    ngx_pool_large_t *large; // 用于存储大数据，链表结构
    ngx_pool_cleanup_t *cleanup;  // 用于清理，链表结构
    ngx_log_t *log;
};

typedef struct {
    ngx_fd_t    fd; // 文件描述符，用于 ngx_pool_cleanup_file
    u_char  *name; // 文件名，用于 ngx_pool_delete_file
    ngx_log_t   *log;
} ngx_pool_cleanup_file_t;


void *ngx_alloc(size_t size, ngx_log_t *log);
void *ngx_calloc(size_t size, ngx_log_t *log);

//创建一个内存池
ngx_pool_t *ngx_create_pool(size_t size, ngx_log_t *log);
//销毁内存池 pool
void ngx_destroy_pool(ngx_pool_t *pool);
//重置内存池
void ngx_reset_pool(ngx_pool_t *pool);
// 从内存池 pool 分配大小为 size 的内存块，并返回其地址
// 是被外部使用最多的内存池相关 API，并且考虑对齐问题
void *ngx_palloc(ngx_pool_t *pool, size_t size);
//类似 ngx_palloc，不考虑对齐问题
void *ngx_pnalloc(ngx_pool_t *pool, size_t size);
//封装 palloc 为 pcalloc，实现分配内存并初始化为 0
void *ngx_pcalloc(ngx_pool_t *pool, size_t size);
//该方法就是ngx_palloc_large简单暴力版，直接申请ngx_pool_large_t并加入链表中
void *ngx_pmemalign(ngx_pool_t *pool, size_t size, size_t alignment);

//大内存块的释放
//内存池释放需要走ngx_destroy_pool，独立大内存块的单独释放，可以走ngx_pfree方法
ngx_int_t ngx_pfree(ngx_pool_t *pool, void *p);

//向 cleanup 链添加 p->cleanup 这个节点
ngx_pool_cleanup_t *ngx_pool_cleanup_add(ngx_pool_t *p, size_t size);

// 查找指定的 fd，且其 handler 为 ngx_pool_cleanup_file，执行相应动作,这里面有一个遍历的操作
void ngx_pool_run_cleanup_file(ngx_pool_t *p, ngx_fd_t fd);
// 释放文件描述符
void ngx_pool_cleanup_file(void *data);

//从文件系统删除文件，data 指针指向一个 ngx_pool_cleanup_file_t 类型的数据
void ngx_pool_delete_file(void *data);


#endif //NGX_PALLOC_NGX_PALLOC_H
