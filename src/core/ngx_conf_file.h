/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_conf_file.h
* @date:      2018/1/24 上午10:11
* @desc:
 *
 * https://github.com/xieyugui/reading-code-of-nginx-1.9.2/blob/master/nginx-1.9.2/src/core/ngx_conf_file.h
*/

//
// Created by daemon.xie on 2018/1/24.
//

#ifndef NGX_CONF_FILE_NGX_CONF_FILE_H
#define NGX_CONF_FILE_NGX_CONF_FILE_H

#include <ngx_config.h>
#include <ngx_core.h>

/*
以下这些宏用于限制配置项的参数个数
NGX_CONF_NOARGS：配置项不允许带参数
NGX_CONF_TAKE1：配置项可以带1个参数
NGX_CONF_TAKE2：配置项可以带2个参数
NGX_CONF_TAKE3：配置项可以带3个参数
NGX_CONF_TAKE4：配置项可以带4个参数
NGX_CONF_TAKE5：配置项可以带5个参数
NGX_CONF_TAKE6：配置项可以带6个参数
NGX_CONF_TAKE7：配置项可以带7个参数
NGX_CONF_TAKE12：配置项可以带1或2个参数
NGX_CONF_TAKE13：配置项可以带1或3个参数
NGX_CONF_TAKE23：配置项可以带2或3个参数
NGX_CONF_TAKE123：配置项可以带1-3个参数
NGX_CONF_TAKE1234：配置项可以带1-4个参数
*/
#define NGX_CONF_NOARGS      0x00000001
#define NGX_CONF_TAKE1       0x00000002
#define NGX_CONF_TAKE2       0x00000004
#define NGX_CONF_TAKE3       0x00000008
#define NGX_CONF_TAKE4       0x00000010
#define NGX_CONF_TAKE5       0x00000020
#define NGX_CONF_TAKE6       0x00000040
#define NGX_CONF_TAKE7       0x00000080

#define NGX_CONF_MAX_ARGS    8

#define NGX_CONF_TAKE12      (NGX_CONF_TAKE1|NGX_CONF_TAKE2)
#define NGX_CONF_TAKE13      (NGX_CONF_TAKE1|NGX_CONF_TAKE3)

#define NGX_CONF_TAKE23      (NGX_CONF_TAKE2|NGX_CONF_TAKE3)

#define NGX_CONF_TAKE123     (NGX_CONF_TAKE1|NGX_CONF_TAKE2|NGX_CONF_TAKE3)
#define NGX_CONF_TAKE1234    (NGX_CONF_TAKE1|NGX_CONF_TAKE2|NGX_CONF_TAKE3   \
                              |NGX_CONF_TAKE4)

/*
以下这些宏用于限制配置项参数形式
NGX_CONF_BLOCK：配置指令可以接受的值是一个配置信息块。也就是一对大括号括起来的内容。里面可以再包括很多的配置指令。比如常见的server指令就是这个属性的
NGX_CONF_ANY：不验证配置项携带的参数个数。
NGX_CONF_FLAG：配置项只能带一个参数，并且参数必需是on或者off。
NGX_CONF_1MORE：配置项携带的参数必需超过一个。
NGX_CONF_2MORE：配置项携带的参数必需超过二个。
*/
#define NGX_CONF_ARGS_NUMBER 0x000000ff
#define NGX_CONF_BLOCK       0x00000100
#define NGX_CONF_FLAG        0x00000200
#define NGX_CONF_ANY         0x00000400
#define NGX_CONF_1MORE       0x00000800
#define NGX_CONF_2MORE       0x00001000

//支持NGX_MAIN_CONF | NGX_DIRECT_CONF的包括:
//ngx_core_commands  ngx_openssl_commands  ngx_google_perftools_commands   ngx_regex_commands  ngx_thread_pool_commands
//可以出现在配置文件中最外层。例如已经提供的配置指令daemon，master_process等
#define NGX_DIRECT_CONF      0x00010000

