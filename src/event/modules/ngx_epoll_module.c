
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 *
 * 参考 https://www.kancloud.cn/digest/understandingnginx/202601
 * https://github.com/xieyugui/reading-code-of-nginx-1.9.2/blob/master/nginx-1.9.2/src/event/modules/ngx_epoll_module.c
 *
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


#if (NGX_TEST_BUILD_EPOLL)

/* epoll declarations */

#define EPOLLIN        0x001 //表示对应的连接上有数据可以读出（TCP连接的远端主动关闭连接，也相当于可读事件，因为需要处理发送来的FIN包
#define EPOLLPRI       0x002 //表示对应的连接上有紧急数据需要读
#define EPOLLOUT       0x004 //表示对应的连接上可以写入数据发送（主动向上游服务器发起非阻塞的TCP连接，连接建立成功的事件相当于可写事件
#define EPOLLERR       0x008 //表示对应的连接发生错误
#define EPOLLHUP       0x010 //表示对应的连接被挂起
#define EPOLLRDNORM    0x040
#define EPOLLRDBAND    0x080
#define EPOLLWRNORM    0x100
#define EPOLLWRBAND    0x200
#define EPOLLMSG       0x400

#define EPOLLRDHUP     0x2000 //表示TCP连接的远端关闭或半关闭连接

#define EPOLLEXCLUSIVE 0x10000000
#define EPOLLONESHOT   0x40000000 //表示对这个事件只处理一次，下次需要处理时需重新加入epoll
#define EPOLLET        0x80000000 //表示将触发方式设置妁边缘触发(ET)，系统默认为水平触发(LT’)

#define EPOLL_CTL_ADD  1
#define EPOLL_CTL_DEL  2
#define EPOLL_CTL_MOD  3

typedef union epoll_data {
    void         *ptr; //在ngx_epoll_add_event中赋值为ngx_connection_t类型
    int           fd;
    uint32_t      u32;
    uint64_t      u64;
} epoll_data_t;

struct epoll_event {
    uint32_t      events;
    epoll_data_t  data;
};


int epoll_create(int size);

int epoll_create(int size)
{
    return -1;
}


int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
    return -1;
}


int epoll_wait(int epfd, struct epoll_event *events, int nevents, int timeout);

int epoll_wait(int epfd, struct epoll_event *events, int nevents, int timeout)
{
    return -1;
}

#if (NGX_HAVE_EVENTFD)
#define SYS_eventfd       323
#endif

#if (NGX_HAVE_FILE_AIO)

#define SYS_io_setup      245
#define SYS_io_destroy    246
#define SYS_io_getevents  247

typedef u_int  aio_context_t;

struct io_event {
    uint64_t  data;  /* the data field from the iocb */ //与提交事件时对应的iocb结构体中的aio_data是一致的
    uint64_t  obj;   /* what iocb this event came from */ //指向提交事件时对应的iocb结构体
    int64_t   res;   /* result code for this event */
    int64_t   res2;  /* secondary result */
};


#endif
#endif /* NGX_TEST_BUILD_EPOLL */

/* 存储epoll模块配置项结构体 */
typedef struct {
    ngx_uint_t  events; /* 表示epoll_wait函数返回的最大事件数 */
    ngx_uint_t  aio_requests; /* 并发处理异步IO事件个数 */
} ngx_epoll_conf_t;

/* epoll模块初始化函数
 * 创建epoll 对象 和 创建 event_list 数组（调用epoll_wait 函数时用于存储从内核复制的已就绪的事件）
 * */
static ngx_int_t ngx_epoll_init(ngx_cycle_t *cycle, ngx_msec_t timer);
#if (NGX_HAVE_EVENTFD)
static ngx_int_t ngx_epoll_notify_init(ngx_log_t *log);
static void ngx_epoll_notify_handler(ngx_event_t *ev);
#endif
#if (NGX_HAVE_EPOLLRDHUP)
static void ngx_epoll_test_rdhup(ngx_cycle_t *cycle);
#endif
static void ngx_epoll_done(ngx_cycle_t *cycle);
static ngx_int_t ngx_epoll_add_event(ngx_event_t *ev, ngx_int_t event,
    ngx_uint_t flags);
static ngx_int_t ngx_epoll_del_event(ngx_event_t *ev, ngx_int_t event,
    ngx_uint_t flags);
static ngx_int_t ngx_epoll_add_connection(ngx_connection_t *c);
static ngx_int_t ngx_epoll_del_connection(ngx_connection_t *c,
    ngx_uint_t flags);
#if (NGX_HAVE_EVENTFD)
static ngx_int_t ngx_epoll_notify(ngx_event_handler_pt handler);
#endif
static ngx_int_t ngx_epoll_process_events(ngx_cycle_t *cycle, ngx_msec_t timer,
    ngx_uint_t flags);

#if (NGX_HAVE_FILE_AIO)
static void ngx_epoll_eventfd_handler(ngx_event_t *ev);
#endif

static void *ngx_epoll_create_conf(ngx_cycle_t *cycle);
static char *ngx_epoll_init_conf(ngx_cycle_t *cycle, void *conf);

