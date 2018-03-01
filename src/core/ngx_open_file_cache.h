/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_open_file_cache.h
* @date:      2018/2/24 上午10:50
* @desc:
 * https://github.com/xieyugui/reading-code-of-nginx-1.9.2/blob/master/nginx-1.9.2/src/core/ngx_open_file_cache.h
*/

//
// Created by daemon.xie on 2018/2/24.
//

#include <ngx_config.h>
#include <ngx_core.h>

#ifndef NGINX_LEARNING_NGX_OPEN_FILE_CACHE_H
#define NGINX_LEARNING_NGX_OPEN_FILE_CACHE_H

#define NGX_OPEN_FILE_DIRECTIO_OFF NGX_MAX_OFF_T_VALUE

//可以通过ngx_open_and_stat_file获取文件的相关属性信息
typedef struct {
    ngx_fd_t                 fd;
    ngx_file_uniq_t          uniq;
    time_t                   mtime; //文件最后被修改的时间
    off_t                    size;
    off_t                    fs_size;
    //取值是从ngx_http_core_loc_conf_s->directio  //在获取缓存文件内容的时候，只有文件大小大与等于directio的时候才会生效ngx_directio_on
    //默认NGX_OPEN_FILE_DIRECTIO_OFF是个超级大的值，相当于不使能
    off_t                    directio;
    size_t                   read_ahead;

    /*
    在ngx_file_info_wrapper中获取文件stat属性信息的时候，如果文件不存在或者open失败，或者stat失败，都会把错误放入这两个字段
    of->err = ngx_errno;
    of->failed = ngx_fd_info_n;
     */
    ngx_err_t                err;
    char                    *failed;
    //open_file_cache_valid 60S
    //表示60s后来的第一个请求要对文件stat信息做一次检查，检查是否发送变化，如果发送变化则从新获取文件stat信息或者从新创建该阶段，
    //生效在ngx_open_cached_file中的(&& now - file->created < of->valid )
    time_t                   valid;

    //在valid秒中没有使用到这个配置的次数的话就删除
    ngx_uint_t               min_uses;

#if (NGX_HAVE_OPENAT)
    size_t                   disable_symlinks_from;
    unsigned                 disable_symlinks:2;
#endif

    unsigned                 test_dir:1;
    unsigned                 test_only:1;
    unsigned                 log:1;
    unsigned                 errors:1;
    unsigned                 events:1;

    unsigned                 is_dir:1;
    unsigned                 is_file:1;
    unsigned                 is_link:1;
    unsigned                 is_exec:1;
    //注意这里如果文件大小大于direction设置，则置1，后面会使能direct I/O方式,生效见ngx_directio_on
    unsigned                 is_directio:1;
} ngx_open_file_info_t;

typedef struct ngx_cached_open_file_s ngx_cached_open_file_t;

//为什么需要内存中保存文件stat信息节点?因为这里面可以保存文件的fd已经文件大小等信息，就不用每次重复打开文件并且获取文件大小信息，可以直接读fd，这样可以提高效率
//ngx_open_cached_file中创建节点   主要存储的是文件的fstat信息
struct ngx_cached_open_file_s {
    //node.key是文件名做的 hash = ngx_crc32_long(name->data, name->len);
    // 文件名做hash添加到ngx_open_file_cache_t->rbtree红黑树中
    ngx_rbtree_node_t        node;
    ngx_queue_t              queue;

    u_char                  *name;
    time_t                   created;//该缓存文件对应的创建时间
    time_t                   accessed;//该阶段最近一次访问时间

    ngx_fd_t                 fd;
    ngx_file_uniq_t          uniq;
    time_t                   mtime;
    off_t                    size;
    ngx_err_t                err;

    //ngx_open_cached_file->ngx_open_file_lookup每次查找到有该文件，则增加1
    uint32_t                 uses;

#if (NGX_HAVE_OPENAT)
    size_t                   disable_symlinks_from;
    unsigned                 disable_symlinks:2;
#endif

    //只要有一个客户端r在使用该节点node，则不能释放该node节点
    unsigned                 count:24;//表示在引用该node节点的客户端个数
    //在ngx_expire_old_cached_files中从红黑树中移除节点后，会关闭文件，同时把close置1
    unsigned                 close:1;
    unsigned                 use_event:1;

    unsigned                 is_dir:1;
    unsigned                 is_file:1;
    unsigned                 is_link:1;
    unsigned                 is_exec:1;
    unsigned                 is_directio:1;

    ngx_event_t             *event;
};

typedef struct {
    ngx_rbtree_t             rbtree;
    ngx_rbtree_node_t        sentinel;
    //这个是用于过期快速判断用的，一般最尾部的最新过期，前面的红黑树rbtree一般是用于遍历查找的
    ngx_queue_t              expire_queue;

    ngx_uint_t               current;//红黑树和expire_queue队列中成员node个数
    ngx_uint_t               max;
    time_t                   inactive;
} ngx_open_file_cache_t;


typedef struct {
    ngx_open_file_cache_t   *cache;
    ngx_cached_open_file_t  *file;
    ngx_uint_t               min_uses;
    ngx_log_t               *log;
} ngx_open_file_cache_cleanup_t;


typedef struct {

    /* ngx_connection_t stub to allow use c->fd as event ident */
    void                    *data;
    ngx_event_t             *read;
    ngx_event_t             *write;
    ngx_fd_t                 fd;

    ngx_cached_open_file_t  *file;
    ngx_open_file_cache_t   *cache;
} ngx_open_file_cache_event_t;


ngx_open_file_cache_t *ngx_open_file_cache_init(ngx_pool_t *pool,
                                                ngx_uint_t max, time_t inactive);
ngx_int_t ngx_open_cached_file(ngx_open_file_cache_t *cache, ngx_str_t *name,
                               ngx_open_file_info_t *of, ngx_pool_t *pool);

#endif //NGINX_LEARNING_NGX_OPEN_FILE_CACHE_H
