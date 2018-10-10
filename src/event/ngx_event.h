
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_EVENT_H_INCLUDED_
#define _NGX_EVENT_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


#define NGX_INVALID_INDEX  0xd0d0d0d0


#if (NGX_HAVE_IOCP)

typedef struct {
    WSAOVERLAPPED    ovlp;
    ngx_event_t     *event;
    int              error;
} ngx_event_ovlp_t;

#endif

/* 描述每一个事件的ngx_event_t结构体 */
struct ngx_event_s {
    void            *data;//事件相关的对象。通常data都是指向ngx_connection_t连接对象

    //可写事件ev默认为1  读ev事件应该默认还是0
    unsigned         write:1;/* 标志位，为1表示事件可写，即当前对应的TCP连接状态可写 */

    //标志位，为1时表示为此事件可以建立新的连接
    unsigned         accept:1;

    /* used to detect the stale events in kqueue and epoll */
    unsigned         instance:1;//标志位用于区分当前事件是否是过期的

    /*
     * the event was passed or would be passed to a kernel;
     * in aio mode - operation was posted.
     * 标志位，为1时表示当前事件是活跃的，为0时表示事件是不活跃的
     */
    unsigned         active:1;//标记是否已经添加到事件驱动中，避免重复添加  在server端accept成功后

    //标志位，为1时表示禁用事件，仅在kqueue或者rtsig事件驱动模块中有效，而对于epoll事件驱动模块则无意义
    unsigned         disabled:1;

    /* the ready event; in aio mode 0 means that no operation can be posted */
    //标志位，为1时表示当前事件已经淮备就绪，也就是说，允许这个事件的消费模块处理这个事件
    unsigned         ready:1;

    unsigned         oneshot:1;

    /* aio operation is complete */
    unsigned         complete:1;//表示读取数据完成，通过epoll机制返回获取

    //标志位，为1时表示当前处理的字符流已经结束  例如内核缓冲区没有数据，你去读，则会返回0
    unsigned         eof:1;
    //标志位，为1时表示事件在处理过程中出现错误
    unsigned         error:1;

    //标志位，为I时表示这个事件已经超时，用以提示事件的消费模块做超时处理
    unsigned         timedout:1;
    //标志位，为1时表示这个事件存在于定时器中
    unsigned         timer_set:1;

    //标志位，delayed为1时表示需要延迟处理这个事件，它仅用于限速功能
    unsigned         delayed:1;

    //标志位，为1时表示延迟建立TCP连接，也就是说，经过TCP三次握手后并不建立连接，而是要等到真正收到数据包后才会建立TCP连接
    unsigned         deferred_accept:1;

    /* the pending eof reported by kqueue, epoll or in aio chain operation */
    //标志位，为1时表示等待字符流结束，它只与kqueue和aio事件驱动机制有关
    unsigned         pending_eof:1;


    //表示延迟处理该事件
    unsigned         posted:1;

    //标志位，为1时表示当前事件已经关闭，epoll模块没有使用它
    unsigned         closed:1;

    /* to test on worker exit */
    unsigned         channel:1;
    unsigned         resolver:1;

    unsigned         cancelable:1;

#if (NGX_HAVE_KQUEUE)
    unsigned         kq_vnode:1;

    /* the pending errno reported by kqueue */
    int              kq_errno;
#endif

    /*
     * kqueue only:
     *   accept:     number of sockets that wait to be accepted
     *   read:       bytes to read when event is ready
     *               or lowat when event is set with NGX_LOWAT_EVENT flag
     *   write:      available space in buffer when event is ready
     *               or lowat when event is set with NGX_LOWAT_EVENT flag
     *
     * epoll with EPOLLRDHUP:
     *   accept:     1 if accept many, 0 otherwise
     *   read:       1 if there can be data to read, 0 otherwise
     *
     * iocp: TODO
     *
     * otherwise:
     *   accept:     1 if accept many, 0 otherwise
     */

//标志住，在epoll事件驱动机制下表示一次尽可能多地建立TCP连接，它与multi_accept配置项对应
#if (NGX_HAVE_KQUEUE) || (NGX_HAVE_IOCP)
    int              available;
#else
    unsigned         available:1;
#endif

    //每一个事件最核心的部分是handler回调方法，它将由每一个事件消费模块实现，以此决定这个事件究竟如何“消费”
    ngx_event_handler_pt  handler;//由epoll读写事件在ngx_epoll_process_events触发


#if (NGX_HAVE_IOCP)
    ngx_event_ovlp_t ovlp;
#endif