static int                  ep = -1; /* epoll对象描述符 */
static struct epoll_event  *event_list; /* 作为epoll_wait函数的第二个参数，保存从内存复制的事件 */
static ngx_uint_t           nevents; /* epoll_wait函数返回的最多事件数 */

#if (NGX_HAVE_EVENTFD)
//执行ngx_epoll_notify后会通过epoll_wait返回执行该函数ngx_epoll_notify_handler
//ngx_epoll_notify_handler  ngx_epoll_notify_init  ngx_epoll_notify(ngx_notify)配合阅读
static int                  notify_fd = -1;
static ngx_event_t          notify_event;
static ngx_connection_t     notify_conn;
#endif

#if (NGX_HAVE_FILE_AIO)
//用于通知异步I/O事件的描述符，它与iocb结构体中的aio_resfd成员是一致的  通过该fd添加到epoll事件中，从而可以检测异步io事件
int                         ngx_eventfd = -1; /* 用于通知异步IO的事件描述符 */
//异步I/O的上下文，全局唯一，必须经过io_setup初始化才能使用
aio_context_t               ngx_aio_ctx = 0; /* 异步IO的上下文结构，由io_setup 函数初始化 */

//异步I/O事件完成后进行通知的描述符，也就是ngx_eventfd所对应的ngx_event_t事件
static ngx_event_t          ngx_eventfd_event; /* 异步IO事件 */
//异步I/O事件完成后进行通知的描述符ngx_eventfd所对应的ngx_connectiont连接
static ngx_connection_t     ngx_eventfd_conn; /* 异步IO事件所对应的连接ngx_connection_t */

#endif

#if (NGX_HAVE_EPOLLRDHUP)
ngx_uint_t                  ngx_use_epoll_rdhup;
#endif

static ngx_str_t      epoll_name = ngx_string("epoll");


/* 定义epoll模块感兴趣的配置项结构数组 */
static ngx_command_t  ngx_epoll_commands[] = {
     /*
      * epoll_events配置项表示epoll_wait函数每次返回的最多事件数(即第3个参数)，
      * 在ngx_epoll_init函数中会预分配epoll_events配置项指定的epoll_event结构体；
      */
    { ngx_string("epoll_events"),
      NGX_EVENT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      0,
      offsetof(ngx_epoll_conf_t, events),
      NULL },

     /*
      * 该配置项表示创建的异步IO上下文能并发处理异步IO事件的个数，
      * 即io_setup函数的第一个参数；
      */
    { ngx_string("worker_aio_requests"),
      NGX_EVENT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      0,
      offsetof(ngx_epoll_conf_t, aio_requests),
      NULL },

      ngx_null_command
};

/* 由事件模块通用接口ngx_event_module_t定义的epoll模块上下文结构 */
static ngx_event_module_t  ngx_epoll_module_ctx = {
    &epoll_name,
    ngx_epoll_create_conf,               /* create configuration */
    ngx_epoll_init_conf,                 /* init configuration */

    {
        ngx_epoll_add_event,             /* add an event */
        ngx_epoll_del_event,             /* delete an event */
        ngx_epoll_add_event,             /* enable an event */
        ngx_epoll_del_event,             /* disable an event */
        ngx_epoll_add_connection,        /* add an connection */
        ngx_epoll_del_connection,        /* delete an connection */
#if (NGX_HAVE_EVENTFD)
        ngx_epoll_notify,                /* trigger a notify */
#else
        NULL,                            /* trigger a notify */
#endif
        ngx_epoll_process_events,        /* process the events */
        ngx_epoll_init,                  /* init the events */
        ngx_epoll_done,                  /* done the events */
    }
};

/* 事件驱动模块的定义 */
ngx_module_t  ngx_epoll_module = {
    NGX_MODULE_V1,
    &ngx_epoll_module_ctx,               /* module context */
    ngx_epoll_commands,                  /* module directives */
    NGX_EVENT_MODULE,                    /* module type */
    NULL,                                /* init master */
    NULL,                                /* init module */
    NULL,                                /* init process */
    NULL,                                /* init thread */
    NULL,                                /* exit thread */
    NULL,                                /* exit process */
    NULL,                                /* exit master */
    NGX_MODULE_V1_PADDING
};


#if (NGX_HAVE_FILE_AIO)

/*
 * We call io_setup(), io_destroy() io_submit(), and io_getevents() directly
 * as syscalls instead of libaio usage, because the library header file
 * supports eventfd() since 0.3.107 version only.
 */
/* 初始化文件异步IO的上下文结构
 * nr_reqs 表示需要初始化的异步IO上下文可以处理的事件的最小个数
 * ctx 是文件异步IO的上下文描述符指针
 * */
static int
io_setup(u_int nr_reqs, aio_context_t *ctx)
{
    return syscall(SYS_io_setup, nr_reqs, ctx);
}

/* 销毁文件异步IO的上下文结构 */
static int
io_destroy(aio_context_t ctx)
{
    return syscall(SYS_io_destroy, ctx);
}

/**
 *  ctx 是文件异步IO的上下文描述符指针
 *  min_nr 表示至少要获取mln_ nr个事件；
 *  nr 表示至多获取nr个事件
 *  events 是执行完成的事件数组
 *  tmo 是超时时间，也就是在获取mm_nr个事件前的等待时间
 */
