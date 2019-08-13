/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui
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
    off_t                      offset;//记录文件偏移，表示已经读或者写到文件的位置
    off_t                      sys_offset; //默认情况等于offset，如果不相等，会被强制修改为offset值

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
    size_t                     level[NGX_MAX_PATH_LEVEL];//每一层子目录的长度

    //文件缓存管理回调，为ngx_http_file_cache_manager，在ngx_cache_manager_process_handler中被调用
    ngx_path_manager_pt        manager;
    ngx_path_purger_pt         purger;
    //文件缓存loader回调，为ngx_http_file_cache_loader，在ngx_cache_loader_process_handler中被调用
    ngx_path_loader_pt         loader; //决定是否启用cache loader进程
    void                      *data;//ngx_http_file_cache_t文件缓存对象,回调上下文

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
    ngx_path_t                *path; //临时文件所对应的路径
    ngx_pool_t                *pool;
    char                      *warn; //提示信息

    ngx_uint_t                 access; //文件权限 6660等  默认0600

    unsigned                   log_level:8; //日志等级
    unsigned                   persistent:1; //是否需要对当前临时文件进行持久化
    unsigned                   clean:1;//退出时，临时文件是否要删除
    unsigned                   thread_write:1;//是否开启多线程写
} ngx_temp_file_t;


typedef struct {
    ngx_uint_t                 access;
    ngx_uint_t                 path_access; //路径访问权限
    time_t                     time; //重命名时间
    ngx_fd_t                   fd;

    unsigned                   create_path:1; //是否创建路径
    unsigned                   delete_file:1; //如果重命名失败，是否要删除文件

    ngx_log_t                 *log;
} ngx_ext_rename_file_t;


typedef struct {
    off_t                      size; //要拷贝的文件大小
    size_t                     buf_size; //拷贝时新开临时缓冲区大小

    ngx_uint_t                 access;
    time_t                     time; //新拷贝文件的最近访问时间和最近修改时间

    ngx_log_t                 *log;
} ngx_copy_file_t;


typedef struct ngx_tree_ctx_s  ngx_tree_ctx_t;

typedef ngx_int_t (*ngx_tree_init_handler_pt) (void *ctx, void *prev);
typedef ngx_int_t (*ngx_tree_handler_pt) (ngx_tree_ctx_t *ctx, ngx_str_t *name);

struct ngx_tree_ctx_s {
    off_t                      size; //遍历到的文件的大小
    off_t                      fs_size; //指的是遍历到的文件所占磁盘块数目乘以512的值与size中的最大值，即fs_size = ngx_max(size,st_blocks*512)
    ngx_uint_t                 access;
    time_t                     mtime; //指的是遍历到的文件上次被修改的时间

    ngx_tree_init_handler_pt   init_handler;
    ngx_tree_handler_pt        file_handler; //文件节点为普通文件时调用
    ngx_tree_handler_pt        pre_tree_handler; // 进入一个目录前的回调函数
    ngx_tree_handler_pt        post_tree_handler; //离开一个目录后的回调函数
    ngx_tree_handler_pt        spec_handler; //处理特殊文件的回调函数，比如socket、FIFO等

    void                      *data;//传递一些数据结构，可以在不同的目录下使用相同的数据结构，或者也可以重新分配，前提是alloc不为0
    size_t                     alloc;

    ngx_log_t                 *log;
};

//获得某个文件的全路径
ngx_int_t ngx_get_full_name(ngx_pool_t *pool, ngx_str_t *prefix,
                            ngx_str_t *name);
//将buffer chain写入到temp文件
ssize_t ngx_write_chain_to_temp_file(ngx_temp_file_t *tf, ngx_chain_t *chain);
//创建temp文件
ngx_int_t ngx_create_temp_file(ngx_file_t *file, ngx_path_t *path,
                               ngx_pool_t *pool, ngx_uint_t persistent, ngx_uint_t clean,
                               ngx_uint_t access);
//创建hash文件名
void ngx_create_hashed_filename(ngx_path_t *path, u_char *file, size_t len);
//创建路径
ngx_int_t ngx_create_path(ngx_file_t *file, ngx_path_t *path);
//创建全路径
ngx_err_t ngx_create_full_path(u_char *dir, ngx_uint_t access);
//添加路径到cf->cycle->paths数组中
ngx_int_t ngx_add_path(ngx_conf_t *cf, ngx_path_t **slot);
//创建cycle->paths数组中的所有路径
ngx_int_t ngx_create_paths(ngx_cycle_t *cycle, ngx_uid_t user);
//重命名文件
ngx_int_t ngx_ext_rename_file(ngx_str_t *src, ngx_str_t *to,
                              ngx_ext_rename_file_t *ext);
//拷贝文件
ngx_int_t ngx_copy_file(u_char *from, u_char *to, ngx_copy_file_t *cf);
//遍历某一目录下(tree目录)的文件
ngx_int_t ngx_walk_tree(ngx_tree_ctx_t *ctx, ngx_str_t *tree);

//获得下一个临时number值
ngx_atomic_uint_t ngx_next_temp_number(ngx_uint_t collision);

//设置某一模块中的路径指令
char *ngx_conf_set_path_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
//合并路径值
char *ngx_conf_merge_path_value(ngx_conf_t *cf, ngx_path_t **path,
                                ngx_path_t *prev, ngx_path_init_t *init);
//设置某一个文件的所有者、所属组、其他人的访问权限
char *ngx_conf_set_access_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

//全局的临时值变量，主要用于产生临时文件名使用
extern ngx_atomic_t      *ngx_temp_number;
//nginx内部维持的一个随机值变量
extern ngx_atomic_int_t   ngx_random_number;

#endif //NGINX_LEARNING_NGX_FILE_H