    ngx_uint_t       index;

    //可用于记录error_log日志的ngx_log_t对象
    ngx_log_t       *log;

    //定时器节点，用于定时器红黑树中
    ngx_rbtree_node_t   timer;

    /*
    post事件将会构成一个队列再统一处理，这个队列以next和prev作为链表指针，以此构成一个简易的双向链表，其中next指向后一个事件的地址，
    prev指向前一个事件的地址
     */
    /* the posted queue */
    ngx_queue_t      queue;

#if 0

    /* the threads support */

    /*
     * the event thread context, we store it here
     * if $(CC) does not understand __thread declaration
     * and pthread_getspecific() is too costly
     */

    void            *thr_ctx;

#if (NGX_EVENT_T_PADDING)

    /* event should not cross cache line in SMP */

    uint32_t         padding[NGX_EVENT_T_PADDING];
#endif
#endif
};


#if (NGX_HAVE_FILE_AIO)

struct ngx_event_aio_s {
    void                      *data;//指向ngx_http_request_t
    ngx_event_handler_pt       handler;//这是真正由业务模块实现的方法，在异步I/O事件完成后被调用
    ngx_file_t                *file;//file为要读取的file文件信息

#if (NGX_HAVE_AIO_SENDFILE || NGX_COMPAT)
    ssize_t                  (*preload_handler)(ngx_buf_t *file);
#endif

    ngx_fd_t                   fd;//file为要读取的file文件信息对应的文件描述符

#if (NGX_HAVE_EVENTFD)
    int64_t                    res;
#endif

#if !(NGX_HAVE_EVENTFD) || (NGX_TEST_BUILD_EPOLL)
    ngx_err_t                  err;
    size_t                     nbytes;
#endif

    ngx_aiocb_t                aiocb;
    ngx_event_t                event;//只是异步i/o读事件
};

#endif

//事件驱动模块的具体方法
typedef struct {
    /* 添加事件，将某个描述符的某个事件添加到事件驱动机制监控描述符集中 */
    ngx_int_t  (*add)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags);//ngx_add_event中执行
    /* 删除事件，将某个描述符的某个事件从事件驱动机制监控描述符集中删除 */
    ngx_int_t  (*del)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags);

    /* 启动对某个指定事件的监控 */
    ngx_int_t  (*enable)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags);
    /* 禁用对某个指定事件的监控 */
    ngx_int_t  (*disable)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags);

    /* 将指定连接所关联的描述符添加到事件驱动机制监控中 */
    ngx_int_t  (*add_conn)(ngx_connection_t *c);
    /* 将指定连接所关联的描述符从事件驱动机制监控中删除 */
    ngx_int_t  (*del_conn)(ngx_connection_t *c, ngx_uint_t flags);

    ngx_int_t  (*notify)(ngx_event_handler_pt handler);

    /* 等待事件的发生，并对事件进行处理 */
    ngx_int_t  (*process_events)(ngx_cycle_t *cycle, ngx_msec_t timer,
                                 ngx_uint_t flags);

    /* 初始化事件驱动模块 */
    ngx_int_t  (*init)(ngx_cycle_t *cycle, ngx_msec_t timer);
    /* 在退出事件驱动模块前调用该函数回收资源 */
    void       (*done)(ngx_cycle_t *cycle);
} ngx_event_actions_t;


extern ngx_event_actions_t   ngx_event_actions;
#if (NGX_HAVE_EPOLLRDHUP)
extern ngx_uint_t            ngx_use_epoll_rdhup;
#endif


/*
 * The event filter requires to read/write the whole data:
 * select, poll, /dev/poll, kqueue, epoll.
 */
//epoll的LT模式   见ngx_handle_read_event  nginx默认使用NGX_USE_CLEAR_EVENT边沿触发方式
#define NGX_USE_LEVEL_EVENT      0x00000001

/*
 * The event filter is deleted after a notification without an additional
 * syscall: kqueue, epoll.
 * 单次触发
 */
#define NGX_USE_ONESHOT_EVENT    0x00000002

/*
 * The event filter notifies only the changes and an initial level:
 * kqueue, epoll.
 * 边沿触发
 */
#define NGX_USE_CLEAR_EVENT      0x00000004