/* 从文件异步IO操作队列中读取操作 */
static int
io_getevents(aio_context_t ctx, long min_nr, long nr, struct io_event *events,
    struct timespec *tmo)
{
    return syscall(SYS_io_getevents, ctx, min_nr, nr, events, tmo);
}

/* 异步IO的初始化 */
static void
ngx_epoll_aio_init(ngx_cycle_t *cycle, ngx_epoll_conf_t *epcf)
{
    int                 n;
    struct epoll_event  ee;

    /*
        调用eventfd系统调用新建一个eventfd对象，第二个参数中的0表示eventfd的计数器初始值为0，
        系统调用成功返回的是与eventfd对象关联的描述符
    */

#if (NGX_HAVE_SYS_EVENTFD_H)
    ngx_eventfd = eventfd(0, 0);
#else
    /* 使用Linux系统调用获取一个描述符句柄 */
    ngx_eventfd = syscall(SYS_eventfd, 0);
#endif

    if (ngx_eventfd == -1) {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                      "eventfd() failed");
        ngx_file_aio = 0;
        return;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                   "eventfd: %d", ngx_eventfd);

    n = 1;

    /* 设置ngx_eventfd描述符句柄为非阻塞IO模式 */
    if (ioctl(ngx_eventfd, FIONBIO, &n) == -1) {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                      "ioctl(eventfd, FIONBIO) failed");
        goto failed;
    }

    /* 初始化文件异步IO的上下文结构 */
    if (io_setup(epcf->aio_requests, &ngx_aio_ctx) == -1) {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                      "io_setup() failed");
        goto failed;
    }
     /* 设置异步IO事件ngx_eventfd_event，该事件是ngx_eventfd对应的ngx_event事件 */

    /* 用于异步IO完成通知的ngx_eventfd_event事件，它与ngx_eventfd_conn连接对应 */
    ngx_eventfd_event.data = &ngx_eventfd_conn;
    /* 在异步IO事件完成后，调用ngx_epoll_eventfd_handler处理方法 */
    ngx_eventfd_event.handler = ngx_epoll_eventfd_handler;
    /* 设置事件相应的日志 */
    ngx_eventfd_event.log = cycle->log;
    /* 设置active标志位 */
    ngx_eventfd_event.active = 1;
    /* 初始化ngx_eventfd_conn 连接 */
    ngx_eventfd_conn.fd = ngx_eventfd;
     /* ngx_eventfd_conn连接的读事件就是ngx_eventfd_event事件 */
    ngx_eventfd_conn.read = &ngx_eventfd_event;
    /* 设置连接的相应日志 */
    ngx_eventfd_conn.log = cycle->log;

    ee.events = EPOLLIN|EPOLLET;
    ee.data.ptr = &ngx_eventfd_conn;

    /* 向epoll对象添加异步IO通知描述符ngx_eventfd */
    /*
     这样，ngx_epoll_aio init方法会把异步I/O与epoll结合起来，当某一个异步I/O事件完成后，ngx_eventfd旬柄就处于可用状态，
      这样epoll_wait在返回ngx_eventfd event事件后就会调用它的回调方法ngx_epoll_eventfd handler处理已经完成的异步I/O事件
     */
    if (epoll_ctl(ep, EPOLL_CTL_ADD, ngx_eventfd, &ee) != -1) {
        return;
    }

    /* 若添加出错，则销毁文件异步IO上下文结构，并返回 */
    ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                  "epoll_ctl(EPOLL_CTL_ADD, eventfd) failed");

    if (io_destroy(ngx_aio_ctx) == -1) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      "io_destroy() failed");
    }

failed:

    if (close(ngx_eventfd) == -1) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      "eventfd close() failed");
    }

    ngx_eventfd = -1;
    ngx_aio_ctx = 0;
    ngx_file_aio = 0;
}

#endif

