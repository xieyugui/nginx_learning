/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_connection.c
* @date:      2018/2/6 下午4:59
* @desc:
*/

//
// Created by daemon.xie on 2018/2/6.
//

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>

// os/unix/ngx_os.h  统一的IO结构
// 操作系统提供的底层数据收发接口
// ngx_posix_init.c里初始化为linux的底层接口
// 在epoll模块的ngx_epoll_init里设置
//
// typedef struct {
//     ngx_recv_pt        recv;
//     ngx_recv_chain_pt  recv_chain;
//     ngx_recv_pt        udp_recv;
//     ngx_send_pt        send;
//     ngx_send_chain_pt  send_chain;
//     ngx_uint_t         flags;
// } ngx_os_io_t;
ngx_os_io_t ngx_io;

// 检查最多32个在可复用连接队列里的元素
// 设置为连接关闭c->close = 1;
// 调用事件的处理函数，里面会检查c->close
// 这样就会调用ngx_http_close_connection
// 释放连接，加入空闲链表，可以再次使用
// 早期函数参数是void，1.12改成cycle
static void ngx_drain_connections(ngx_cycle_t *cycle);

// http/ngx_http.c:ngx_http_add_listening()里调用
// 添加到cycle的监听端口数组，只是添加，没有其他动作
// 添加后需要用ngx_open_listening_sockets()才能打开端口监听
ngx_listening_t *
ngx_create_listening(ngx_conf_t *cf, struct sockaddr *sockaddr, socklen_t socklen)
{
    size_t len;
    ngx_listening_t *ls;
    struct sockaddr *sa;
    u_char text[NGX_SOCKADDR_STRLEN];

    // 从cycle的listening数组中获取一个空闲的
    ls = ngx_array_push(&cf->cycle->listening);
    if (le == NULL) {
        return NULL;
    }

    ngx_memzero(ls, sizeof(ngx_listening_t));//初始化

    // 根据socklen分配sockaddr的内存空间
    sa = ngx_palloc(cf->pool, socklen);
    if (sa == NULL) {
        return NULL;
    }

    ngx_memcpy(sa, sockaddr, socklen);

    ls->sockaddr = sa;
    ls->socklen = socklen;

    //sockaddr[]转换成可读的字符串形式 /* 将二进制的地址结构，转换为文本格式 */
    len = ngx_sock_ntop(sa, socklen, text, NGX_SOCKADDR_STRLEN, 1);
    ls->addr_text.len = len;

    switch (ls->sockaddr->sa_family) {
#if (NGX_HAVE_INET6)
        case AF_INET6:
        ls->addr_text_max_len = NGX_INET6_ADDRSTRLEN;
        break;
#endif
#if (NGX_HAVE_UNIX_DOMAIN)
        case AF_UNIX:
        ls->addr_text_max_len = NGX_UNIX_ADDRSTRLEN;
        len++;
        break;
#endif
        case AF_INET:
            ls->addr_text_max_len = NGX_INET_ADDRSTRLEN;
            break;
        default://按照最大端口来估值  ipv4 255.255.255.255:65535
            ls->addr_text_max_len = NGX_SOCKADDR_STRLEN;
            break;
    }
    // 从内存池 pool 分配大小为 size 的内存块，并返回其地址, 不考虑对齐问题
    ls->addr_text.data = ngx_pnalloc(cf->pool, len);
    if (ls->addr_text.data == NULL) {
        return NULL;
    }

    ngx_memcpy(ls->addr_text.data, text, len);

    ls->fd = (ngx_socket_t) -1;

    /**
    SOCK_STREAM： 提供面向连接的稳定数据传输，即TCP协议。
    SOCK_DGRAM： 使用不连续不可靠的数据包连接。
    SOCK_SEQPACKET： 提供连续可靠的数据包连接。
    SOCK_RAW： 提供原始网络协议存取。
    SOCK_RDM： 提供可靠的数据包连接。
    SOCK_PACKET： 与网络驱动程序直接通信。
     */
    ls->type = SOCK_STREAM;

    ls->backlog = NGX_LISTEN_BACKLOG;
    ls->rcvbuf = -1;
    ls->sndbuf = -1;

#if (NGX_HAVE_SETFIB)
    ls->setfib = -1;
#endif

#if (NGX_HAVE_TCP_FASTOPEN)
    ls->fastopen = -1;
#endif

    return ls;
}

// 1.10新函数，专为reuseport使用
// 拷贝了worker数量个的监听结构体
// 在ngx_stream_optimize_servers等函数创建端口时调用
//reuseport的意思：内核支持同一个端口可以有多个socket同时进行监听而不报错误
ngx_int_t
ngx_clone_listening(ngx_conf_t *cf, ngx_listening_t *ls)
{
#if (NGX_HAVE_REUSEPORT)
    ngx_int_t n;
    ngx_core_conf_t *ccf;
    ngx_listening_t ols;

    // 监听指令需要配置了reuseport
    if (!ls->reuseport) {
        return NGX_OK;
    }

    ols = *ls;

    ccf = (ngx_core_conf_t *) ngx_get_conf(cf->cycle->conf_ctx, ngx_core_module);

    for (n = 0; n < ccf->worker_processes; n++) {
        ls = ngx_array_push(&cf->cycle->listening);
        if (ls == NULL) {
            return NGX_ERROR;
        }

        // 完全拷贝结构体
        *ls = ols;

        // 设置worker的序号
        // 被克隆的对象的worker是0
        //
        // worker的使用是在ngx_event.c:ngx_event_process_init
        // 只有worker id是本worker的listen才会enable
        ls->worker = n;
    }

#endif

    return NGX_OK;
}