/*
总结，一般一级配置(http{}外的配置项)一般属性包括NGX_MAIN_CONF|NGX_DIRECT_CONF。http events等这一行的配置属性,
 全局配置项worker_priority等也属于这个行列
http、mail、events、error_log等
*/
#define NGX_MAIN_CONF        0x01000000

/*
该配置指令可以出现在任意配置级别上
*/
#define NGX_ANY_CONF         0x1F000000


#define NGX_CONF_UNSET       -1
#define NGX_CONF_UNSET_UINT  (ngx_uint_t) -1
#define NGX_CONF_UNSET_PTR   (void *) -1
#define NGX_CONF_UNSET_SIZE  (size_t) -1
#define NGX_CONF_UNSET_MSEC  (ngx_msec_t) -1

#define NGX_CONF_OK          NULL
#define NGX_CONF_ERROR       (void *) -1

/*
遇到'{'表示新的block的开始,列如
if (ch == '{') {
    return NGX_CONF_BLOCK_START;
}
*/
#define NGX_CONF_BLOCK_START 1
#define NGX_CONF_BLOCK_DONE  2
/*如果偏移量大于文件大小,说明文件已读取完毕，返回NGX_CONF_FILE_DONE*/
#define NGX_CONF_FILE_DONE   3

//GX_CORE_MODULE类型的核心模块解析配置项时，配置项一定是全局的，
/*
NGX_CORE_MODULE主要包括以下模块:
ngx_core_module  ngx_events_module  ngx_http_module  ngx_errlog_module  ngx_mail_module
ngx_regex_module  ngx_stream_module  ngx_thread_pool_module
*/
//所有的核心模块NGX_CORE_MODULE对应的上下文ctx为ngx_core_module_t，子模块，例如http{} NGX_HTTP_MODULE模块
// 对应的为上下文为ngx_http_module_t
//events{} NGX_EVENT_MODULE模块对应的为上下文为ngx_event_module_t
/*
Nginx还定义了一种基础类型的模块：核心模块，它的模块类型叫做NGX_CORE_MODULE。目前官方的核心类型模块中共有6个具体模块，分别
是ngx_core_module、ngx_errlog_module、ngx_events_module、ngx_openssl_module、ngx_http_module、ngx_mail_module模块
*/
#define NGX_CORE_MODULE      0x45524F43  /* "CORE" */

#define NGX_CONF_MODULE      0x464E4F43  /* "CONF" */

#define NGX_MAX_CONF_ERRSTR  1024

/*
commands数组用于定义模块的配置文件参数，每一个数组元素都是ngx_command_t类型，数组的结尾用ngx_null_command表示。Nginx在解析配置
文件中的一个配置项时首先会遍历所有的模块，对于每一个模块而言，即通过遍历commands数组进行，另外，在数组中检查到ngx_null_command时，
会停止使用当前模块解析该配置项。每一个ngx_command_t结构体定义了自己感兴趣的一个配置项：
typedef struct ngx_command_s     ngx_command_t;
 每个module都有自己的command，见ngx_modules中对应模块的command。 每个进程中都有一个唯一的ngx_cycle_t核心结构体，
 它有一个成员conf_ctx维护着所有模块的配置结构体
*/
struct ngx_command_s {
    ngx_str_t name; //配置项名称，如"gzip"
    /*
     type决定这个配置项可以在哪些块（如http、server、location、if、upstream块等）
        中出现，以及可以携带的参数类型和个数等。
        注意，type可以同时取多个值，各值之间用|符号连接，例如，type可以取
        值为NGX_TTP_MAIN_CONF | NGX_HTTP_SRV_CONFI | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE。
     */
    ngx_uint_t type; //取值可能为NGX_HTTP_LOC_CONF | NGX_CONF_TAKE2等
    //cf里面存储的是从配置文件里面解析出的内容，conf是最终用来存储解析内容的内存空间，cmd为存到空间的那个地方(使用偏移量来衡量)
    char *(*set)(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);/* 处理读取的数据  */
    //crate分配内存的时候的偏移量 NGX_HTTP_LOC_CONF_OFFSET NGX_HTTP_SRV_CONF_OFFSET
    ngx_uint_t conf; /* 使用哪块内存池  */
    /*通常用于使用预设的解析方法解析配置项，这是配置模块的一个优秀设计。它需要与conf配合使用*/
    ngx_uint_t offset; /* 配置项的精确存放位置  */
    //一个指针 可以指向任何一个在读取配置过程中需要的数据，以便于进行配置读取的处理
    void *post;
};