/*
 * The event filter has kqueue features: the eof flag, errno,
 * available data, etc.
 */
#define NGX_USE_KQUEUE_EVENT     0x00000008

/*
 * The event filter supports low water mark: kqueue's NOTE_LOWAT.
 * kqueue in FreeBSD 4.1-4.2 has no NOTE_LOWAT so we need a separate flag.
 */
#define NGX_USE_LOWAT_EVENT      0x00000010

/*
 * The event filter requires to do i/o operation until EAGAIN: epoll.
 */
#define NGX_USE_GREEDY_EVENT     0x00000020

/*
 * The event filter is epoll.
 */
#define NGX_USE_EPOLL_EVENT      0x00000040

/*
 * Obsolete.
 */
#define NGX_USE_RTSIG_EVENT      0x00000080

/*
 * Obsolete.
 */
#define NGX_USE_AIO_EVENT        0x00000100

/*
 * Need to add socket or handle only once: i/o completion port.
 */
#define NGX_USE_IOCP_EVENT       0x00000200

/*
 * The event filter has no opaque data and requires file descriptors table:
 * poll, /dev/poll.
 */
#define NGX_USE_FD_EVENT         0x00000400

/*
 * The event module handles periodic or absolute timer event by itself:
 * kqueue in FreeBSD 4.4, NetBSD 2.0, and MacOSX 10.4, Solaris 10's event ports.
 */
#define NGX_USE_TIMER_EVENT      0x00000800

/*
 * All event filters on file descriptor are deleted after a notification:
 * Solaris 10's event ports.
 */
#define NGX_USE_EVENTPORT_EVENT  0x00001000

/*
 * The event filter support vnode notifications: kqueue.
 */
#define NGX_USE_VNODE_EVENT      0x00002000


/*
 * The event filter is deleted just before the closing file.
 * Has no meaning for select and poll.
 * kqueue, epoll, eventport:         allows to avoid explicit delete,
 *                                   because filter automatically is deleted
 *                                   on file close,
 *
 * /dev/poll:                        we need to flush POLLREMOVE event
 *                                   before closing file.
 */
#define NGX_CLOSE_EVENT    1

/*
 * disable temporarily event filter, this may avoid locks
 * in kernel malloc()/free(): kqueue.
 */
#define NGX_DISABLE_EVENT  2

/*
 * event must be passed to kernel right now, do not wait until batch processing.
 */
#define NGX_FLUSH_EVENT    4


/* these flags have a meaning only for kqueue */
#define NGX_LOWAT_EVENT    0
#define NGX_VNODE_EVENT    0


#if (NGX_HAVE_EPOLL) && !(NGX_HAVE_EPOLLRDHUP)
#define EPOLLRDHUP         0
#endif

// 重定义事件宏，屏蔽系统差异
#if (NGX_HAVE_KQUEUE)

#define NGX_READ_EVENT     EVFILT_READ
#define NGX_WRITE_EVENT    EVFILT_WRITE

#undef  NGX_VNODE_EVENT
#define NGX_VNODE_EVENT    EVFILT_VNODE

/*
 * NGX_CLOSE_EVENT, NGX_LOWAT_EVENT, and NGX_FLUSH_EVENT are the module flags
 * and they must not go into a kernel so we need to choose the value
 * that must not interfere with any existent and future kqueue flags.
 * kqueue has such values - EV_FLAG1, EV_EOF, and EV_ERROR:
 * they are reserved and cleared on a kernel entrance.
 */
#undef  NGX_CLOSE_EVENT
#define NGX_CLOSE_EVENT    EV_EOF

#undef  NGX_LOWAT_EVENT
#define NGX_LOWAT_EVENT    EV_FLAG1

#undef  NGX_FLUSH_EVENT
#define NGX_FLUSH_EVENT    EV_ERROR

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  EV_ONESHOT
#define NGX_CLEAR_EVENT    EV_CLEAR

#undef  NGX_DISABLE_EVENT
#define NGX_DISABLE_EVENT  EV_DISABLE


#elif (NGX_HAVE_DEVPOLL && !(NGX_TEST_BUILD_DEVPOLL)) \
      || (NGX_HAVE_EVENTPORT && !(NGX_TEST_BUILD_EVENTPORT))

#define NGX_READ_EVENT     POLLIN
#define NGX_WRITE_EVENT    POLLOUT

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  1