// 根据传递过来的socket描述符，使用系统调用获取之前设置的参数
// 该函数从参数cycle(后续调用ngx_init_cycle()函数后全局变量ngx_cycle会指向该参数)的listening数组中逐一
//    对每个元素(ngx_listening_t结构)进行初始化，即初始化除fd字段外的其他的字段。
ngx_int_t
ngx_set_inherited_sockets(ngx_cycle_t *cycle)
{
    size_t len;
    ngx_uint_t i;
    ngx_listening_t *ls;
    socklen_t olen;
#if (NGX_HAVE_DEFERRED_ACCEPT || NGX_HAVE_TCP_FASTOPEN)
    ngx_err_t                  err;
#endif
    //SO_ACCEPTFILTER 接受过滤器  基本与deferred_accept 作用一样
#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
    struct accept_filter_arg   af;
#endif
#if (NGX_HAVE_DEFERRED_ACCEPT && defined TCP_DEFER_ACCEPT)
    int                        timeout;
#endif
#if (NGX_HAVE_REUSEPORT)
    int                        reuseport;
#endif

    // 遍历监听端口数组，里面应该有之前传递过来的socket
    ls = cycle->listening.elts;//取出cycle->listening数组中的数据地址
    for(i = 0; i < cycle->listening.nelts; i++) {
        // 监听端口的实际地址，先分配内存
        ls[i].sockaddr = ngx_palloc(cycle->pool, sizeof(ngx_sockaddr_t));
        if (ls[i].sockaddr == NULL) {
            return NGX_ERROR;
        }

        ls[i].socklen = sizeof(ngx_sockaddr_t);
        //getsockname succ:0.0.0.0:5000 系统调用，获取socket的地址
        if (getsockname(ls[i].fd, ls[i].sockaddr, &ls[i].socklen) == -1) {
            ngx_log_error(NGX_LOG_CRIT, cycle->log, ngx_socket_errno,
                          "getsockname() of the inherited "
                                  "socket #%d failed", ls[i].fd);
            ls[i].ignore = 1;
            continue;
        }

        if (ls[i].socklen > (socklen_t) sizeof(ngx_sockaddr_t)) {
            ls[i].socklen = sizeof(ngx_sockaddr_t);
        }

        switch (ls[i].sockaddr->sa_family) {
#if (NGX_HAVE_INET6)
            case AF_INET6:
            ls[i].addr_text_max_len = NGX_INET6_ADDRSTRLEN;
            len = NGX_INET6_ADDRSTRLEN + sizeof("[]:65535") - 1;
            break;
#endif

#if (NGX_HAVE_UNIX_DOMAIN)
            case AF_UNIX:
            ls[i].addr_text_max_len = NGX_UNIX_ADDRSTRLEN;
            len = NGX_UNIX_ADDRSTRLEN;
            break;
#endif
            case AF_INET:
                ls[i].addr_text_max_len = NGX_INET_ADDRSTRLEN;
                len = NGX_INET_ADDRSTRLEN + sizeof(":65535") - 1;
                break;
            default:
                ngx_log_error(NGX_LOG_CRIT, cycle->log, ngx_socket_errno,
                              "the inherited socket #%d has "
                                      "an unsupported protocol family", ls[i].fd);
                //直接忽视
                ls[i].ignore = 1;
                continue;
        }

        ls[i].addr_text.data = ngx_pnalloc(cycle->pool, len);
        if (ls[i].addr_text.data == NULL) {
            return NGX_ERROR;
        }

        // socket地址转换为字符串
        len = ngx_sock_ntop(ls[i].sockaddr, ls[i].socklen,
                            ls[i].addr_text.data, len, 1);
        if (len == 0) {
            return NGX_ERROR;
        }

        ls[i].addr_text.len = len;

        ls[i].backlog = NGX_LISTEN_BACKLOG;

        // 逐个获取socket的各个参数
        olen = sizeof(int);

        // type，即tcp或udp
        if (getsockopt(ls[i].fd, SOL_SOCKET, SO_TYPE, (void *) &ls[i].type,
                       &olen)
            == -1)
        {
            ngx_log_error(NGX_LOG_CRIT, cycle->log, ngx_socket_errno,
                          "getsockopt(SO_TYPE) %V failed", &ls[i].addr_text);
            ls[i].ignore = 1;
            continue;
        }

        olen = sizeof(int);

        if (getsockopt(ls[i].fd, SOL_SOCKET, SO_RCVBUF, (void *) &ls[i].rcvbuf,
                       &olen)
            == -1)
        {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                          "getsockopt(SO_RCVBUF) %V failed, ignored",
                          &ls[i].addr_text);

            ls[i].rcvbuf = -1;
        }

        olen = sizeof(int);

        if (getsockopt(ls[i].fd, SOL_SOCKET, SO_SNDBUF, (void *) &ls[i].sndbuf,
                       &olen)
            == -1)
        {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                          "getsockopt(SO_SNDBUF) %V failed, ignored",
                          &ls[i].addr_text);

            ls[i].sndbuf = -1;
        }

#if 0
        /* SO_SETFIB is currently a set only option */

#if (NGX_HAVE_SETFIB)

        olen = sizeof(int);

        if (getsockopt(ls[i].fd, SOL_SOCKET, SO_SETFIB,
                       (void *) &ls[i].setfib, &olen)
            == -1)
        {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                          "getsockopt(SO_SETFIB) %V failed, ignored",
                          &ls[i].addr_text);

            ls[i].setfib = -1;
        }

#endif
#endif

#if (NGX_HAVE_REUSEPORT)

        reuseport = 0;
        olen = sizeof(int);

        if (getsockopt(ls[i].fd, SOL_SOCKET, SO_REUSEPORT,
                       (void *) &reuseport, &olen)
            == -1)
        {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                          "getsockopt(SO_REUSEPORT) %V failed, ignored",
                          &ls[i].addr_text);

        } else {
            ls[i].reuseport = reuseport ? 1 : 0;
        }

#endif
        // udp不检查下面的fastopen等参数
        if (ls[i].type != SOCK_STREAM) {
            continue;
        }

#if (NGX_HAVE_TCP_FASTOPEN)

        olen = sizeof(int);
        // tcp专用的fastopen
        if (getsockopt(ls[i].fd, IPPROTO_TCP, TCP_FASTOPEN,
                       (void *) &ls[i].fastopen, &olen)
            == -1)
        {
            err = ngx_socket_errno;

            if (err != NGX_EOPNOTSUPP && err != NGX_ENOPROTOOPT) {
                ngx_log_error(NGX_LOG_NOTICE, cycle->log, err,
                              "getsockopt(TCP_FASTOPEN) %V failed, ignored",
                              &ls[i].addr_text);
            }

            ls[i].fastopen = -1;
        }

#endif

