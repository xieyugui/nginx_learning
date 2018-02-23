/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_module.h
* @date:      2018/2/23 下午5:27
* @desc:
 * https://github.com/chronolaw/annotated_nginx/blob/master/nginx/src/core/ngx_module.c
*/

//
// Created by daemon.xie on 2018/2/23.
//

#ifndef NGINX_LEARNING_NGX_MODULE_H
#define NGINX_LEARNING_NGX_MODULE_H

// 提取了部分ngx_conf_file.h里的模块数据结构代码
// 加上了新的动态模块代码
//
// 不需要我们为模块增加特别的代码
// nginx使用脚本处理动态模块相关的数据
// 会自动为动态模块生成一个用于编译的c源码文件
// 编译使用make modules

#include <ngx_config.h>
#include <ngx_core.h>
#include <nginx.h>

// 无效模块序号，同样使用-1
#define NGX_MODULE_UNSET_INDEX  (ngx_uint_t) -1


// 定义动态模块的签名，保证可以正确加载
// 使用了指针大小等宏，保证二进制一致性
#define NGX_MODULE_SIGNATURE_0                                                \
    ngx_value(NGX_PTR_SIZE) ","                                               \
    ngx_value(NGX_SIG_ATOMIC_T_SIZE) ","                                      \
    ngx_value(NGX_TIME_T_SIZE) ","

// 定义各种系统调用的支持程度
// 只有完全匹配即系统符合要求才能加载

#if (NGX_HAVE_KQUEUE)
#define NGX_MODULE_SIGNATURE_1   "1"
#else
#define NGX_MODULE_SIGNATURE_1   "0"
#endif

#if (NGX_HAVE_IOCP)
#define NGX_MODULE_SIGNATURE_2   "1"
#else
#define NGX_MODULE_SIGNATURE_2   "0"
#endif

#if (NGX_HAVE_FILE_AIO || NGX_COMPAT)
#define NGX_MODULE_SIGNATURE_3   "1"
#else
#define NGX_MODULE_SIGNATURE_3   "0"
#endif

#if (NGX_HAVE_AIO_SENDFILE || NGX_COMPAT)
#define NGX_MODULE_SIGNATURE_4   "1"
#else
#define NGX_MODULE_SIGNATURE_4   "0"
#endif

#if (NGX_HAVE_EVENTFD)
#define NGX_MODULE_SIGNATURE_5   "1"
#else
#define NGX_MODULE_SIGNATURE_5   "0"
#endif

#if (NGX_HAVE_EPOLL)
#define NGX_MODULE_SIGNATURE_6   "1"
#else
#define NGX_MODULE_SIGNATURE_6   "0"
#endif

#if (NGX_HAVE_KEEPALIVE_TUNABLE)
#define NGX_MODULE_SIGNATURE_7   "1"
#else
#define NGX_MODULE_SIGNATURE_7   "0"
#endif

#if (NGX_HAVE_INET6)
#define NGX_MODULE_SIGNATURE_8   "1"
#else
#define NGX_MODULE_SIGNATURE_8   "0"
#endif

#define NGX_MODULE_SIGNATURE_9   "1"
#define NGX_MODULE_SIGNATURE_10  "1"

#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
#define NGX_MODULE_SIGNATURE_11  "1"
#else
#define NGX_MODULE_SIGNATURE_11  "0"
#endif

#define NGX_MODULE_SIGNATURE_12  "1"

#if (NGX_HAVE_SETFIB)
#define NGX_MODULE_SIGNATURE_13  "1"
#else
#define NGX_MODULE_SIGNATURE_13  "0"
#endif

#if (NGX_HAVE_TCP_FASTOPEN)
#define NGX_MODULE_SIGNATURE_14  "1"
#else
#define NGX_MODULE_SIGNATURE_14  "0"
#endif

#if (NGX_HAVE_UNIX_DOMAIN)
#define NGX_MODULE_SIGNATURE_15  "1"
#else
#define NGX_MODULE_SIGNATURE_15  "0"
#endif

#if (NGX_HAVE_VARIADIC_MACROS)
#define NGX_MODULE_SIGNATURE_16  "1"
#else
#define NGX_MODULE_SIGNATURE_16  "0"
#endif

#define NGX_MODULE_SIGNATURE_17  "0"
#define NGX_MODULE_SIGNATURE_18  "0"

#if (NGX_HAVE_OPENAT)
#define NGX_MODULE_SIGNATURE_19  "1"
#else
#define NGX_MODULE_SIGNATURE_19  "0"
#endif

#if (NGX_HAVE_ATOMIC_OPS)
#define NGX_MODULE_SIGNATURE_20  "1"
#else
#define NGX_MODULE_SIGNATURE_20  "0"
#endif

#if (NGX_HAVE_POSIX_SEM)
#define NGX_MODULE_SIGNATURE_21  "1"
#else
#define NGX_MODULE_SIGNATURE_21  "0"
#endif