/* epoll模块初始化函数 */
static ngx_int_t
ngx_epoll_init(ngx_cycle_t *cycle, ngx_msec_t timer)
{
    ngx_epoll_conf_t  *epcf;

    /* 获取ngx_epoll_module模块的配置项结构 */
    epcf = ngx_event_get_conf(cycle->conf_ctx, ngx_epoll_module);

    if (ep == -1) {
        /* 调用epoll_create函数创建epoll对象描述符 */
        ep = epoll_create(cycle->connection_n / 2);

        /* 若创建失败，则出错返回 */
        if (ep == -1) {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                          "epoll_create() failed");
            return NGX_ERROR;
        }

#if (NGX_HAVE_EVENTFD)
        if (ngx_epoll_notify_init(cycle->log) != NGX_OK) {
            ngx_epoll_module_ctx.actions.notify = NULL;
        }
#endif

#if (NGX_HAVE_FILE_AIO)
        /* 若系统支持异步IO，则初始化异步IO */
        ngx_epoll_aio_init(cycle, epcf);
#endif

#if (NGX_HAVE_EPOLLRDHUP)
        ngx_epoll_test_rdhup(cycle);
#endif
    }

    /*
     * 预分配events个epoll_event结构event_list，event_list是存储产生事件的数组；
     * events由epoll_events配置项指定；
     */
    if (nevents < epcf->events) {
        /*
         * 若现有event_list个数小于配置项所指定的值epcf->events，
         * 则先释放，再从新分配；
         */
        if (event_list) {
            ngx_free(event_list);
        }

        /* 预分配epcf->events个epoll_event结构，并使event_list指向该地址 */
        event_list = ngx_alloc(sizeof(struct epoll_event) * epcf->events,
                               cycle->log);
        if (event_list == NULL) {
            return NGX_ERROR;
        }
    }

    /* 设置正确的epoll_event结构个数 */
    nevents = epcf->events;

    /* 指定IO的读写方法 */
    /*
     * 初始化全局变量ngx_io, ngx_os_io定义为:
        ngx_os_io_t ngx_os_io = {
            ngx_unix_recv,
            ngx_readv_chain,
            ngx_udp_unix_recv,
            ngx_unix_send,
            ngx_writev_chain,
            0
        };（位于src/os/unix/ngx_posix_init.c）
    */
    ngx_io = ngx_os_io;

    /* 设置ngx_event_actions 接口 */
    ngx_event_actions = ngx_epoll_module_ctx.actions;

#if (NGX_HAVE_CLEAR_EVENT)
    /* ET模式 */
    ngx_event_flags = NGX_USE_CLEAR_EVENT
#else
    /* LT模式 */
    ngx_event_flags = NGX_USE_LEVEL_EVENT
#endif
                      |NGX_USE_GREEDY_EVENT
                      |NGX_USE_EPOLL_EVENT;

    return NGX_OK;
}


#if (NGX_HAVE_EVENTFD)

static ngx_int_t
ngx_epoll_notify_init(ngx_log_t *log)
{
    struct epoll_event  ee;

#if (NGX_HAVE_SYS_EVENTFD_H)
    notify_fd = eventfd(0, 0);
#else
    notify_fd = syscall(SYS_eventfd, 0);
#endif

    if (notify_fd == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "eventfd() failed");
        return NGX_ERROR;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, log, 0,
                   "notify eventfd: %d", notify_fd);

    notify_event.handler = ngx_epoll_notify_handler;
    notify_event.log = log;
    notify_event.active = 1;

    notify_conn.fd = notify_fd;
    notify_conn.read = &notify_event;
    notify_conn.log = log;

    ee.events = EPOLLIN|EPOLLET;
    ee.data.ptr = &notify_conn;

    if (epoll_ctl(ep, EPOLL_CTL_ADD, notify_fd, &ee) == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                      "epoll_ctl(EPOLL_CTL_ADD, eventfd) failed");

        if (close(notify_fd) == -1) {
            ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                            "eventfd close() failed");
        }

        return NGX_ERROR;
    }

    return NGX_OK;
}


static void
ngx_epoll_notify_handler(ngx_event_t *ev)
{
    ssize_t               n;
    uint64_t              count;
    ngx_err_t             err;
    ngx_event_handler_pt  handler;

    if (++ev->index == NGX_MAX_UINT32_VALUE) {
        ev->index = 0;

        /* Each successful read(2) returns an 8-byte integer. A read(2) will fail with the error EINVAL if the size of
         *  the supplied buffer is less than 8 bytes.
         */
        n = read(notify_fd, &count, sizeof(uint64_t));

        err = ngx_errno;

        ngx_log_debug3(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                       "read() eventfd %d: %z count:%uL", notify_fd, n, count);

        if ((size_t) n != sizeof(uint64_t)) {
            ngx_log_error(NGX_LOG_ALERT, ev->log, err,
                          "read() eventfd %d failed", notify_fd);
        }
    }

    handler = ev->data;
    handler(ev);
}

#endif


#if (NGX_HAVE_EPOLLRDHUP)
/* 测试是否支持EPOLLRDHUP */
static void
ngx_epoll_test_rdhup(ngx_cycle_t *cycle)
{
    int                 s[2], events;
    struct epoll_event  ee;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, s) == -1) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      "socketpair() failed");
        return;
    }

    ee.events = EPOLLET|EPOLLIN|EPOLLRDHUP;

    if (epoll_ctl(ep, EPOLL_CTL_ADD, s[0], &ee) == -1) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      "epoll_ctl() failed");
        goto failed;
    }

    if (close(s[1]) == -1) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      "close() failed");
        s[1] = -1;
        goto failed;
    }

    s[1] = -1;

    events = epoll_wait(ep, &ee, 1, 5000);

    if (events == -1) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      "epoll_wait() failed");
        goto failed;
    }

    if (events) {
        ngx_use_epoll_rdhup = ee.events & EPOLLRDHUP;

    } else {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, NGX_ETIMEDOUT,
                      "epoll_wait() timed out");
    }

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                   "testing the EPOLLRDHUP flag: %s",
                   ngx_use_epoll_rdhup ? "success" : "fail");

failed:

    if (s[1] != -1 && close(s[1]) == -1) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      "close() failed");
    }

    if (close(s[0]) == -1) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      "close() failed");
    }
}

#endif

