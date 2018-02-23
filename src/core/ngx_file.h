/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_file.h
* @date:      2018/2/23 下午4:01
* @desc:
 * https://github.com/xieyugui/reading-code-of-nginx-1.9.2/blob/master/nginx-1.9.2/src/core/ngx_file.h
*/

//
// Created by daemon.xie on 2018/2/23.
//

#ifndef NGINX_LEARNING_NGX_FILE_H
#define NGINX_LEARNING_NGX_FILE_H


#include <ngx_config.h>
#include <ngx_core.h>

/*实际上，ngx_open_file与open方法的区别不大，ngx_open_file返回的是Linux系统的文件句柄。对于打开文件的标志位，Nginx也定义了以下几个宏来加以封装。
#define NGX_FILE_RDONLY O_RDONLY
#define NGX_FILE_WRONLY O_WRONLY
#define NGX_FILE_RDWR O_RDWR
#define NGX_FILE_CREATE_OR_OPEN O_CREAT
#define NGX_FILE_OPEN 0
#define NGX_FILE_TRUNCATE O_CREAT|O_TRUNC
#define NGX_FILE_APPEND O_WRONLY|O_APPEND
#define NGX_FILE_NONBLOCK O_NONBLOCK
#define NGX_FILE_DEFAULT_ACCESS 0644
#define NGX_FILE_OWNER_ACCESS 0600

　　│O_RDONLY│读文件 │
　　│O_WRONLY│写文件 │
　　│O_RDWR │即读也写 │
　　│O_NDELAY│没有使用；对UNIX系统兼容 │
　　│O_APPEND│即读也写，但每次写总是在文件尾添加 │
　　│O_CREAT │若文件存在，此标志无用；若不存在，建新文件 │
　　│O_TRUNC │若文件存在，则长度被截为0，属性不变 │
　　│O_EXCL │未用；对UNIX系统兼容 │
　　│O_BINARY│此标志可显示地给出以二进制方式打开文件 │
　　│O_TEXT │此标志可用于显示地给出以文本方式打开文件│

*/
struct ngx_file_s {
    ngx_fd_t                   fd; //文件句柄描述符
    ngx_str_t                  name;//文件名称
    ngx_file_info_t            info;//文件大小等资源信息，实际就是Linux系统定义的stat结构

    /* 该偏移量告诉Nginx现在处理到文件何处了，一般不用设置它，Nginx框架会根据当前发送状态设置它 */
    off_t                      offset;
    off_t                      sys_offset;

    ngx_log_t                 *log; //日志对象，相关的日志会输出到log指定的日志文件中

#if (NGX_THREADS || NGX_COMPAT)
    ngx_int_t                (*thread_handler)(ngx_thread_task_t *task,
                                               ngx_file_t *file);
    void                      *thread_ctx;
    ngx_thread_task_t         *thread_task;
#endif

#if (NGX_HAVE_FILE_AIO || NGX_COMPAT)
    ngx_event_aio_t           *aio;
#endif

    unsigned                   valid_info:1;
    /*
     of.is_directio只有在文件大小大于directio 512配置的大小时才会置1，见ngx_open_and_stat_file中会置1
     只有配置文件中有配置这几个模块相关配置，并且获取的文件大小(例如缓存文件)大于directio 512，也就是文件大小大于512时，则置1
     */
    unsigned                   directio:1;
};


#define NGX_MAX_PATH_LEVEL  3


typedef ngx_msec_t (*ngx_path_manager_pt) (void *data);
typedef ngx_msec_t (*ngx_path_purger_pt) (void *data);
typedef void (*ngx_path_loader_pt) (void *data);


typedef struct {
    ngx_str_t                  name; //路径名
    size_t                     len;//levels=x:y最终的结果是path->len = (x+1) + (y+1)

    /*
     levels=1:2，意思是说使用两级目录，第一级目录名是一个字符，第二级用两个字符。但是nginx最大支持3级目录，即levels=xxx:xxx:xxx。
     那么构成目录名字的字符哪来的呢？假设我们的存储目录为/cache，levels=1:2，那么对于上面的文件 就是这样存储的：
     /cache/0/8d/8ef9229f02c5672c747dc7a324d658d0  注意后面的8d0和cache后面的/0/8d一致  参考ngx_create_hashed_filename
    */
    size_t                     level[NGX_MAX_PATH_LEVEL];

    ngx_path_manager_pt        manager;
    ngx_path_purger_pt         purger;
    ngx_path_loader_pt         loader; //决定是否启用cache loader进程
    void                      *data;

    u_char                    *conf_file; //所在的配置文件
    ngx_uint_t                 line; //在配置文件中的行号
} ngx_path_t;