#if (NGX_THREADS || NGX_COMPAT)
#define NGX_MODULE_SIGNATURE_22  "1"
#else
#define NGX_MODULE_SIGNATURE_22  "0"
#endif

#if (NGX_PCRE)
#define NGX_MODULE_SIGNATURE_23  "1"
#else
#define NGX_MODULE_SIGNATURE_23  "0"
#endif

#if (NGX_HTTP_SSL || NGX_COMPAT)
#define NGX_MODULE_SIGNATURE_24  "1"
#else
#define NGX_MODULE_SIGNATURE_24  "0"
#endif

#define NGX_MODULE_SIGNATURE_25  "1"

#if (NGX_HTTP_GZIP)
#define NGX_MODULE_SIGNATURE_26  "1"
#else
#define NGX_MODULE_SIGNATURE_26  "0"
#endif

#define NGX_MODULE_SIGNATURE_27  "1"

#if (NGX_HTTP_X_FORWARDED_FOR)
#define NGX_MODULE_SIGNATURE_28  "1"
#else
#define NGX_MODULE_SIGNATURE_28  "0"
#endif

#if (NGX_HTTP_REALIP)
#define NGX_MODULE_SIGNATURE_29  "1"
#else
#define NGX_MODULE_SIGNATURE_29  "0"
#endif

#if (NGX_HTTP_HEADERS)
#define NGX_MODULE_SIGNATURE_30  "1"
#else
#define NGX_MODULE_SIGNATURE_30  "0"
#endif

#if (NGX_HTTP_DAV)
#define NGX_MODULE_SIGNATURE_31  "1"
#else
#define NGX_MODULE_SIGNATURE_31  "0"
#endif

#if (NGX_HTTP_CACHE)
#define NGX_MODULE_SIGNATURE_32  "1"
#else
#define NGX_MODULE_SIGNATURE_32  "0"
#endif

#if (NGX_HTTP_UPSTREAM_ZONE)
#define NGX_MODULE_SIGNATURE_33  "1"
#else
#define NGX_MODULE_SIGNATURE_33  "0"
#endif

#if (NGX_COMPAT)
#define NGX_MODULE_SIGNATURE_34  "1"
#else
#define NGX_MODULE_SIGNATURE_34  "0"
#endif

#define NGX_MODULE_SIGNATURE                                                  \
    NGX_MODULE_SIGNATURE_0 NGX_MODULE_SIGNATURE_1 NGX_MODULE_SIGNATURE_2      \
    NGX_MODULE_SIGNATURE_3 NGX_MODULE_SIGNATURE_4 NGX_MODULE_SIGNATURE_5      \
    NGX_MODULE_SIGNATURE_6 NGX_MODULE_SIGNATURE_7 NGX_MODULE_SIGNATURE_8      \
    NGX_MODULE_SIGNATURE_9 NGX_MODULE_SIGNATURE_10 NGX_MODULE_SIGNATURE_11    \
    NGX_MODULE_SIGNATURE_12 NGX_MODULE_SIGNATURE_13 NGX_MODULE_SIGNATURE_14   \
    NGX_MODULE_SIGNATURE_15 NGX_MODULE_SIGNATURE_16 NGX_MODULE_SIGNATURE_17   \
    NGX_MODULE_SIGNATURE_18 NGX_MODULE_SIGNATURE_19 NGX_MODULE_SIGNATURE_20   \
    NGX_MODULE_SIGNATURE_21 NGX_MODULE_SIGNATURE_22 NGX_MODULE_SIGNATURE_23   \
    NGX_MODULE_SIGNATURE_24 NGX_MODULE_SIGNATURE_25 NGX_MODULE_SIGNATURE_26   \
    NGX_MODULE_SIGNATURE_27 NGX_MODULE_SIGNATURE_28 NGX_MODULE_SIGNATURE_29   \
    NGX_MODULE_SIGNATURE_30 NGX_MODULE_SIGNATURE_31 NGX_MODULE_SIGNATURE_32   \
    NGX_MODULE_SIGNATURE_33 NGX_MODULE_SIGNATURE_34


// 重新定义了填充宏，加入了签名字符串
#define NGX_MODULE_V1                                                         \
    NGX_MODULE_UNSET_INDEX, NGX_MODULE_UNSET_INDEX,                           \
    NULL, 0, 0, nginx_version, NGX_MODULE_SIGNATURE

// 填充宏，填充ngx_module_t的最后8个字段，设置为空指针
#define NGX_MODULE_V1_PADDING  0, 0, 0, 0, 0, 0, 0, 0

// 重要的数据结构，定义nginx模块
// 重要的模块ngx_core_module/ngx_event_module/ngx_http_module
struct ngx_module_s {
    // 每类(http/event)模块各自的index
    // 初始化为-1
    ngx_uint_t            ctx_index;

    // 在ngx_modules数组里的唯一索引，main()里赋值
    // 使用计数器变量ngx_max_module
    ngx_uint_t            index;