#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)

        ngx_memzero(&af, sizeof(struct accept_filter_arg));
        olen = sizeof(struct accept_filter_arg);
        // tcp专用的defered accept
        if (getsockopt(ls[i].fd, SOL_SOCKET, SO_ACCEPTFILTER, &af, &olen)
            == -1)
        {
            err = ngx_socket_errno;

            if (err == NGX_EINVAL) {
                continue;
            }

            ngx_log_error(NGX_LOG_NOTICE, cycle->log, err,
                          "getsockopt(SO_ACCEPTFILTER) for %V failed, ignored",
                          &ls[i].addr_text);
            continue;
        }

        if (olen < sizeof(struct accept_filter_arg) || af.af_name[0] == '\0') {
            continue;
        }

        ls[i].accept_filter = ngx_palloc(cycle->pool, 16);
        if (ls[i].accept_filter == NULL) {
            return NGX_ERROR;
        }

        (void) ngx_cpystrn((u_char *) ls[i].accept_filter,
                           (u_char *) af.af_name, 16);
#endif

#if (NGX_HAVE_DEFERRED_ACCEPT && defined TCP_DEFER_ACCEPT)

        timeout = 0;
        olen = sizeof(int);

        if (getsockopt(ls[i].fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &timeout, &olen)
            == -1)
        {
            err = ngx_socket_errno;

            if (err == NGX_EOPNOTSUPP) {
                continue;
            }

            ngx_log_error(NGX_LOG_NOTICE, cycle->log, err,
                          "getsockopt(TCP_DEFER_ACCEPT) for %V failed, ignored",
                          &ls[i].addr_text);
            continue;
        }

        if (olen < sizeof(int) || timeout == 0) {
            continue;
        }

        ls[i].deferred_accept = 1;
#endif
    }

    return NGX_OK;
}

// ngx_cycle.c : init_cycle()里被调用
// 创建socket, bind/listen
ngx_int_t
ngx_open_listening_sockets(ngx_cycle_t *cycle)
{
    int reuseaddr;
    ngx_uint_t i, tries, failed;
    ngx_err_t err;
    ngx_log_t *log;
    ngx_socket_t s;
    ngx_listening_t *ls;

    reuseaddr = 1;
#if (NGX_SUPPRESS_WARN)
    failed = 0;
#endif

    log = cycle->log;

    for (tries = 5; tries; tries--) {

        // 遍历监听端口链表
        // 如果设置了reuseport，那么一个端口会有worker个克隆的结构体
        ls = cycle->listening.elts;
        for(i = 0; i < cycle->listening.nelts; i++) {

            if (ls[i].ignore) {
                continue;
            }

#if (NGX_HAVE_REUSEPORT)

            // 检查是否已经设置的标志位，只是用在inherited的时候，见注释
            // 在init_cycle里设置
            if (ls[i].add_reuseport) {
                int  reuseport = 1;

                if (setsockopt(ls[i].fd, SOL_SOCKET, SO_REUSEPORT,
                               (const void *) &reuseport, sizeof(int))
                    == -1)
                {
                    ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                                  "setsockopt(SO_REUSEPORT) %V failed, ignored",
                                  &ls[i].addr_text);
                }
                // 标志位清零
                ls[i].add_reuseport = 0;

            }
#endif

            // 已经打开的端口不再处理
            if (ls[i].fd != (ngx_socket_t) -1) {
                continue;
            }

            // 从前一个nginx进程继承过来的
            // 已经打开，所以也不需要再处理
            // 在ngx_set_inherited_sockets里操作
            if (ls[i].inherited) {
                continue;
            }

            // 创建socket 当protocol为0时，会自动选择type类型对应的默认协议
            s = ngx_socket(ls[i].sockaddr->sa_family, ls[i].type, 0);


            if (s == (ngx_socket_t) -1) {
                ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                              ngx_socket_n " %V failed", &ls[i].addr_text);
                return NGX_ERROR;
            }

            // 设置SO_REUSEADDR选项 SO_REUSEADDR是让端口释放后立即就可以被再次使用
            /*
              默认情况下,server重启,调用socket,bind,然后listen,会失败.因为该端口正在被使用.
              如果设定SO_REUSEADDR,那么server重启才会成功.因此,
              所有的TCP server都必须设定此选项,用以应对server重启的现象.
              */
            if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
                           (const void *) &reuseaddr, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                              "setsockopt(SO_REUSEADDR) %V failed",
                              &ls[i].addr_text);

                if (ngx_close_socket(s) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  ngx_close_socket_n " %V failed",
                                  &ls[i].addr_text);
                }

                return NGX_ERROR;
            }

            // 设置SO_REUSEPORT选项
            // 这样多个进程可以打开相同的端口，由内核负责负载均衡
#if (NGX_HAVE_REUSEPORT)

            if (ls[i].reuseport && !ngx_test_config) {
                int  reuseport;

                reuseport = 1;

                if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT,
                               (const void *) &reuseport, sizeof(int))
                    == -1)
                {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  "setsockopt(SO_REUSEPORT) %V failed",
                                  &ls[i].addr_text);

                    if (ngx_close_socket(s) == -1) {
                        ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                      ngx_close_socket_n " %V failed",
                                      &ls[i].addr_text);
                    }

                    return NGX_ERROR;
                }
            }
#endif

//IPV6_V6ONLY 只使用IPV6（因为IPV6 会同时监听IPV4 IPV6，会导致原先的监听IPV4冲突）
#if (NGX_HAVE_INET6 && defined IPV6_V6ONLY)

            if (ls[i].sockaddr->sa_family == AF_INET6) {
                int  ipv6only;

                ipv6only = ls[i].ipv6only;

                if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY,
                               (const void *) &ipv6only, sizeof(int))
                    == -1)
                {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  "setsockopt(IPV6_V6ONLY) %V failed, ignored",
                                  &ls[i].addr_text);
                }
            }
#endif

            /* TODO: close on exit */
            if (!(ngx_event_flags & NGX_USE_IOCP_EVENT)) {

                // 设置为nonblocking，使用FIONBIO
                if (ngx_nonblocking(s) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  ngx_nonblocking_n " %V failed",
                                  &ls[i].addr_text);

                    if (ngx_close_socket(s) == -1) {
                        ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                      ngx_close_socket_n " %V failed",
                                      &ls[i].addr_text);
                    }

                    return NGX_ERROR;
                }
            }

            ngx_log_debug2(NGX_LOG_DEBUG_CORE, log, 0,
                           "bind() %V #%d ", &ls[i].addr_text, s);

            //绑定地址
            if (bind(s, ls[i].sockaddr, ls[i].socklen) == -1) {
                err = ngx_socket_errno;

                if (err != NGX_EADDRINUSE || !ngx_test_config) {
                    ngx_log_error(NGX_LOG_EMERG, log, err,
                                  "bind() to %V failed", &ls[i].addr_text);
                }

                if (ngx_close_socket(s) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  ngx_close_socket_n " %V failed",
                                  &ls[i].addr_text);
                }

                if (err != NGX_EADDRINUSE) {
                    return NGX_ERROR;
                }

                if (!ngx_test_config) {
                    failed = 1;
                }

                continue;
            }