//ngx_null_command只是一个空的ngx_command_t，表示模块的命令数组解析完毕，如下所示：
#define ngx_null_command  { ngx_null_string, 0, NULL, 0, 0, NULL }

//定义了打开文件的参数的结构体
struct ngx_open_file_s {
    ngx_fd_t              fd; //文件描述符
    ngx_str_t             name; //文件名称
    //文件操作函数指针
    void                (*flush)(ngx_open_file_t *file, ngx_log_t *log);
    void                 *data; //要写入的文件的数据缓冲区
};

//定义了缓存配置文件的数据的结构体 表示将要解析的配置文件
typedef struct {
    ngx_file_t            file; //配置文件名
    ngx_buf_t            *buffer; //文件内容在这里面存储
    //当在解析从文件中读取到的4096字节内存时，如果最后面的内存不足以构成一个token，
    //则把这部分内存零时存起来，然后拷贝到下一个4096内存的头部参考ngx_conf_read_token
    ngx_buf_t            *dump;
    ngx_uint_t            line; //在配置文件中的行号  可以参考ngx_thread_pool_add
} ngx_conf_file_t;

typedef struct {
    ngx_str_t             name;
    ngx_buf_t            *buffer;
} ngx_conf_dump_t;

//模块自定义的handler
typedef char *(*ngx_conf_handler_pt)(ngx_conf_t *cf,
                                     ngx_command_t *dummy, void *conf);

struct ngx_conf_s {
    char *name; //存放当前解析到的指令
    ngx_array_t *args; //每解析一行，从配置文件中解析出的配置项全部在这里面 存放该指令包含的所有参数
    //最终指向的是一个全局类型的ngx_cycle_s，即ngx_cycle，见ngx_init_cycle
    //指向对应的cycle，见ngx_init_cycle中的两行conf.ctx = cycle->conf_ctx; conf.cycle = cycle;
    ngx_cycle_t *cycle;
    ngx_pool_t *pool;
    //用该pool的空间都是临时空间，最终在ngx_init_cycle->ngx_destroy_pool(conf.temp_pool);中释放
    ngx_pool_t *temp_pool;
    ngx_conf_file_t *conf_file; //nginx.conf
    ngx_log_t *log; //描述日志文件的相关属性

    //指向ngx_cycle_t->conf_ctx 有多少个模块，就有多少个ctx指针数组成员  conf.ctx = cycle->conf_ctx;见ngx_init_cycle
    //这个ctx每次在在进入对应的server{}  location{}前都会指向零时保存父级的ctx，该{}解析完后在恢复到父的ctx。
    // 可以参考ngx_http_core_server，ngx_http_core_location
    void *ctx;//指向结构ngx_http_conf_ctx_t  ngx_core_module_t ngx_event_module_t ngx_stream_conf_ctx_t等
    //表示当前配置项是属于那个大类模块 取值有如下5种：NGX_HTTP_MODULE、NGX_CORE_MODULE、NGX_CONF_MODULE、NGX_EVENT_MODULE、NGX_MAIL_MODULE
    ngx_uint_t module_type;
    //大类里面的那个子类模块,如NGX_HTTP_SRV_CONF NGX_HTTP_LOC_CONF等
    ngx_uint_t cmd_type;