    // 1.10，模块的名字，标识字符串，默认是空指针
    // 由脚本生成ngx_module_names数组，然后在ngx_preinit_modules里填充
    // 动态模块在ngx_load_module里设置名字
    char                 *name;

    ngx_uint_t            spare0;
    ngx_uint_t            spare1;

    ngx_uint_t            version;
    const char           *signature;

    // 模块不同含义不同,通常是函数指针表，是在配置解析的某个阶段调用的函数
    // core模块的ctx
    void                 *ctx;
    // 模块支持的指令，数组形式，最后用空对象表示结束
    ngx_command_t        *commands;

    // 模块的类型标识，相当于RTTI,如CORE/HTTP/STRM/MAIL等
    ngx_uint_t            type;

    ngx_int_t           (*init_master)(ngx_log_t *log);

    // 在ngx_init_cycle里被调用
    // 在master进程里，fork出worker子进程之前
    // 做一些基本的初始化工作，数据会被子进程复制
    ngx_int_t           (*init_module)(ngx_cycle_t *cycle);

    // 在ngx_single_process_cycle/ngx_worker_process_init里调用
    // 在worker进程进入工作循环之前被调用
    // 初始化每个子进程自己专用的数据
    ngx_int_t           (*init_process)(ngx_cycle_t *cycle);
    ngx_int_t           (*init_thread)(ngx_cycle_t *cycle);
    void                (*exit_thread)(ngx_cycle_t *cycle);

    // 在ngx_worker_process_exit调用
    void                (*exit_process)(ngx_cycle_t *cycle);

    void                (*exit_master)(ngx_cycle_t *cycle);

    // 下面8个成员通常用用NGX_MODULE_V1_PADDING填充
    uintptr_t             spare_hook0;
    uintptr_t             spare_hook1;
    uintptr_t             spare_hook2;
    uintptr_t             spare_hook3;
    uintptr_t             spare_hook4;
    uintptr_t             spare_hook5;
    uintptr_t             spare_hook6;
    uintptr_t             spare_hook7;
};


// 核心模块的ctx结构，比较简单，只有创建和初始化配置结构函数
// create_conf函数返回的是void*指针
typedef struct {
    ngx_str_t             name;
    void               *(*create_conf)(ngx_cycle_t *cycle);
    char               *(*init_conf)(ngx_cycle_t *cycle, void *conf);
} ngx_core_module_t;

// main()里调用
// 计算所有的静态模块数量
// ngx_modules是nginx模块数组，存储所有的模块指针，由make生成在objs/ngx_modules.c
// 这里赋值每个模块的index成员
// ngx_modules_n保存了最后一个可用的序号
// ngx_max_module是模块数量的上限
ngx_int_t ngx_preinit_modules(void);

// main -> ngx_init_cycle里调用
// 内存池创建一个数组，可以容纳所有的模块，大小是ngx_max_module + 1
// 拷贝脚本生成的静态模块数组到本cycle
// 拷贝模块序号计数器到本cycle
// 完成cycle的模块初始化
ngx_int_t ngx_cycle_modules(ngx_cycle_t *cycle);

// main -> ngx_init_cycle里调用
// 调用所有模块的init_module函数指针，初始化模块
// 不使用全局的ngx_modules数组，而是使用cycle里的
ngx_int_t ngx_init_modules(ngx_cycle_t *cycle);

// 在ngx_event.c等调用，在解析配置块时
// 得到cycle里所有的事件/http/stream模块数量
// 设置某类型模块的ctx_index
// type是模块的类型，例如NGX_EVENT_MODULE
// 返回此类型模块的数量
ngx_int_t ngx_count_modules(ngx_cycle_t *cycle, ngx_uint_t type);

// cycle->modules_n是模块计数器 如果超过最大数量则报错
// 使用模块里的各种信息进行检查，只有正确的才能加载
// 首先是版本号，必须一致，例如1.10的不能给1.9使用
// 比较签名字符串，里面是二进制兼容信息
// 看cycle里的模块数组里是否有重名的 也就是说每个模块的名字都不能相同
// 模块还没有加载，那么就给一个全局序号，不是ctx_index
// 把动态模块的指针加入cycle的模块数组
// 最后完成了一个动态模块的加载，放到了cycle模块数组里的合适位置
ngx_int_t ngx_add_module(ngx_conf_t *cf, ngx_str_t *file,
                         ngx_module_t *module, char **order);

// nginx模块数组，存储所有的模块指针，由make生成在objs/ngx_modules.c
// ngx_cycle_modules拷贝后就不再使用
extern ngx_module_t  *ngx_modules[];

// 模块数量的上限， 所有模块不能超过这个数量，ngx_module.c
extern ngx_uint_t     ngx_max_module;

// 模块的名字数组，由make生成在objs/ngx_modules.c
// 仅在ngx_preinit_modules里使用
extern char          *ngx_module_names[];


#endif //NGINX_LEARNING_NGX_MODULE_H