typedef struct {
    ngx_str_t                  name;
    size_t                     level[NGX_MAX_PATH_LEVEL];
} ngx_path_init_t;

//ngx_http_upstream_send_response中会创建ngx_temp_file_t
typedef struct {
    ngx_file_t                 file; //里面包括文件信息，fd 文件名等
    off_t                      offset; //指向写入到文件中的内容的最尾处
    ngx_path_t                *path; //文件路径
    ngx_pool_t                *pool;
    char                      *warn; //提示信息

    ngx_uint_t                 access; //文件权限 6660等  默认0600

    unsigned                   log_level:8; //日志等级
    unsigned                   persistent:1; //文件内容是否永久存储
    unsigned                   clean:1;//文件是临时的，关闭连接会删除文件
    unsigned                   thread_write:1;
} ngx_temp_file_t;


typedef struct {
    ngx_uint_t                 access;
    ngx_uint_t                 path_access;
    time_t                     time;
    ngx_fd_t                   fd;

    unsigned                   create_path:1;
    unsigned                   delete_file:1;

    ngx_log_t                 *log;
} ngx_ext_rename_file_t;


typedef struct {
    off_t                      size;
    size_t                     buf_size;

    ngx_uint_t                 access;
    time_t                     time;

    ngx_log_t                 *log;
} ngx_copy_file_t;


typedef struct ngx_tree_ctx_s  ngx_tree_ctx_t;

typedef ngx_int_t (*ngx_tree_init_handler_pt) (void *ctx, void *prev);
typedef ngx_int_t (*ngx_tree_handler_pt) (ngx_tree_ctx_t *ctx, ngx_str_t *name);

struct ngx_tree_ctx_s {
    off_t                      size;
    off_t                      fs_size;
    ngx_uint_t                 access;
    time_t                     mtime;

    ngx_tree_init_handler_pt   init_handler;
    ngx_tree_handler_pt        file_handler; //文件节点为普通文件时调用
    ngx_tree_handler_pt        pre_tree_handler; // 在递归进入目录节点时调用
    ngx_tree_handler_pt        post_tree_handler; // 在递归遍历完目录节点后调用
    ngx_tree_handler_pt        spec_handler; //文件节点为特殊文件时调用

    void                      *data;
    size_t                     alloc;

    ngx_log_t                 *log;
};


ngx_int_t ngx_get_full_name(ngx_pool_t *pool, ngx_str_t *prefix,
                            ngx_str_t *name);

ssize_t ngx_write_chain_to_temp_file(ngx_temp_file_t *tf, ngx_chain_t *chain);
ngx_int_t ngx_create_temp_file(ngx_file_t *file, ngx_path_t *path,
                               ngx_pool_t *pool, ngx_uint_t persistent, ngx_uint_t clean,
                               ngx_uint_t access);
void ngx_create_hashed_filename(ngx_path_t *path, u_char *file, size_t len);
ngx_int_t ngx_create_path(ngx_file_t *file, ngx_path_t *path);
ngx_err_t ngx_create_full_path(u_char *dir, ngx_uint_t access);
ngx_int_t ngx_add_path(ngx_conf_t *cf, ngx_path_t **slot);
ngx_int_t ngx_create_paths(ngx_cycle_t *cycle, ngx_uid_t user);
ngx_int_t ngx_ext_rename_file(ngx_str_t *src, ngx_str_t *to,
                              ngx_ext_rename_file_t *ext);
ngx_int_t ngx_copy_file(u_char *from, u_char *to, ngx_copy_file_t *cf);
ngx_int_t ngx_walk_tree(ngx_tree_ctx_t *ctx, ngx_str_t *tree);

ngx_atomic_uint_t ngx_next_temp_number(ngx_uint_t collision);

char *ngx_conf_set_path_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char *ngx_conf_merge_path_value(ngx_conf_t *cf, ngx_path_t **path,
                                ngx_path_t *prev, ngx_path_init_t *init);
char *ngx_conf_set_access_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


extern ngx_atomic_t      *ngx_temp_number;
extern ngx_atomic_int_t   ngx_random_number;

#endif //NGINX_LEARNING_NGX_FILE_H