    ngx_conf_handler_pt handler; //指令自定义的处理函数
    void *handler_conf; //自定义处理函数需要的相关配置
};

typedef char *(*ngx_conf_post_handler_pt) (ngx_conf_t *cf,
                                           void *data, void *conf);

typedef struct {
    ngx_conf_post_handler_pt  post_handler;
} ngx_conf_post_t;

typedef struct {
    ngx_conf_post_handler_pt  post_handler;
    char                     *old_name;
    char                     *new_name;
} ngx_conf_deprecated_t;

typedef struct {
    ngx_conf_post_handler_pt  post_handler;
    ngx_int_t                 low;
    ngx_int_t                 high;
} ngx_conf_num_bounds_t;

typedef struct {
    ngx_str_t                 name;
    ngx_uint_t                value;
} ngx_conf_enum_t;

#define NGX_CONF_BITMASK_SET  1

typedef struct {
    ngx_str_t                 name;
    ngx_uint_t                mask;
} ngx_conf_bitmask_t;

char * ngx_conf_deprecated(ngx_conf_t *cf, void *post, void *data);
char *ngx_conf_check_num_bounds(ngx_conf_t *cf, void *post, void *data);

#define ngx_get_conf(conf_ctx, module)  conf_ctx[module.index]


#define ngx_conf_init_value(conf, default)                                   \
    if (conf == NGX_CONF_UNSET) {                                            \
        conf = default;                                                      \
    }

#define ngx_conf_init_ptr_value(conf, default)                               \
    if (conf == NGX_CONF_UNSET_PTR) {                                        \
        conf = default;                                                      \
    }

#define ngx_conf_init_uint_value(conf, default)                              \
    if (conf == NGX_CONF_UNSET_UINT) {                                       \
        conf = default;                                                      \
    }

#define ngx_conf_init_size_value(conf, default)                              \
    if (conf == NGX_CONF_UNSET_SIZE) {                                       \
        conf = default;                                                      \
    }

#define ngx_conf_init_msec_value(conf, default)                              \
    if (conf == NGX_CONF_UNSET_MSEC) {                                       \
        conf = default;                                                      \
    }

#define ngx_conf_merge_value(conf, prev, default)                            \
    if (conf == NGX_CONF_UNSET) {                                            \
        conf = (prev == NGX_CONF_UNSET) ? default : prev;                    \
    }

#define ngx_conf_merge_ptr_value(conf, prev, default)                        \
    if (conf == NGX_CONF_UNSET_PTR) {                                        \
        conf = (prev == NGX_CONF_UNSET_PTR) ? default : prev;                \
    }

#define ngx_conf_merge_uint_value(conf, prev, default)                       \
    if (conf == NGX_CONF_UNSET_UINT) {                                       \
        conf = (prev == NGX_CONF_UNSET_UINT) ? default : prev;               \
    }


#define ngx_conf_merge_msec_value(conf, prev, default)                       \
    if (conf == NGX_CONF_UNSET_MSEC) {                                       \
        conf = (prev == NGX_CONF_UNSET_MSEC) ? default : prev;               \
    }

#define ngx_conf_merge_sec_value(conf, prev, default)                        \
    if (conf == NGX_CONF_UNSET) {                                            \
        conf = (prev == NGX_CONF_UNSET) ? default : prev;                    \
    }

#define ngx_conf_merge_size_value(conf, prev, default)                       \
    if (conf == NGX_CONF_UNSET_SIZE) {                                       \
        conf = (prev == NGX_CONF_UNSET_SIZE) ? default : prev;               \
    }

#define ngx_conf_merge_off_value(conf, prev, default)                        \
    if (conf == NGX_CONF_UNSET) {                                            \
        conf = (prev == NGX_CONF_UNSET) ? default : prev;                    \
    }