/*
ngx_event_actions_t done接口是由ngx_epoll_done方法实现的，在Nginx退出服务时它会得到调用。ngx_epoll_done主要是关闭epoll描述符ep，
同时释放event_1ist数组。
*/
static void
ngx_epoll_done(ngx_cycle_t *cycle)
{
    if (close(ep) == -1) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      "epoll close() failed");
    }

    ep = -1;

#if (NGX_HAVE_EVENTFD)

    if (close(notify_fd) == -1) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      "eventfd close() failed");
    }

    notify_fd = -1;

#endif

#if (NGX_HAVE_FILE_AIO)

    if (ngx_eventfd != -1) {

        if (io_destroy(ngx_aio_ctx) == -1) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                          "io_destroy() failed");
        }

        if (close(ngx_eventfd) == -1) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                          "eventfd close() failed");
        }

        ngx_eventfd = -1;
    }

    ngx_aio_ctx = 0;

#endif

    ngx_free(event_list);

    event_list = NULL;
    nevents = 0;
}

/* 将某个描述符的某个事件添加到epoll对象的监控机制中 */
static ngx_int_t
ngx_epoll_add_event(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags)
{
    int                  op;
    uint32_t             events, prev;
    ngx_event_t         *e;
    ngx_connection_t    *c;
    struct epoll_event   ee;

    /* 每个事件的data成员都存放着其对应的ngx_connection_t连接 */
    /* 获取事件关联的连接 */
    c = ev->data;

    /* events参数是方便下面确定当前事件是可读还是可写
     * 下面会根据event参数确定当前事件是读事件还是写事件，这会决定eventg是加上EPOLLIN标志位还是EPOLLOUT标志位
     * */
    events = (uint32_t) event;

    /*
     * 这里在判断事件类型是可读还是可写，必须根据事件的active标志位来判断事件是否活跃；
     * 因为epoll_ctl函数有添加add和修改mod模式，
     * 若一个事件所关联的连接已经在epoll对象的监控中，则只需修改事件的类型即可；
     * 若一个事件所关联的连接没有在epoll对象的监控中，则需要将其相应的事件类型注册到epoll对象中；
     * 这样做的情况是避免与事件相关联的连接两次注册到epoll对象中；
     */
    if (event == NGX_READ_EVENT) {
        /*
         * 若待添加的事件类型event是可读；
         * 则首先判断该事件所关联的连接是否将写事件添加到epoll对象中，
         * 即先判断关联的连接的写事件是否为活跃事件；
         */
        e = c->write;
        prev = EPOLLOUT;
#if (NGX_READ_EVENT != EPOLLIN|EPOLLRDHUP)
        events = EPOLLIN|EPOLLRDHUP;
#endif

    } else {
        e = c->read;
        prev = EPOLLIN|EPOLLRDHUP;
#if (NGX_WRITE_EVENT != EPOLLOUT)
        events = EPOLLOUT;
#endif
    }

    /* 根据active标志位确定事件是否为活跃事件，以决定到达是修改还是添加事件 */
    if (e->active) {
        /* 若当前事件是活跃事件，则只需修改其事件类型即可 */
        op = EPOLL_CTL_MOD;
        events |= prev;

    } else {
        /* 若当前事件不是活跃事件，则将该事件添加到epoll对象中 */
        op = EPOLL_CTL_ADD;
    }

#if (NGX_HAVE_EPOLLEXCLUSIVE && NGX_HAVE_EPOLLRDHUP)
    if (flags & NGX_EXCLUSIVE_EVENT) {
        events &= ~EPOLLRDHUP;
    }
#endif

    /* 将flags参数加入到events标志位中 */
    ee.events = events | (uint32_t) flags;
    /* prt存储事件关联的连接对象ngx_connection_t以及过期事件instance标志位 */
    ee.data.ptr = (void *) ((uintptr_t) c | ev->instance);

    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "epoll add event: fd:%d op:%d ev:%08XD",
                   c->fd, op, ee.events);

    /* 调用epoll_ctl方法向epoll对象添加事件或在epoll对象中修改事件 */
    if (epoll_ctl(ep, op, c->fd, &ee) == -1) {
        ngx_log_error(NGX_LOG_ALERT, ev->log, ngx_errno,
                      "epoll_ctl(%d, %d) failed", op, c->fd);
        return NGX_ERROR;
    }

    /* 将该事件的active标志位设置为1，表示当前事件是活跃事件 */
    ev->active = 1;
#if 0
    ev->oneshot = (flags & NGX_ONESHOT_EVENT) ? 1 : 0;
#endif

    return NGX_OK;
}