#if (NGX_HAVE_UNIX_DOMAIN)

            if (ls[i].sockaddr->sa_family == AF_UNIX) {
                mode_t   mode;
                u_char  *name;

                name = ls[i].addr_text.data + sizeof("unix:") - 1;
                mode = (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

                if (chmod((char *) name, mode) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                                  "chmod() \"%s\" failed", name);
                }

                if (ngx_test_config) {
                    if (ngx_delete_file(name) == NGX_FILE_ERROR) {
                        ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                                      ngx_delete_file_n " %s failed", name);
                    }
                }
            }
#endif

            // 检查是否是tcp，即SOCK_STREAM
            if (ls[i].type != SOCK_STREAM) {
                // 如果不是tcp，即udp，那么无需监听
                ls[i].fd = s;
                // 继续处理下一个监听结构体
                continue;
            }

            // 开始监听，设置backlog
            if (listen(s, ls[i].backlog) == -1) {
                err = ngx_socket_errno;

                /*
                 * on OpenVZ after suspend/resume EADDRINUSE
                 * may be returned by listen() instead of bind(), see
                 * https://bugzilla.openvz.org/show_bug.cgi?id=2470
                 */

                if (err != NGX_EADDRINUSE || !ngx_test_config) {
                    ngx_log_error(NGX_LOG_EMERG, log, err,
                                  "listen() to %V, backlog %d failed",
                                  &ls[i].addr_text, ls[i].backlog);
                }

                if (ngx_close_socket(s) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                                  ngx_close_socket_n " %V failed",
                                  &ls[i].addr_text);
                }

                if (err != NGX_EADDRINUSE) {
                    return NGX_ERROR;
                }

                if (!ngx_test_config) {
                    failed = 1;
                }

                continue;
            }
            // 设置已经监听标志
            ls[i].listen = 1;

            // 设置socket描述符
            ls[i].fd = s;

        }

        if (!failed) {
            break;
        }

        /* TODO: delay configurable */

        ngx_log_error(NGX_LOG_NOTICE, log, 0,
                      "try again to bind() after 500ms");
        //sleep sleep....
        ngx_msleep(500);

    }

    if (failed) {
        ngx_log_error(NGX_LOG_EMERG, log, 0, "still could not bind()");
        return NGX_ERROR;
    }

    return NGX_OK;
}

// 配置监听端口的rcvbuf/sndbuf等参数，调用setsockopt()
void
ngx_configure_listening_sockets(ngx_cycle_t *cycle)
{
    int value;
    ngx_uint_t i;
    ngx_listening_t *ls;

#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
    struct accept_filter_arg   af;
#endif

    ls = cycle->listening.elts;
    for(i = 0; i < cycle->listening.nelts; i++) {
        ls[i].log = *ls[i].logp;

        if(ls[i].rcvbuf != -1) {
            if (setsockopt(ls[i].fd, SOL_SOCKET, SO_RCVBUF,
                           (const void *) &ls[i].rcvbuf, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(SO_RCVBUF, %d) %V failed, ignored",
                              ls[i].rcvbuf, &ls[i].addr_text);
            }
        }

        if (ls[i].sndbuf != -1) {
            if (setsockopt(ls[i].fd, SOL_SOCKET, SO_SNDBUF,
                           (const void *) &ls[i].sndbuf, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(SO_SNDBUF, %d) %V failed, ignored",
                              ls[i].sndbuf, &ls[i].addr_text);
            }
        }

        if (ls[i].keepalive) {
            value = (ls[i].keepalive == 1) ? 1 : 0;

            if (setsockopt(ls[i].fd, SOL_SOCKET, SO_KEEPALIVE,
                           (const void *) &value, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(SO_KEEPALIVE, %d) %V failed, ignored",
                              value, &ls[i].addr_text);
            }
        }

#if (NGX_HAVE_KEEPALIVE_TUNABLE)
        //空闲 keepalive 之后多久没发送数据
        if (ls[i].keepidle) {
            value = ls[i].keepidle;

#if (NGX_KEEPALIVE_FACTOR)
            value *= NGX_KEEPALIVE_FACTOR;
#endif
            /*
            设置SO_KEEPALIVE选项来开启KEEPALIVE，然后通过TCP_KEEPIDLE、TCP_KEEPINTVL和TCP_KEEPCNT
                设置keepalive的开始时间、间隔、次数等参数
             */
            if (setsockopt(ls[i].fd, IPPROTO_TCP, TCP_KEEPIDLE,
                           (const void *) &value, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(TCP_KEEPIDLE, %d) %V failed, ignored",
                              value, &ls[i].addr_text);
            }
        }
        //间隔
        if (ls[i].keepintvl) {
            value = ls[i].keepintvl;

#if (NGX_KEEPALIVE_FACTOR)
            value *= NGX_KEEPALIVE_FACTOR;
#endif

            if (setsockopt(ls[i].fd, IPPROTO_TCP, TCP_KEEPINTVL,
                           (const void *) &value, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                             "setsockopt(TCP_KEEPINTVL, %d) %V failed, ignored",
                             value, &ls[i].addr_text);
            }
        }
        //重试次数
        if (ls[i].keepcnt) {
            if (setsockopt(ls[i].fd, IPPROTO_TCP, TCP_KEEPCNT,
                           (const void *) &ls[i].keepcnt, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(TCP_KEEPCNT, %d) %V failed, ignored",
                              ls[i].keepcnt, &ls[i].addr_text);
            }
        }

#endif

#if (NGX_HAVE_SETFIB)
        if (ls[i].setfib != -1) {
            if (setsockopt(ls[i].fd, SOL_SOCKET, SO_SETFIB,
                           (const void *) &ls[i].setfib, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(SO_SETFIB, %d) %V failed, ignored",
                              ls[i].setfib, &ls[i].addr_text);
            }
        }
#endif

// tcp fast open， 可以优化tcp三次握手的延迟，提高响应速度
#if (NGX_HAVE_TCP_FASTOPEN)
        if (ls[i].fastopen != -1) {
            if (setsockopt(ls[i].fd, IPPROTO_TCP, TCP_FASTOPEN,
                           (const void *) &ls[i].fastopen, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(TCP_FASTOPEN, %d) %V failed, ignored",
                              ls[i].fastopen, &ls[i].addr_text);
            }
        }
#endif

#if 0
        if (1) {
            int tcp_nodelay = 1;

            if (setsockopt(ls[i].fd, IPPROTO_TCP, TCP_NODELAY,
                       (const void *) &tcp_nodelay, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(TCP_NODELAY) %V failed, ignored",
                              &ls[i].addr_text);
            }
        }
#endif
        // 已经监听标志, backlog
        if (ls[i].listen) {

            /* change backlog via listen() */
            /* 在创建子进程前listen，这样可以保证创建子进程后，所有的进程都能获取这个fd，这样所有进程就能个accept客户端连接 */
            // 修改了tcp选项，重新监听
            if (listen(ls[i].fd, ls[i].backlog) == -1) {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "listen() to %V, backlog %d failed, ignored",
                              &ls[i].addr_text, ls[i].backlog);
            }
        }

#if (NGX_HAVE_DEFERRED_ACCEPT)
// 这个是freebsd的设置
#ifdef SO_ACCEPTFILTER
        //当前socket是否需要被取消延迟接受
        if (ls[i].delete_deferred) {
            if (setsockopt(ls[i].fd, SOL_SOCKET, SO_ACCEPTFILTER, NULL, 0)
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(SO_ACCEPTFILTER, NULL) "
                              "for %V failed, ignored",
                              &ls[i].addr_text);

                if (ls[i].accept_filter) {
                    ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                                  "could not change the accept filter "
                                  "to \"%s\" for %V, ignored",
                                  ls[i].accept_filter, &ls[i].addr_text);
                }

                continue;
            }

            ls[i].deferred_accept = 0;
        }
        //当前socket是否需要被设置为延迟接受
        if (ls[i].add_deferred) {
            ngx_memzero(&af, sizeof(struct accept_filter_arg));
            (void) ngx_cpystrn((u_char *) af.af_name,
                               (u_char *) ls[i].accept_filter, 16);

            if (setsockopt(ls[i].fd, SOL_SOCKET, SO_ACCEPTFILTER,
                           &af, sizeof(struct accept_filter_arg))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(SO_ACCEPTFILTER, \"%s\") "
                              "for %V failed, ignored",
                              ls[i].accept_filter, &ls[i].addr_text);
                continue;
            }

            ls[i].deferred_accept = 1;
        }

