/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_cycle.h
* @date:      2018/2/12 下午3:18
* @desc:
 * https://github.com/chronolaw/annotated_nginx/blob/master/nginx/src/core/ngx_cycle.c
 * https://github.com/xieyugui/reading-code-of-nginx-1.9.2/blob/master/nginx-1.9.2/src/core/ngx_cycle.c
*/

//
// Created by daemon.xie on 2018/2/12.
//

#ifndef NGX_CYCLE_NGX_CYCLE_H
#define NGX_CYCLE_NGX_CYCLE_H

#include <ngx_config.h>
#include <ngx_core.h>

#ifndef NGX_CYCLE_POOL_SIZE
#define NGX_CYCLE_POOL_SIZE  NGX_DEFAULT_POOL_SIZE
#endif

#define NGX_DEBUG_POINTS_STOP 1
#define NGX_DEBUG_POINTS_ABORT 2

typedef struct ngx_shm_zone_s ngx_shm_zone_t;

typedef ngx_int_t (*ngx_shm_zone_init_pt) (ngx_shm_zone_t *zone, void *data);

//所有的共享内存都通过ngx_http_file_cache_s->shpool进行管理
//  每个共享内存对应一个ngx_slab_pool_t来管理，见ngx_init_zone_pool
//nginx是多进程模型，在许多场景我们可能需要跨进程共享数据，考虑到这个可能性，nginx本身也提供了共享内存这方面的接口
// nginx共享内存结构体
struct ngx_shm_zone_s {
    void *data; //指向ngx_http_file_cache_t，赋值见ngx_http_file_cache_set_slot
    //真正的共享内存
    ngx_shm_t shm; //ngx_init_cycle->ngx_shm_alloc->ngx_shm_alloc中创建相应的共享内存空间
    //ngx_init_cycle中执行 初始化函数
    ngx_shm_zone_init_pt init;
    void *tag; //创建的这个共享内存属于哪个模块
    ngx_uint_t noreuse;
};

// nginx核心数据结构，表示nginx的生命周期，含有许多重要参数
// conf_ctx, 存储所有模块的配置结构体，是个二维数组
// 启动nginx时的环境参数，配置文件，工作路径等 每个进程都有这个结构
struct ngx_cycle_s {
    // 存储所有模块的配置结构体，是个二维数组
    // 0 = ngx_core_module
    // 1 = ngx_errlog_module
    // 3 = ngx_event_module
    // 4 = ngx_event_core_module
    // 5 = ngx_epoll_module
    // 7 = ngx_http_module
    // 8 = ngx_http_core_module
    void ****conf_ctx;
    ngx_pool_t *pool;

    /*    日志模块中提供了生成基本ngx_log_t日志对象的功能，这里的log实际上是在还没有执行ngx_init_cycle方法前，
    也就是还没有解析配置前，如果有信息需要输出到日志，就会暂时使用log对象，它会输出到屏幕。 */
    //如果配置error_log，指向这个配置后面的文件参数，见ngx_error_log。否则在ngx_log_open_default中设置
    ngx_log_t *log;
    //如果配置error_log，指向这个配置后面的文件参数，见ngx_error_log。否则在ngx_log_open_default中设置
    /* 由nginx.conf配置文件读取到日志文件路径后，将开始初始化error_log日志文件，
     * 由于log对象还在用于输出日志到屏幕，这时会用new_log对象暂时性地替代log日志，
     * 待初始化成功后，会用new_log的地址覆盖上面的log指针    */
    ngx_log_t new_log;

    ngx_uint_t log_use_stderr;

    // 文件也当做连接来处理，也是读写操作
    /*  对于poll，rtsig这样的事件模块，会以有效文件句柄数来预先建立这些ngx_connection t结构
    体，以加速事件的收集、分发。这时files就会保存所有ngx_connection_t的指针组成的数组，files_n就是指
    针的总数，而文件句柄的值用来访问files数组成员 */
    ngx_connection_t **files;

    // 空闲连接，使用指针串成单向链表
    // 指向第一个空闲连接，即头节点
    ngx_connection_t *free_connections;
    ngx_uint_t free_connection_n; // 空闲连接的数量

    ngx_module_t **modules; // 可以容纳所有的模块，大小是ngx_max_module + 1
    ngx_uint_t modules_n;
    // 标志位，cycle已经完成模块的初始化，不能再添加模块
    // 在ngx_load_module里检查，不允许加载动态模块
    ngx_uint_t modules_used;

    // 复用连接对象队列
    ngx_queue_t reusable_connections_queue; //双向链表容器
    ngx_uint_t reusable_connections_n;

    // 监听的端口数组
    ngx_array_t listening;
    // 打开的目录
    ngx_array_t paths;

    // dump config用
    ngx_array_t config_dump;
    ngx_rbtree_t config_dump_rbtree;
    ngx_rbtree_node_t config_dump_sentinel;