#elif (NGX_HAVE_EPOLL) && !(NGX_TEST_BUILD_EPOLL)

#define NGX_READ_EVENT     (EPOLLIN|EPOLLRDHUP)
#define NGX_WRITE_EVENT    EPOLLOUT

#define NGX_LEVEL_EVENT    0
#define NGX_CLEAR_EVENT    EPOLLET
#define NGX_ONESHOT_EVENT  0x70000000
#if 0
#define NGX_ONESHOT_EVENT  EPOLLONESHOT
#endif

#if (NGX_HAVE_EPOLLEXCLUSIVE)
#define NGX_EXCLUSIVE_EVENT  EPOLLEXCLUSIVE
#endif

#elif (NGX_HAVE_POLL)

#define NGX_READ_EVENT     POLLIN
#define NGX_WRITE_EVENT    POLLOUT

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  1


#else /* select */

#define NGX_READ_EVENT     0
#define NGX_WRITE_EVENT    1

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  1

#endif /* NGX_HAVE_KQUEUE */


#if (NGX_HAVE_IOCP)
#define NGX_IOCP_ACCEPT      0
#define NGX_IOCP_IO          1
#define NGX_IOCP_CONNECT     2
#endif


#if (NGX_TEST_BUILD_EPOLL)
#define NGX_EXCLUSIVE_EVENT  0
#endif


#ifndef NGX_CLEAR_EVENT
#define NGX_CLEAR_EVENT    0    /* dummy declaration */
#endif

// 全局的事件模块访问接口，是一个函数表
// 定义了若干宏简化对它的操作
// 常用的有ngx_add_event/ngx_del_event
#define ngx_process_events   ngx_event_actions.process_events
#define ngx_done_events      ngx_event_actions.done

// 向epoll添加删除事件
#define ngx_add_event        ngx_event_actions.add
#define ngx_del_event        ngx_event_actions.del

// 向epoll添加删除连接，即同时添加读写事件
#define ngx_add_conn         ngx_event_actions.add_conn
#define ngx_del_conn         ngx_event_actions.del_conn

// 事件通知
#define ngx_notify           ngx_event_actions.notify
// 向定时器红黑树里添加事件，设置超时，ngx_event_timer.h
#define ngx_add_timer        ngx_event_add_timer
// 从定时器红黑树里删除事件
#define ngx_del_timer        ngx_event_del_timer


extern ngx_os_io_t  ngx_io;

// 宏定义简化调用
#define ngx_recv             ngx_io.recv
#define ngx_recv_chain       ngx_io.recv_chain
#define ngx_udp_recv         ngx_io.udp_recv
#define ngx_send             ngx_io.send
#define ngx_send_chain       ngx_io.send_chain
#define ngx_udp_send         ngx_io.udp_send
#define ngx_udp_send_chain   ngx_io.udp_send_chain


#define NGX_EVENT_MODULE      0x544E5645  /* "EVNT" */
#define NGX_EVENT_CONF        0x02000000

/* 存储ngx_event_core_module事件模块配置项参数的结构体 ngx_event_conf_t */
typedef struct {
    /* 连接池中最大连接数 */
    ngx_uint_t    connections;
    /* 被选用模块在所有事件模块中的序号 */
    ngx_uint_t    use;

    /* 标志位，为1表示可批量建立连接 */
    ngx_flag_t    multi_accept;
    /* 标志位，为1表示打开负载均衡锁 */
    ngx_flag_t    accept_mutex;

    /* 延迟建立连接 */
    ngx_msec_t    accept_mutex_delay;

    /* 被使用事件模块的名称 */
    u_char       *name;

#if (NGX_DEBUG)
    /* 用于保存与输出调试级别日志连接对应客户端的地址信息 */
    ngx_array_t   debug_connection;
#endif
} ngx_event_conf_t;

/* 事件驱动模型通用接口ngx_event_module_t结构体 */
typedef struct {
    /* 事件模块名称 */
    ngx_str_t              *name;

    /* 解析配置项前调用，创建存储配置项参数的结构体 */
    void                 *(*create_conf)(ngx_cycle_t *cycle);
    /* 完成配置项解析后调用，处理当前事件模块感兴趣的全部配置 */
    char                 *(*init_conf)(ngx_cycle_t *cycle, void *conf);

    /* 每个事件模块具体实现的方法，有10个方法，即IO多路复用模型的统一接口 */
    ngx_event_actions_t     actions;
} ngx_event_module_t;