#endif

// 这个是linux的设置
#ifdef TCP_DEFER_ACCEPT
        // deferred，只有socket上有数据可读才接受连接
        // 由内核检查客户端连接的数据发送情况
        // 减少了epoll的调用次数，可以提高性能
        //当前socket是否需要被设置为延迟接受  当前socket是否需要被取消延迟接受
        if (ls[i].add_deferred || ls[i].delete_deferred) {

            if (ls[i].add_deferred) {
                /*
                 * There is no way to find out how long a connection was
                 * in queue (and a connection may bypass deferred queue at all
                 * if syncookies were used), hence we use 1 second timeout
                 * here.
                 */
                value = 1;

            } else {
                value = 0;
            }
            /*
            TCP_DEFER_ACCEPT 优化 使用TCP_DEFER_ACCEPT可以减少用户程序hold的连接数，也可以减少用户调用epoll_ctl和epoll_wait的次数，从而提高了程序的性能。
            设置listen套接字的TCP_DEFER_ACCEPT选项后， 只当一个链接有数据时是才会从accpet中返回（而不是三次握手完成)。所以节省了一次读第一个http请求包的过程，减少了系统调用

            查询资料，TCP_DEFER_ACCEPT是一个很有趣的选项，
            Linux 提供的一个特殊 setsockopt ,　在 accept 的 socket 上面，只有当实际收到了数据，才唤醒正在 accept 的进程，可以减少一些无聊的上下文切换。代码如下。
            val = 5;
            setsockopt(srv_socket->fd, SOL_TCP, TCP_DEFER_ACCEPT, &val, sizeof(val)) ;
            里面 val 的单位是秒，注意如果打开这个功能，kernel 在 val 秒之内还没有收到数据，不会继续唤醒进程，而是直接丢弃连接。
            经过测试发现，设置TCP_DEFER_ACCEPT选项后，服务器受到一个CONNECT请求后，操作系统不会Accept，也不会创建IO句柄。操作系统应该在若干秒，(但肯定远远大于上面设置的1s) 后，
            会释放相关的链接。但没有同时关闭相应的端口，所以客户端会一直以为处于链接状态。如果Connect后面马上有后续的发送数据，那么服务器会调用Accept接收这个链接端口。
            感觉了一下，这个端口设置对于CONNECT链接上来而又什么都不干的攻击方式处理很有效。我们原来的代码都是先允许链接，然后再进行超时处理，比他这个有点Out了。不过这个选项可能会导致定位某些问题麻烦。
            timeout = 0表示取消 TCP_DEFER_ACCEPT选项
            性能四杀手：内存拷贝，内存分配，进程切换，系统调用。TCP_DEFER_ACCEPT 对性能的贡献，就在于 减少系统调用了。
            */
            if (setsockopt(ls[i].fd, IPPROTO_TCP, TCP_DEFER_ACCEPT,
                           &value, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(TCP_DEFER_ACCEPT, %d) for %V failed, "
                              "ignored",
                              value, &ls[i].addr_text);

                continue;
            }
        }

        if (ls[i].add_deferred) {
            ls[i].deferred_accept = 1;
        }

#endif

#endif /* NGX_HAVE_DEFERRED_ACCEPT */

#if (NGX_HAVE_IP_RECVDSTADDR)
        //表示当前监听句柄是否支持通配：主要是通配UDP等接受数据
        if (ls[i].wildcard
            && ls[i].type == SOCK_DGRAM
            && ls[i].sockaddr->sa_family == AF_INET)
        {
            value = 1;
            //随UDP数据报接收目的的地址
            if (setsockopt(ls[i].fd, IPPROTO_IP, IP_RECVDSTADDR,
                           (const void *) &value, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(IP_RECVDSTADDR) "
                              "for %V failed, ignored",
                              &ls[i].addr_text);
            }
        }