/* 将某个连接的某个事件从epoll对象监控中删除 */
static ngx_int_t
ngx_epoll_del_event(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags)
{
    int                  op;
    uint32_t             prev;
    ngx_event_t         *e;
    ngx_connection_t    *c;
    struct epoll_event   ee;

    /*
     * when the file descriptor is closed, the epoll automatically deletes
     * it from its queue, so we do not need to delete explicitly the event
     * before the closing the file descriptor
     */

    /* 当事件关联的文件描述符关闭后，epoll对象自动将其事件删除 */
    if (flags & NGX_CLOSE_EVENT) {
        ev->active = 0;
        return NGX_OK;
    }

    /* 获取事件关联的连接对象 */
    c = ev->data;

    /* 根据event参数判断当前删除的是读事件还是写事件 */
    if (event == NGX_READ_EVENT) {
        /* 若要删除读事件，则首先判断写事件的active标志位 */
        e = c->write;
        prev = EPOLLOUT;

    } else {
        /* 若要删除写事件，则判断读事件的active标志位 */
        e = c->read;
        prev = EPOLLIN|EPOLLRDHUP;
    }

    /*
     * 若要删除读事件，且写事件是活跃事件，则修改事件类型即可；
     * 若要删除写事件，且读事件是活跃事件，则修改事件类型即可；
     */
    if (e->active) {
        op = EPOLL_CTL_MOD;
        ee.events = prev | (uint32_t) flags;
        ee.data.ptr = (void *) ((uintptr_t) c | ev->instance);

    } else {
        /* 若读写事件都不是活跃事件，此时表示事件未准备就绪，则将其删除 */
        op = EPOLL_CTL_DEL;
        ee.events = 0;
        ee.data.ptr = NULL;
    }

    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "epoll del event: fd:%d op:%d ev:%08XD",
                   c->fd, op, ee.events);

    /* 删除或修改事件 */
    if (epoll_ctl(ep, op, c->fd, &ee) == -1) {
        ngx_log_error(NGX_LOG_ALERT, ev->log, ngx_errno,
                      "epoll_ctl(%d, %d) failed", op, c->fd);
        return NGX_ERROR;
    }

    /* 设置当前事件的active标志位 */
    ev->active = 0;

    return NGX_OK;
}

/* 将指定连接所关联的描述符添加到epoll对象中 */
static ngx_int_t
ngx_epoll_add_connection(ngx_connection_t *c)
{
    struct epoll_event  ee;

    /* 设置事件的类型：可读、可写、ET模式 */
    ee.events = EPOLLIN|EPOLLOUT|EPOLLET|EPOLLRDHUP;
    ee.data.ptr = (void *) ((uintptr_t) c | c->read->instance);

    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, c->log, 0,
                   "epoll add connection: fd:%d ev:%08XD", c->fd, ee.events);

    /* 调用epoll_ctl方法将连接所关联的描述符添加到epoll对象中 */
    if (epoll_ctl(ep, EPOLL_CTL_ADD, c->fd, &ee) == -1) {
        ngx_log_error(NGX_LOG_ALERT, c->log, ngx_errno,
                      "epoll_ctl(EPOLL_CTL_ADD, %d) failed", c->fd);
        return NGX_ERROR;
    }

    /* 设置读写事件的active标志位 */
    c->read->active = 1;
    c->write->active = 1;

    return NGX_OK;
}

/* 将连接所关联的描述符从epoll对象中删除 */
static ngx_int_t
ngx_epoll_del_connection(ngx_connection_t *c, ngx_uint_t flags)
{
    int                 op;
    struct epoll_event  ee;

    /*
     * when the file descriptor is closed the epoll automatically deletes
     * it from its queue so we do not need to delete explicitly the event
     * before the closing the file descriptor
     */

    //如果该函数紧接着会调用close(fd)，那么就不用epoll del了，系统会自动del
    if (flags & NGX_CLOSE_EVENT) {
        c->read->active = 0;
        c->write->active = 0;
        return NGX_OK;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, c->log, 0,
                   "epoll del connection: fd:%d", c->fd);

    op = EPOLL_CTL_DEL;
    ee.events = 0;
    ee.data.ptr = NULL;

    /* 调用epoll_ctl方法将描述符从epoll对象中删除 */
    if (epoll_ctl(ep, op, c->fd, &ee) == -1) {
        ngx_log_error(NGX_LOG_ALERT, c->log, ngx_errno,
                      "epoll_ctl(%d, %d) failed", op, c->fd);
        return NGX_ERROR;
    }

    /* 设置描述符读写事件的active标志位 */
    c->read->active = 0;
    c->write->active = 0;

    return NGX_OK;
}


#if (NGX_HAVE_EVENTFD)

static ngx_int_t
ngx_epoll_notify(ngx_event_handler_pt handler)
{
    static uint64_t inc = 1;

    notify_event.data = handler;

    if ((size_t) write(notify_fd, &inc, sizeof(uint64_t)) != sizeof(uint64_t)) {
        ngx_log_error(NGX_LOG_ALERT, notify_event.log, ngx_errno,
                      "write() to eventfd %d failed", notify_fd);
        return NGX_ERROR;
    }

    return NGX_OK;
}

#endif