#define ngx_conf_merge_str_value(conf, prev, default)                        \
    if (conf.data == NULL) {                                                 \
        if (prev.data) {                                                     \
            conf.len = prev.len;                                             \
            conf.data = prev.data;                                           \
        } else {                                                             \
            conf.len = sizeof(default) - 1;                                  \
            conf.data = (u_char *) default;                                  \
        }                                                                    \
    }

#define ngx_conf_merge_bufs_value(conf, prev, default_num, default_size)     \
    if (conf.num == 0) {                                                     \
        if (prev.num) {                                                      \
            conf.num = prev.num;                                             \
            conf.size = prev.size;                                           \
        } else {                                                             \
            conf.num = default_num;                                          \
            conf.size = default_size;                                        \
        }                                                                    \
    }

#define ngx_conf_merge_bitmask_value(conf, prev, default)                    \
    if (conf == 0) {                                                         \
        conf = (prev == 0) ? default : prev;                                 \
    }

//ngx_conf_param是基于ngx_conf_parse实现的
//ngx_conf_param负责解析nginx命令行参数’-g’加入的配置。ngx_conf_parse负责解析nginx配置文件
char *ngx_conf_param(ngx_conf_t *cf);
/**
　　1、获取配置文件。
　　2、保存当前配置文件的上下文，并将cf->conf_file 指向当前配置文件。
　　3、读取当前配置文件中的配置指令名 ngx_conf_read_token 。
　　4、判断读取指令的类别、是否正确。
　　5、执行指令前是否进行其他处理。
　　6、交给 ngx_conf_handler 处理指令。
　　7、全部执行完后，恢复上下文。
 */
char *ngx_conf_parse(ngx_conf_t *cf, ngx_str_t *filename);
char *ngx_conf_include(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

//调用ngx_conf_full_name()函数初始化pid，实际上就是在pid字符串前加上NGX_PREFIX获取pid全路径
ngx_int_t ngx_conf_full_name(ngx_cycle_t *cycle, ngx_str_t *name,
                             ngx_uint_t conf_prefix);
//open_files链表
ngx_open_file_t *ngx_conf_open_file(ngx_cycle_t *cycle, ngx_str_t *name);
//记录错误日志，并指向配置对象指针
void ngx_cdecl ngx_conf_log_error(ngx_uint_t level, ngx_conf_t *cf,
                                  ngx_err_t err, const char *fmt, ...);

//将“on”或者“off”转换成1或0；读取NGX_CONF_FLAG类型的参数
char *ngx_conf_set_flag_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
//读取字符串类型的参数
char *ngx_conf_set_str_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
//读取字符串数组类型的参数
char *ngx_conf_set_str_array_slot(ngx_conf_t *cf, ngx_command_t *cmd,
                                  void *conf);
//读取键值对类型的参数
char *ngx_conf_set_keyval_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
//读取整数类型(有符号整数ngx_int_t)的参数
char *ngx_conf_set_num_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
//读取size_t类型的参数，也就是无符号数
char *ngx_conf_set_size_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
//读取off_t类型的参数
char *ngx_conf_set_off_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
//读取毫秒值类型的参数
char *ngx_conf_set_msec_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
//读取秒值类型的参数
char *ngx_conf_set_sec_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
//读取的参数值是2个，一个是buf的个数，一个是buf的大小。例如： output_buffers 1 128k;
char *ngx_conf_set_bufs_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
//读取枚举类型的参数，将其转换成整数ngx_uint_t类型
char *ngx_conf_set_enum_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
//读取参数的值，并将这些参数的值以bit位的形式存储。例如：HttpDavModule模块的dav_methods指令
char *ngx_conf_set_bitmask_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

//记录模块数，每个模块用唯一的index区别
extern ngx_uint_t     ngx_max_module;
extern ngx_module_t  *ngx_modules[];

#endif //NGX_CONF_FILE_NGX_CONF_FILE_H