#elif (NGX_HAVE_IP_PKTINFO)

        if (ls[i].wildcard
            && ls[i].type == SOCK_DGRAM
            && ls[i].sockaddr->sa_family == AF_INET)
        {
            value = 1;
            //传递一条包含pktinfo结构(该结构提供一些来访包的相关信息)的IP_PKTINFO辅助信息
            if (setsockopt(ls[i].fd, IPPROTO_IP, IP_PKTINFO,
                           (const void *) &value, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(IP_PKTINFO) "
                              "for %V failed, ignored",
                              &ls[i].addr_text);
            }
        }

#endif

#if (NGX_HAVE_INET6 && NGX_HAVE_IPV6_RECVPKTINFO)

        if (ls[i].wildcard
            && ls[i].type == SOCK_DGRAM
            && ls[i].sockaddr->sa_family == AF_INET6)
        {
            value = 1;

            if (setsockopt(ls[i].fd, IPPROTO_IPV6, IPV6_RECVPKTINFO,
                           (const void *) &value, sizeof(int))
                == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_socket_errno,
                              "setsockopt(IPV6_RECVPKTINFO) "
                              "for %V failed, ignored",
                              &ls[i].addr_text);
            }
        }

#endif

    }
    return;
}

// 关闭连接，删除epoll里的读写事件
// 释放连接，加入空闲链表，可以再次使用
void
ngx_close_listening_sockets(ngx_cycle_t *cycle)
{
    ngx_uint_t i;
    ngx_listening_t *ls;
    ngx_connection_t *c;

    //对于IOCP事件模型的socket，这里不做任何处理
    if (ngx_event_flags & NGX_USE_IOCP_EVENT) {
        return ;
    }

    //因为这里关闭了所有监听sockets，因此这里不再持有互斥锁的任何相关变量
    ngx_accept_mutex_held = 0;
    ngx_use_accept_mutex = 0;

    ls = cycle->listening.elts;
    for(i = 0; i < cycle->listening.nelts; i++) {

        c = ls[i].connection;

        if(c) {
            //c->read->active为真，说明该事件已经被注册用于接收IO通知，因此这里需要将该事件删除
            if(c->read->active) {
                if(ngx_event_flags & NGX_USE_EPOLL_EVENT) {
                    ngx_del_event(c->read, NGX_READ_EVENT, 0);
                } else {
                    ngx_del_event(c->read, NGX_READ_EVENT, NGX_CLOSE_EVENT);
                }

            }

            // 释放连接，加入空闲链表
            ngx_free_connection(c);

            c->fd = (ngx_socket_t) -1;
        }

        ngx_log_debug2(NGX_LOG_DEBUG_CORE, cycle->log, 0,
                       "close listening %V #%d ", &ls[i].addr_text, ls[i].fd);
        //关闭socket
        if (ngx_close_socket(ls[i].fd) == -1) {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_socket_errno,
                          ngx_close_socket_n " %V failed", &ls[i].addr_text);
        }

#if (NGX_HAVE_UNIX_DOMAIN)
        //对于unix域socket,如果是属于最后一个进程退出，则需要删除本地产生的unix域文件
        if (ls[i].sockaddr->sa_family == AF_UNIX
            && ngx_process <= NGX_PROCESS_MASTER
            && ngx_new_binary == 0)
        {
            u_char *name = ls[i].addr_text.data + sizeof("unix:") - 1;

            if (ngx_delete_file(name) == NGX_FILE_ERROR) {
                ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_socket_errno,
                              ngx_delete_file_n " %s failed", name);
            }
        }

#endif

        ls[i].fd = (ngx_socket_t) -1;
    }

    cycle->listening.nelts = 0;
}

// 从全局变量ngx_cycle里获取空闲链接，即free_connections链表
// 如果没有空闲连接，调用ngx_drain_connections释放一些可复用的连接
/**
ngx_cycle->files: 本字段是nginx事件模块初始化时预先分配的一个足够大的空间，用于将来存放所有正在使用的连接（指针）,
    并且可以通过socket fd来索引该ngx_connection_t连接对象
ngx_cycle->connections: 预先分配了一个足够大的空间来在这空间分配ngx_connection_t对象
ngx_cycle->free_connections: 本字段存放了所有空闲状态的ngx_connection_t对象，通过ngx_connection_t.data字段连接起来
cycle->read_events: 预先分配的足够大的ngx_event_t对象空间
 */
ngx_connection_t *
ngx_get_connection(ngx_socket_t s, ngx_log_t *log)
{
    ngx_uint_t instance;
    ngx_event_t *rev, *wev;
    ngx_connection_t *c;

    //socket句柄s不能大于ngx_cycle->files_n，否则是没有地方存放的，也不可能会出现这种情况。若出现，则肯定发生了错误
    if (ngx_cycle->files && (ngx_uint_t) s >= ngx_cycle->files_n) {
        ngx_log_error(NGX_LOG_ALERT, log, 0,
                      "the new socket has number %d, "
                              "but only %ui files are available",
                      s, ngx_cycle->files_n);
        return NULL;
    }

    //从free_connections中取出一个ngx_connection_t对象，如果当前已经没有空闲，则通过ngx_drain_connections()释放长连接的
    //   方式来获得空闲连接。如果还是不能或者，直接返回NULL
    c = ngx_cycle->free_connections;

    if (c == NULL) {
        ngx_drain_connections((ngx_cycle_t *) ngx_cycle);
        c = ngx_cycle->free_connections;
    }

    if (c == NULL) {
        ngx_log_error(NGX_LOG_ALERT, log, 0,
                      "%ui worker_connections are not enough",
                      ngx_cycle->connection_n);

        return NULL;
    }

    //指向连接池中下一个未用的节点
    ngx_cycle->free_connections = c->data;
    ngx_cycle->free_connection_n--;

    //将获取到的长连接存放进ngx_cycle->files[s]中
    if (ngx_cycle->files && ngx_cycle->files[s] == NULL) {
        ngx_cycle->files[s] = c;
    }

    // 暂时保存读写事件对象
    rev = c->read;
    wev = c->write;

    // 清空连接对象
    // 注意这时fd、sent、type等字段、计数器、标志都变成了0
    ngx_memzero(c, sizeof(ngx_connection_t));

    // 恢复读写事件对象
    c->read = rev;
    c->write = wev;

    // 设置连接的socket
    c->fd = s;
    c->log = log;

    //used to detect the stale events in kqueue and epoll
    //置instance标志，用于检查连接是否失效
    instance = rev->instance;

    ngx_memzero(rev, sizeof(ngx_event_t));
    ngx_memzero(wev, sizeof(ngx_event_t));

    // 置instance标志，用于检查连接是否失效 一般情况下是用rev->instance与另外保存的一个instance进行对比，如果不相等，则说明是一个
    // stale事件。因此这里对instance进行取反，表明当前指定的这个events是属于过期事件，不应该被处理
    // TODO ??
    rev->instance = !instance;
    wev->instance = !instance;

    rev->index = NGX_INVALID_INDEX;
    wev->index = NGX_INVALID_INDEX;

    // 读写事件对象的data保存了连接对象
    rev->data = c;
    wev->data = c;

    // 设置写事件的标志
    // 获取到空闲连接，设置为可写状态 用于区分读写事件
    wev->write = 1;

    return c;
}