/* 处理已准备就绪的事件 */
static ngx_int_t
ngx_epoll_process_events(ngx_cycle_t *cycle, ngx_msec_t timer, ngx_uint_t flags)
{
    int                events;
    uint32_t           revents;
    ngx_int_t          instance, i;
    ngx_uint_t         level;
    ngx_err_t          err;
    ngx_event_t       *rev, *wev;
    ngx_queue_t       *queue;
    ngx_connection_t  *c;

    /* NGX_TIMER_INFINITE == INFTIM */

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                   "epoll timer: %M", timer);

    /* 调用epoll_wait在规定的timer时间内等待监控的事件准备就绪 */
    events = epoll_wait(ep, event_list, (int) nevents, timer);
    //EPOLL_WAIT如果没有读写事件或者定时器超时事件发生，则会进入睡眠，这个过程会让出CPU

    /* 若出错，设置错误编码 */
    err = (events == -1) ? ngx_errno : 0;

    /*
     * 若没有设置timer_resolution配置项时，
     * NGX_UPDATE_TIME 标志表示每次调用epoll_wait函数返回后需要更新时间；
     * 若设置timer_resolution配置项，
     * 则每隔timer_resolution配置项参数会设置ngx_event_timer_alarm为1，表示需要更新时间；
     */
    if (flags & NGX_UPDATE_TIME || ngx_event_timer_alarm) {
        /* 更新时间，将时间缓存到一组全局变量中，方便程序高效获取事件 */
        ngx_time_update();
    }

    /* 处理epoll_wait的错误 */
    if (err) {
        if (err == NGX_EINTR) {

            if (ngx_event_timer_alarm) {//定时器超时引起的epoll_wait返回
                ngx_event_timer_alarm = 0;
                return NGX_OK;
            }

            level = NGX_LOG_INFO;

        } else {
            level = NGX_LOG_ALERT;
        }

        ngx_log_error(level, cycle->log, err, "epoll_wait() failed");
        return NGX_ERROR;
    }

    /*
     * 若epoll_wait返回的事件数events为0，则有两种可能：
     * 1、超时返回，即时间超过timer；
     * 2、在限定的timer时间内返回，此时表示出错error返回；
     */
    if (events == 0) {
        if (timer != NGX_TIMER_INFINITE) {
            return NGX_OK;
        }

        ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                      "epoll_wait() returned no events without timeout");
        return NGX_ERROR;
    }

    /* 遍历由epoll_wait返回的所有已准备就绪的事件，并处理这些事件 */
    for (i = 0; i < events; i++) {
        /*
         * 获取与事件关联的连接对象；
         * 连接对象地址的最低位保存的是添加事件时设置的事件过期标志位；
         */
        c = event_list[i].data.ptr;

        /* 获取事件过期标志位，即连接对象地址的最低位 */
        instance = (uintptr_t) c & 1;
        /* 屏蔽连接对象的最低位，即获取连接对象的真正地址 */
        c = (ngx_connection_t *) ((uintptr_t) c & (uintptr_t) ~1);

        /* 获取读事件 */
        //注意这里的c有可能是accept前的c，用于检测是否客户端发起tcp连接事件,accept返回成功后会重新创建一个ngx_connection_t，用来读写客户端的数据
        rev = c->read;

        /*
         * 同一连接的读写事件的instance标志位是相同的；
         * 若fd描述符为-1，或连接对象读事件的instance标志位不相同，则判为过期事件；
         */
        if (c->fd == -1 || rev->instance != instance) {

            /*
             * the stale event from a file descriptor
             * that was just closed in this iteration
             */

            ngx_log_debug1(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                           "epoll: stale event %p", c);
            continue;
        }

        /* 获取连接对象中已准备就绪的事件类型 */
        revents = event_list[i].events;

        ngx_log_debug3(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                       "epoll: fd:%d ev:%04XD d:%p",
                       c->fd, revents, event_list[i].data.ptr);

        /* 记录epoll_wait的错误返回状态 */
        /*
         * EPOLLERR表示连接出错；EPOLLHUP表示收到RST报文；
         * 检测到上面这两种错误时，TCP连接中可能存在未读取的数据；
         */
        if (revents & (EPOLLERR|EPOLLHUP)) {//例如对方close掉套接字，这里会感应到
            ngx_log_debug2(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                           "epoll_wait() error on fd:%d ev:%04XD",
                           c->fd, revents);

            /*
             * if the error events were returned, add EPOLLIN and EPOLLOUT
             * to handle the events at least in one active handler
             */

            //epoll EPOLLERR|EPOLLHUP实际上是通过触发读写事件进行读写操作recv write来检测连接异常
            revents |= EPOLLIN|EPOLLOUT;
        }

#if 0
        if (revents & ~(EPOLLIN|EPOLLOUT|EPOLLERR|EPOLLHUP)) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                          "strange epoll_wait() events fd:%d ev:%04XD",
                          c->fd, revents);
        }