//nginx处理的连接总数 使用共享内存，所有worker公用
extern ngx_atomic_t          *ngx_connection_counter;

// 用共享内存实现的原子变量，负载均衡锁
// 使用1.9.x的reuseport会自动禁用负载均衡，效率更高
extern ngx_atomic_t          *ngx_accept_mutex_ptr;
//用于多个worker进程之间的负载均衡锁
extern ngx_shmtx_t            ngx_accept_mutex;
extern ngx_uint_t             ngx_use_accept_mutex;
extern ngx_uint_t             ngx_accept_events;
extern ngx_uint_t             ngx_accept_mutex_held;
extern ngx_msec_t             ngx_accept_mutex_delay;
extern ngx_int_t              ngx_accept_disabled;

// stat模块的统计用变量，也用共享内存实现
#if (NGX_STAT_STUB)

extern ngx_atomic_t  *ngx_stat_accepted;
extern ngx_atomic_t  *ngx_stat_handled;
extern ngx_atomic_t  *ngx_stat_requests;
extern ngx_atomic_t  *ngx_stat_active;
extern ngx_atomic_t  *ngx_stat_reading;
extern ngx_atomic_t  *ngx_stat_writing;
extern ngx_atomic_t  *ngx_stat_waiting;

#endif

// NGX_UPDATE_TIME要求epoll主动更新时间
#define NGX_UPDATE_TIME         1

// 要求事件延后处理，加入post队列，避免处理事件过多地占用负载均衡锁
#define NGX_POST_EVENTS         2

// 在epoll的ngx_epoll_process_events里检查，更新时间的标志
extern sig_atomic_t           ngx_event_timer_alarm;

// 事件模型的基本标志位
// 在ngx_epoll_init里设置为et模式，边缘触发
// NGX_USE_CLEAR_EVENT|NGX_USE_GREEDY_EVENT|NGX_USE_EPOLL_EVENT
extern ngx_uint_t             ngx_event_flags;
extern ngx_module_t           ngx_events_module;
extern ngx_module_t           ngx_event_core_module;


#define ngx_event_get_conf(conf_ctx, module)                                  \
             (*(ngx_get_conf(conf_ctx, ngx_events_module))) [module.ctx_index];



void ngx_event_accept(ngx_event_t *ev);
#if !(NGX_WIN32)
void ngx_event_recvmsg(ngx_event_t *ev);
#endif
//尝试获取accept锁
ngx_int_t ngx_trylock_accept_mutex(ngx_cycle_t *cycle);
u_char *ngx_accept_log_error(ngx_log_t *log, u_char *buf, size_t len);

// 在ngx_single_process_cycle/ngx_worker_process_cycle里调用
// 处理socket读写事件和定时器事件
// 获取负载均衡锁，监听端口接受连接
// 调用epoll模块的ngx_epoll_process_events获取发生的事件
// 然后处理超时事件和在延后队列里的所有事件
void ngx_process_events_and_timers(ngx_cycle_t *cycle);
// 添加读事件的便捷接口，适合epoll/kqueue/select等各种事件模型
// 内部还是调用ngx_add_event
ngx_int_t ngx_handle_read_event(ngx_event_t *rev, ngx_uint_t flags);
// 添加写事件的便捷接口，适合epoll/kqueue/select等各种事件模型
// 内部还是调用ngx_add_event,多了个send_lowat操作
// linux不支持send_lowat指令，send_lowat总是0
ngx_int_t ngx_handle_write_event(ngx_event_t *wev, size_t lowat);


#if (NGX_WIN32)
void ngx_event_acceptex(ngx_event_t *ev);
ngx_int_t ngx_event_post_acceptex(ngx_listening_t *ls, ngx_uint_t n);
u_char *ngx_acceptex_log_error(ngx_log_t *log, u_char *buf, size_t len);
#endif


ngx_int_t ngx_send_lowat(ngx_connection_t *c, size_t lowat);


/* used in ngx_log_debugX() */
#define ngx_event_ident(p)  ((ngx_connection_t *) (p))->fd


#include <ngx_event_timer.h>
#include <ngx_event_posted.h>

#if (NGX_WIN32)
#include <ngx_iocp_module.h>
#endif


#endif /* _NGX_EVENT_H_INCLUDED_ */