// 释放一个连接，加入空闲链表
//此函数用于释放ngx_connection_t连接，将其插入到ngx_cycle->free_connections链表头，
//    并且如果该connection存放在ngx_cycle->files[c->fd]中,则从该位置移除
void
ngx_free_connection(ngx_connection_t *c)
{
    c->data = ngx_cycle->free_connections;
    ngx_cycle->free_connections = c;
    ngx_cycle->free_connection_n++;

    if (ngx_cycle->files && ngx_cycle->files[c->fd] == c) {
        ngx_cycle->files[c->fd] = NULL;
    }

}

// 关闭连接，删除epoll里的读写事件
// 释放连接，加入空闲链表，可以再次使用
void
ngx_close_connection(ngx_connection_t *c)
{
    ngx_err_t err;
    ngx_uint_t log_error, level;
    ngx_socket_t  fd;

    //如果c->fd == -1，说明连接已经关闭
    if (c->fd == (ngx_socket_t) -1) {
        ngx_log_error(NGX_LOG_ALERT, c->log, 0, "connection already closed");
        return;
    }

    //移除connection上关联的读写定时器事件

    // 读事件在定时器里，需要删除
    if (c->read->timer_set) {
        ngx_del_timer(c->read);
    }

    // 写事件在定时器里，需要删除
    if(c->write->timer_set) {
        ngx_del_timer(c->write);
    }

    //如果不是共享connection的话，移除该connection上关联的读写事件
    if (!c->shared) {
        if (ngx_del_conn) {
            ngx_del_conn(c, NGX_CLOSE_EVENT);
        } else {
            if (c->read->active || c->read->disabled) {
                ngx_del_event(c->read, NGX_READ_EVENT, NGX_CLOSE_EVENT);
            }

            if (c->write->active || c->write->disabled) {
                ngx_del_event(c->write, NGX_WRITE_EVENT, NGX_CLOSE_EVENT);
            }
        }
    }

    //移除该连接已经投递到队列中的事件
    if (c->read->posted) {
        ngx_delete_posted_event(c->read);
    }

    if (c->write->posted) {
        ngx_delete_posted_event(c->write);
    }

    c->read->closed = 1;
    c->write->closed = 1;

    // 回收连接 参数reusable表示是否可以复用，即加入队列
    // 连接加入cycle的复用队列ngx_cycle->reusable_connections_queue
    ngx_reusable_connection(c, 0);

    log_error = c->log_error;

    ngx_free_connection(c);

    fd = c->fd;
    c->fd = (ngx_socket_t) -1;

    //非共享connection的话，需要关闭对应的fd
    if (c->shared) {
        return ;
    }

    // 关闭socket
    if (ngx_close_socket(fd) == -1) {

        err = ngx_socket_errno;

        if (err == NGX_ECONNRESET || err == NGX_ENOTCONN) {

            switch (log_error) {

                case NGX_ERROR_INFO:
                    level = NGX_LOG_INFO;
                    break;

                case NGX_ERROR_ERR:
                    level = NGX_LOG_ERR;
                    break;

                default:
                    level = NGX_LOG_CRIT;
            }

        } else {
            level = NGX_LOG_CRIT;
        }

        ngx_log_error(level, c->log, err, ngx_close_socket_n " %d failed", fd);

    }

}

// 连接加入cycle的复用队列ngx_cycle->reusable_connections_queue
// 参数reusable表示是否可以复用，即加入队列
//函数主要用于在reusable为true，即表示该连接需要马上被复用，因此这里会先从队列中移除，然后再重新加入到可复用连接队列中
void
ngx_reusable_connection(ngx_connection_t *c, ngx_uint_t reusable)
{
    ngx_log_debug1(NGX_LOG_DEBUG_CORE, c->log, 0,
                   "reusable connection: %ui", reusable);

    // 连接已经加入了队列，需要移出
    if (c->reusable) {
        ngx_queue_remove(&c->queue);
        ngx_cycle->reusable_connections_n--;

#if (NGX_STAT_STUB)
        (void) ngx_atomic_fetch_add(ngx_stat_waiting, -1);
#endif
    }

    // 设置标志位，是否已经加入队列
    c->reusable = reusable;

    // 要求加入队列，插入队列头
    if (reusable) {
        ngx_queue_insert_head((ngx_queue_t *) &ngx_cycle->reusable_connections_queue, &c->queue);
        ngx_cycle->reusable_connections_n++;
#if (NGX_STAT_STUB)
        (void) ngx_atomic_fetch_add(ngx_stat_waiting, 1);
#endif
    }
}

// 从全局变量ngx_cycle里获取空闲链接，即free_connections链表
// 如果没有空闲连接，调用ngx_drain_connections释放一些可复用的连接
// 检查最多32个在可复用连接队列里的元素
// 设置为连接关闭c->close = 1;
// 调用事件的处理函数，里面会检查c->close
// 这样就会调用ngx_http_close_connection
// 释放连接，加入空闲链表，可以再次使用

//从ngx_cycle->reusable_connections_queue中释放长连接，释放完成后加入到空闲连接池，以供后续新连接使用
static void
ngx_drain_connections(ngx_cycle_t *cycle)
{
    ngx_uint_t i, n;
    ngx_queue_t *q;
    ngx_connection_t *c;

    // 早期的nginx只检查32次，避免过多消耗时间
    n = ngx_max(ngx_min(32, cycle->reusable_connections_n / 8), 1);

    for (i = 0; i < n; i++) {
        if (ngx_queue_empty(&cycle->reusable_connections_queue)) {
            break;
        }

        // 取出队列末尾的连接对象，必定是c->reusable == true
        q = ngx_queue_last(&cycle->reusable_connections_queue);
        c = ngx_queue_data(q, ngx_connection_t, queue);

        ngx_log_debug0(NGX_LOG_DEBUG_CORE, c->log, 0,
                       "reusing connection");

        // 注意！设置为连接关闭
        c->close = 1;

        //通过读操作可以判断连接是否正常，如果不正常的话，就会把该ngx_close_connection->ngx_free_connection释放出来，这样
        //如果之前free_connections上没有空余ngx_connection_t，c = ngx_cycle->free_connections;
        // 就可以获取到刚才释放出来的ngx_connection_t
        c->read->handler(c->read);
    }
}