#endif

        /* 连接有可读事件，且该读事件是active活跃的 */
        if ((revents & EPOLLIN) && rev->active) {

#if (NGX_HAVE_EPOLLRDHUP)
            /* EPOLLRDHUP表示连接对端关闭了读取端 */
            if (revents & EPOLLRDHUP) {
                rev->pending_eof = 1;
            }

            rev->available = 1;
#endif

            /* 读事件已准备就绪 */
            /*
             * 这里要区分active与ready：
             * active是指事件被添加到epoll对象的监控中，
             * 而ready表示被监控的事件已经准备就绪，即可以对其进程IO处理；
             */
            rev->ready = 1;

            /*
             * NGX_POST_EVENTS表示已准备就绪的事件需要延迟处理，
             * 根据accept标志位将事件加入到相应的队列中；
             */
            if (flags & NGX_POST_EVENTS) {
                queue = rev->accept ? &ngx_posted_accept_events
                                    : &ngx_posted_events;
                /*
                   如果要在post队列中延后处理该事件，首先要判断它是新连接事件还是普通事件，以决定把它加入
                   到ngx_posted_accept_events队列或者ngx_postedL events队列中。关于post队列中的事件何时执行
                   */
                ngx_post_event(rev, queue);

            } else {
                /* 若不延迟处理，则直接调用事件的处理函数 */
                rev->handler(rev);
            }
        }

        /* 获取连接的写事件，写事件的处理逻辑过程与读事件类似 */
        wev = c->write;

        /* 连接有可写事件，且该写事件是active活跃的 */
        if ((revents & EPOLLOUT) && wev->active) {

            /* 检查写事件是否过期 */
            if (c->fd == -1 || wev->instance != instance) {

                /*
                 * the stale event from a file descriptor
                 * that was just closed in this iteration
                 */

                ngx_log_debug1(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                               "epoll: stale event %p", c);
                continue;
            }

            /* 写事件已准备就绪 */
            wev->ready = 1;
#if (NGX_THREADS)
            wev->complete = 1;
#endif

            /*
             * NGX_POST_EVENTS表示已准备就绪的事件需要延迟处理，
             * 根据accept标志位将事件加入到相应的队列中；
             */
            if (flags & NGX_POST_EVENTS) {

                //将这个事件添加到post队列中延后处理
                ngx_post_event(wev, &ngx_posted_events);

            } else {
                /* 若不延迟处理，则直接调用事件的处理函数 */
                wev->handler(wev);
            }
        }
    }

    return NGX_OK;
}


#if (NGX_HAVE_FILE_AIO)
/* 处理已完成的异步IO事件 */
static void
ngx_epoll_eventfd_handler(ngx_event_t *ev)
{
    int               n, events;
    long              i;
    uint64_t          ready;
    ngx_err_t         err;
    ngx_event_t      *e;
    ngx_event_aio_t  *aio;
    struct io_event   event[64]; /* 一次最多处理64个事件 */
    struct timespec   ts;

    ngx_log_debug0(NGX_LOG_DEBUG_EVENT, ev->log, 0, "eventfd handler");

    /* 获取已完成的事件数，并将其设置到ready */
    /* Each successful read(2) returns an 8-byte integer. A read(2) will fail with the error EINVAL if the size of
     *  the supplied buffer is less than 8 bytes.
     */
    n = read(ngx_eventfd, &ready, 8);

    err = ngx_errno;

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, ev->log, 0, "eventfd: %d", n);

    if (n != 8) {
        if (n == -1) {
            if (err == NGX_EAGAIN) {
                return;
            }

            ngx_log_error(NGX_LOG_ALERT, ev->log, err, "read(eventfd) failed");
            return;
        }

        ngx_log_error(NGX_LOG_ALERT, ev->log, 0,
                      "read(eventfd) returned only %d bytes", n);
        return;
    }

    ts.tv_sec = 0;
    ts.tv_nsec = 0;

    /* 遍历ready，处理异步IO事件 */
    while (ready) {

        /* 获取已完成的异步IO事件 */
        events = io_getevents(ngx_aio_ctx, 1, 64, event, &ts);

        ngx_log_debug1(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                       "io_getevents: %d", events);

        if (events > 0) {
            ready -= events; /* ready减去已经取出的事件数 */

             /* 处理已被取出的事件 */
            for (i = 0; i < events; i++) {

                ngx_log_debug4(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                               "io_event: %XL %XL %L %L",
                                event[i].data, event[i].obj,
                                event[i].res, event[i].res2);

                /* 获取异步IO事件对应的实际事件 */
                e = (ngx_event_t *) (uintptr_t) event[i].data;

                e->complete = 1;
                e->active = 0;
                e->ready = 1;

                aio = e->data;
                aio->res = event[i].res;

                /* 将实际事件加入到ngx_posted_event队列中等待处理 */
                ngx_post_event(e, &ngx_posted_events);
            }

            continue;
        }

        if (events == 0) {
            return;
        }

        /* events == -1 */
        ngx_log_error(NGX_LOG_ALERT, ev->log, ngx_errno,
                      "io_getevents() failed");
        return;
    }
}

#endif

/*创建模块*/
static void *
ngx_epoll_create_conf(ngx_cycle_t *cycle)
{
    ngx_epoll_conf_t  *epcf;

    epcf = ngx_palloc(cycle->pool, sizeof(ngx_epoll_conf_t));
    if (epcf == NULL) {
        return NULL;
    }

    epcf->events = NGX_CONF_UNSET;
    epcf->aio_requests = NGX_CONF_UNSET;

    return epcf;
}

/*初始化模块*/
static char *
ngx_epoll_init_conf(ngx_cycle_t *cycle, void *conf)
{
    ngx_epoll_conf_t *epcf = conf;

    ngx_conf_init_uint_value(epcf->events, 512);
    ngx_conf_init_uint_value(epcf->aio_requests, 32);

    return NGX_CONF_OK;
}