    //如nginx.conf配置文件中的access_log参数的文件就保存在该链表中
    ngx_list_t open_files;// 打开的文件
    ngx_list_t shared_memory; // 单链表容器，元素类型是ngx_shm_zone_t结构体，每个元素表示一块共享内存

    // 连接数组的数量
    // 由worker_connections指定，在event模块里设置
    ngx_uint_t connection_n;
    ngx_uint_t files_n;

    // 连接池,大小是connection_n
    // 每个连接都有一个读事件和写事件，使用数组序号对应
    // 由ngx_event_core_module的ngx_event_process_init()创建
    ngx_connection_t *connections; //指向当前进程中的所有连接对象，与connection_n配合使用
    ngx_event_t *read_events; // 读事件数组，大小与connections相同，并且一一对应
    ngx_event_t *write_events; // 写事件数组，大小与connections相同，并且一一对应

    // 保存之前的cycle，如init_cycle
    ngx_cycle_t *old_cycle;

    // 启动nginx时的配置文件
    ngx_str_t conf_file;

    // 启动nginx时的-g参数
    ngx_str_t conf_param;

    // #define NGX_CONF_PREFIX  "conf/"
    // 即-c选项指定的配置文件目录
    ngx_str_t conf_prefix;

    // #define NGX_PREFIX  "/usr/local/nginx/"
    // 即-p选项指定的工作目录
    ngx_str_t prefix;

    // 用于进程间同步的文件锁名称
    ngx_str_t lock_file;

    // 当前主机的hostname
    // ngx_init_cycle()里初始化，全小写
    ngx_str_t hostname;
};

// ngx_core_module的配置结构体，在nginx.c里设置
typedef struct {
    ngx_flag_t daemon; //守护进程是否启用
    ngx_flag_t master; //master/worker进程机制是否启用

    //调用time_update的时间分辨率，毫秒，在event模块里使用
    ngx_msec_t timer_resolution;

    // 1.11.11新增，worker进程关闭的超时时间，默认永远等待
    ngx_msec_t shutdown_timeout;

    ngx_int_t worker_processes; //worker进程的数量
    ngx_int_t debug_points; //是否使用debug point

    // 可打开的最大文件数量，超过则报ENOFILE错误
    ngx_int_t rlimit_nofile;
    off_t rlimit_core; // coredump文件大小

    int priority;

    ngx_uint_t                cpu_affinity_auto;
    ngx_uint_t                cpu_affinity_n; //worker_cpu_affinity参数个数
    ngx_cpuset_t             *cpu_affinity;

    // nginx运行使用的用户名，默认是nobody
    // objs/ngx_auto_config.h:#define NGX_USER  "nobody"
    char *username;
    ngx_uid_t user;
    ngx_gid_t group;

    ngx_str_t working_directory;
    ngx_str_t lock_file;

    ngx_str_t pid; // master进程的pid文件名
    ngx_str_t oldpid; // new binary时老nginx的pid文件名

    ngx_array_t env;
    char **environment;

    ngx_uint_t transparent;
} ngx_core_conf_t;

#define ngx_is_init_cycle(cycle) (cycle->conf_ctx == NULL)

// 在main里调用
// 从old_cycle(init_cycle)里复制必要的信息，创建新cycle
// 当reconfigure的时候old_cycle就是当前的cycle
// 初始化core模块
ngx_cycle_t *ngx_init_cycle(ngx_cycle_t *old_cycle);

// 写pid到文件
ngx_int_t ngx_create_pidfile(ngx_str_t *name, ngx_log_t *log);

void ngx_delete_pidfile(ngx_cycle_t *cycle);

// main()里调用，如果用了-s参数，那么就要发送reload/stop等信号
ngx_int_t ngx_signal_process(ngx_cycle_t *cycle, char *sig);

//ngx_reopen标志位，如果为1，则表示需要重新打开所有文件
void ngx_reopen_files(ngx_cycle_t *cycle, ngx_uid_t user);

//配置一些环境变量
char **ngx_set_environment(ngx_cycle_t *cycle, ngx_uint_t *last);

//进行热代码替换，这里是调用execve来执行新的代码
ngx_pid_t ngx_exec_new_binary(ngx_cycle_t *cycle, char *const *argv);

ngx_cpuset_t *ngx_get_cpu_affinity(ngx_uint_t n);

ngx_shm_zone_t *ngx_shared_memory_add(ngx_conf_t *cf, ngx_str_t *name, size_t size, void *tag);

// 设置关闭worker进程的超时时间
void ngx_set_shutdown_timer(ngx_cycle_t *cycle);

// nginx生命周期使用的超重要对象
extern volatile ngx_cycle_t *ngx_cycle;
extern ngx_array_t ngx_old_cycles;
extern ngx_module_t ngx_core_module;

// -t参数，检查配置文件, in ngx_cycle.c
extern ngx_uint_t ngx_test_config;
extern ngx_uint_t ngx_dump_config;

// 安静模式，不输出测试信息, in ngx_cycle.c
extern ngx_uint_t ngx_quiet_mode;

#endif //NGX_CYCLE_NGX_CYCLE_H