// 检查cycle里的连接数组，如果连接空闲则设置close标志位，关闭
void
ngx_close_idle_connections(ngx_cycle_t *cycle)
{
    ngx_uint_t i;
    ngx_connection_t *c;

    c = cycle->connections;

    for (i = 0; i < cycle->connection_n; i++) {

        // 如果连接空闲则设置close标志位，关闭
        if(cp[i].fd != (ngx_socket_t) -1 && c[i].idle) {
            c[i].close = 1;
            c[i].read->handler(c[i].read);
        }
    }
}

// 获取服务器的ip地址信息
ngx_int_t
ngx_connection_local_sockaddr(ngx_connection_t *c, ngx_str_t *s, ngx_uint_t port)
{
    socklen_t  len;
    ngx_uint_t addr;
    ngx_sockaddr_t sa;
    struct sockaddr_in *sin;
#if (NGX_HAVE_INET6)
    ngx_uint_t            i;
    struct sockaddr_in6  *sin6;
#endif

    addr = 0;

    if (c->local_socklen) {
        switch (c->local_sockaddr->sa_family) {

#if (NGX_HAVE_INET6)
            case AF_INET6:
            sin6 = (struct sockaddr_in6 *) c->local_sockaddr;

            for (i = 0; addr == 0 && i < 16; i++) {
                addr |= sin6->sin6_addr.s6_addr[i];
            }

            break;
#endif

#if (NGX_HAVE_UNIX_DOMAIN)
            case AF_UNIX:
            addr = 1;
            break;
#endif

            default: /* AF_INET */
                sin = (struct sockaddr_in *) c->local_sockaddr;
                addr = sin->sin_addr.s_addr;
                break;
        }
    }

    //收件检查c->local_sockaddr保存的是否是一个有效的IP地址(addr不为0）
    //如果是无效的IP地址，则通过getsockname()来获取，保存到c->local_sockaddr
    //listen *:80 或者listen 80都会满足，这是服务器listen端的IP地址只能通过getsockname获取
    if(addr == 0) {

        len = sizeof(ngx_sockaddr_t);

        if (getsockname(c->fd, &sa.sockaddr, &len) == -1) {
            ngx_connection_error(c, ngx_socket_errno, "getsockname() failed");
            return NGX_ERROR;
        }

        c->local_sockaddr = ngx_palloc(c->pool, len);
        if (c->local_sockaddr == NULL) {
            return NGX_ERROR;
        }

        ngx_memcpy(c->local_sockaddr, &sa, len);

        c->local_socklen = len;
    }

    // 不传入字符串则直接结束，节约计算
    if (s == NULL) {
        return NGX_OK;
    }

    // 格式化地址字符串   将c->local_sockaddr地址通过ngx_sock_ntop()函数转换成字符串表示形式，返回
    s->len = ngx_sock_ntop(c->local_sockaddr, c->local_socklen,
                           s->data, s->len, port);

    return NGX_OK;
}

//设置tcp_nodelay
ngx_int_t
ngx_tcp_nodelay(ngx_connection_t *c)
{
    int tcp_nodelay;

    if (c->tcp_nodelay != NGX_TCP_NODELAY_UNSET) {
        return NGX_OK;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, c->log, 0, "tcp_nodelay");

    tcp_nodelay = 1;


    if (setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY,
                   (const void *) &tcp_nodelay, sizeof(int))
        == -1)
    {
#if (NGX_SOLARIS)
        if (c->log_error == NGX_ERROR_INFO) {

            /* Solaris returns EINVAL if a socket has been shut down */
            c->log_error = NGX_ERROR_IGNORE_EINVAL;

            ngx_connection_error(c, ngx_socket_errno,
                                 "setsockopt(TCP_NODELAY) failed");

            c->log_error = NGX_ERROR_INFO;

            return NGX_ERROR;
        }
#endif

        ngx_connection_error(c, ngx_socket_errno,
                             "setsockopt(TCP_NODELAY) failed");
        return NGX_ERROR;
    }

    c->tcp_nodelay = NGX_TCP_NODELAY_SET;

    return NGX_OK;
}

//此函数主要用于打印ngx_connection_t中的日志信息
ngx_int_t
ngx_connection_error(ngx_connection_t *c, ngx_err_t err, char *text)
{
    ngx_uint_t  level;

    /* Winsock may return NGX_ECONNABORTED instead of NGX_ECONNRESET */

    if ((err == NGX_ECONNRESET
#if (NGX_WIN32)
                || err == NGX_ECONNABORTED
#endif
        ) && c->log_error == NGX_ERROR_IGNORE_ECONNRESET)
    {
        return 0;
    }

#if (NGX_SOLARIS)
    if (err == NGX_EINVAL && c->log_error == NGX_ERROR_IGNORE_EINVAL) {
        return 0;
    }
#endif

    if (err == 0
        || err == NGX_ECONNRESET
        #if (NGX_WIN32)
        || err == NGX_ECONNABORTED
        #else
        || err == NGX_EPIPE
        #endif
        || err == NGX_ENOTCONN
        || err == NGX_ETIMEDOUT
        || err == NGX_ECONNREFUSED
        || err == NGX_ENETDOWN
        || err == NGX_ENETUNREACH
        || err == NGX_EHOSTDOWN
        || err == NGX_EHOSTUNREACH)
    {
        switch (c->log_error) {

            case NGX_ERROR_IGNORE_EINVAL:
            case NGX_ERROR_IGNORE_ECONNRESET:
            case NGX_ERROR_INFO:
                level = NGX_LOG_INFO;
                break;

            default:
                level = NGX_LOG_ERR;
        }

    } else {
        level = NGX_LOG_ALERT;
    }

    ngx_log_error(level, c->log, err, text);

    return NGX_ERROR;
}
