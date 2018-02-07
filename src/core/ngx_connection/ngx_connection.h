/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_connection.h
* @date:      2018/2/6 下午4:59
* @desc:
 *
 * https://github.com/xieyugui/reading-code-of-nginx-1.9.2/blob/master/nginx-1.9.2/src/core/ngx_connection.h
 * https://github.com/chronolaw/annotated_nginx/blob/master/nginx/src/core/ngx_connection.h
 * http://nginx.org/en/docs/http/ngx_http_core_module.html  查看官网 connection 配置说明
*/

//
// Created by daemon.xie on 2018/2/6.
//

#ifndef NGX_CONNECTION_NGX_CONNECTION_H
#define NGX_CONNECTION_NGX_CONNECTION_H

#include <ngx_config.h>
#include <ngx_core.h>

typedef struct ngx_listening_s ngx_listening_t;

// 监听端口数据结构
// 存储在ngx_cycle_t::listening数组里
// 主要成员: fd,backlog,rcvbuf,sndbuf,handler
// 由http模块用listen指令添加
struct ngx_listening_s {
    ngx_socket_t fd;// socket描述符（句柄）

    struct sockaddr *sockaddr;// 本地监听端口的socketaddr，也是ngx_connection中的local_sockaddr
    socklen_t socklen;// sockaddr长度
    size_t addr_text_max_len;// addr_text的最大长度
    ngx_str_t addr_text;// 文本形式的地址

    int type;// socket的类型，SOCK_STREAM 表示TCP

    int backlog;// TCP的backlog队列，即等待连接的队列
    int rcvbuf; // 内核接收缓冲区大小
    int sndbuf;// 内核发送缓冲区大小
#if (NGX_HAVE_KEEPALIVE_TUNABLE)
    int keepidle; //设置连接上如果没有数据发送的话，多久后发送keepalive探测分组
    int keepintvl; //前后两次探测之间的时间间隔
    int keepcnt; //关闭一个非活跃连接之前的最大重试次数
#endif

    // 重要函数，tcp连接成功时的回调函数
    // 对应于http模块是ngx_http_request.c:ngx_http_init_connection
    // stream模块是ngx_stream_init_connection
    ngx_conf_handler_pt handler;

    // 用于解决多个server监听相同端口的情况
    void *servers;

    ngx_log_t log;
    ngx_log_t *logp;

    size_t pool_size;
    size_t post_accept_buffer_size;
    /*
    TCP_DEFER ACCEPT选项将在建立TCP连接成功且接收到用户的请求数据后，才向对监听套接字感兴趣的进程发送事件通知，而连接建立成功后，
    如果post_accept_timeout秒后仍然没有收到的用户数据，则内核直接丢弃连接
    */ //ls->post_accept_timeout = cscf->client_header_timeout;  "client_header_timeout"设置
    ngx_msec_t post_accept_timeout;

    // 链表指针，多个ngx_listening_t组成一个单向链表
    ngx_listening_t *previous;

    // 监听端口对应的连接对象
    // 从cycle的内存池分配，但只用了read事件
    ngx_connection_t *connection;

    // worker进程的序号，用于reuseport
    ngx_uint_t worker;

    //下面这些标志位一般在ngx_init_cycle中初始化赋值
    /*
    标志位，为1则表示在当前监听句柄有效，且执行ngx- init—cycle时不关闭监听端口，为0时则正常关闭。该标志位框架代码会自动设置
    */
    unsigned  open:1;
    /*
      标志位，为1表示使用已有的ngx_cycle_t来初始化新的ngx_cycle_t结构体时，不关闭原先打开的监听端口，这对运行中升级程序很有用，
      remaln为o时，表示正常关闭曾经打开的监听端口。该标志位框架代码会自动设置，参见ngx_init_cycle方法
      */
    unsigned  remain:1;

    unsigned  ignore:1;

    unsigned bound:1;
    /* 表示当前监听句柄是否来自前一个进程（如升级Nginx程序），如果为1，则表示来自前一个进程。一般会保留之前已经设置好的套接字，不做改变 */
    unsigned inherited:1; // 从前一个nginx进程继承过来的
    unsigned nonblocking_accept:1;
    unsigned listen:1;  // 是否已经被监听
    unsigned nonblocking:1;
    unsigned shared:1;  //线程或者进程之间共享
    unsigned addr_ntop:1;
    unsigned wildcard:1;

#if (NGX_HAVE_INET6)
    unsigned            ipv6only:1;
#endif

    // 1.10新增reuseport支持，可以不再使用共享锁负载均衡，性能更高
    // 是否使用reuseport
    unsigned reuseport:1;
    // 是否已经设置了reuseport socket选项
    // ngx_open_listening_sockets
    unsigned add_reuseport:1;
    unsigned keepalive:2;

    // 延迟接受请求，只有真正收到数据内核才通知nginx，提高性能
    unsigned            deferred_accept:1;
    unsigned            delete_deferred:1;
    unsigned            add_deferred:1;
#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
    char               *accept_filter;
#endif
#if (NGX_HAVE_SETFIB)
    int                 setfib;
#endif

// 是否支持tcp fast open
// 可以优化tcp三次握手的延迟，提高响应速度
#if (NGX_HAVE_TCP_FASTOPEN)
    int                 fastopen;
#endif

};


//本连接记录日志时的级别，它占用了3位，取值范围是0-7，但实际上目前只定义了5个值。见ngx_connection_s->log_error
typedef enum {
    NGX_ERROR_ALERT = 0,
    NGX_ERROR_ERR,
    NGX_ERROR_INFO,
    NGX_ERROR_IGNORE_ECONNRESET,
    NGX_ERROR_IGNORE_EINVAL
} ngx_connection_log_error_e;

/*
linux 下是tcp_cork,上面的意思就是说，当使用sendfile函数时，tcp_nopush才起作用，它和指令tcp_nodelay是互斥的。tcp_cork是linux下
tcp/ip传输的一个标准了，这个标准的大概的意思是，一般情况下，在tcp交互的过程中，当应用程序接收到数据包后马上传送出去，不等待，
而tcp_cork选项是数据包不会马上传送出去，等到数据包最大时，一次性的传输出去，这样有助于解决网络堵塞，已经是默认了。
也就是说tcp_nopush = on 会设置调用tcp_cork方法，这个也是默认的，结果就是数据包不会马上传送出去，等到数据包最大时，一次性的传输出去，
这样有助于解决网络堵塞。
以快递投递举例说明一下（以下是我的理解，也许是不正确的），当快递东西时，快递员收到一个包裹，马上投递，这样保证了即时性，但是会
耗费大量的人力物力，在网络上表现就是会引起网络堵塞，而当快递收到一个包裹，把包裹放到集散地，等一定数量后统一投递，这样就是tcp_cork的
选项干的事情，这样的话，会最大化的利用网络资源，虽然有一点点延迟。
对于nginx配置文件中的tcp_nopush，默认就是tcp_nopush,不需要特别指定，这个选项对于www，ftp等大文件很有帮助
tcp_nodelay
        TCP_NODELAY和TCP_CORK基本上控制了包的“Nagle化”，Nagle化在这里的含义是采用Nagle算法把较小的包组装为更大的帧。
        John Nagle是Nagle算法的发明人，后者就是用他的名字来命名的，他在1984年首次用这种方法来尝试解决福特汽车公司的网络拥塞问题
            （欲了解详情请参看IETF RFC 896）。他解决的问题就是所谓的silly window syndrome，中文称“愚蠢窗口症候群”，具体含义是，
            因为普遍终端应用程序每产生一次击键操作就会发送一个包，而典型情况下一个包会拥有一个字节的数据载荷以及40个字节长的包头，
            于是产生4000%的过载，很轻易地就能令网络发生拥塞,。 Nagle化后来成了一种标准并且立即在因特网上得以实现。它现在已经成为缺省配置了，
            但在我们看来，有些场合下把这一选项关掉也是合乎需要的。
       现在让我们假设某个应用程序发出了一个请求，希望发送小块数据。我们可以选择立即发送数据或者等待产生更多的数据然后再一次发送两种策略。
           如果我们马上发送数据，那么交互性的以及客户/服务器型的应用程序将极大地受益。如果请求立即发出那么响应时间也会快一些。
           以上操作可以通过设置套接字的TCP_NODELAY = on 选项来完成，这样就禁用了Nagle 算法。
       另外一种情况则需要我们等到数据量达到最大时才通过网络一次发送全部数据，这种数据传输方式有益于大量数据的通信性能，典型的应用就是文件服务器
           。应用 Nagle算法在这种情况下就会产生问题。但是，如果你正在发送大量数据，你可以设置TCP_CORK选项禁用Nagle化，
           其方式正好同 TCP_NODELAY相反（TCP_CORK和 TCP_NODELAY是互相排斥的）。
tcp_nopush
语法：tcp_nopush on | off;
默认：tcp_nopush off;
配置块：http、server、location
在打开sendfile选项时，确定是否开启FreeBSD系统上的TCP_NOPUSH或Linux系统上的TCP_CORK功能。打开tcp_nopush后，将会在发送响应时把整个响应包头放到一个TCP包中发送。
*/

//表示如何使用TCP的nodelay特性
typedef enum {
    NGX_TCP_NODELAY_UNSET = 0,
    NGX_TCP_NODELAY_SET,
    NGX_TCP_NODELAY_DISABLED
} ngx_connection_tcp_nodelay_e;

//表示如何使用TCP的nopush特性
typedef enum {
    NGX_TCP_NOPUSH_UNSET = 0,
    NGX_TCP_NOPUSH_SET,
    NGX_TCP_NOPUSH_DISABLED
} ngx_connection_tcp_nopush_e;

#define NGX_LOWLEVEL_BUFFERED  0x0f
#define NGX_SSL_BUFFERED       0x01
#define NGX_HTTP_V2_BUFFERED   0x02

/*
 * ngx_event_t事件和ngx_connection_t连接是处理TCP连接的基础数据结构, 通过ngx_get_connection从连接池中获取一个ngx_connection_s结构，
 * 被动连接(客户端连接nginx)对应的数据结构是ngx_connection_s，主动连接(nginx连接后端服务器)对应的数据结构是ngx_peer_connection_s
 */
struct ngx_connection_s {
    // data成员有两种用法
    // 未使用（空闲）时作为链表的后继指针，连接在ngx_cycle_t::free_connections里
    // 在http模块里保存ngx_http_request_t对象，标记连接对应的http请求
    // 在stream模块里保存ngx_stream_session_t对象
    /*
    连接未使用时，data成员用于充当连接池中空闲连接链表中的next指针(ngx_event_process_init)。
     当连接被使用时，data的意义由使用它的Nginx模块而定，如在HTTP框架中，data指向ngx_http_request_t请求
     在服务器端accept客户端连接成功(ngx_event_accept)后，会通过ngx_get_connection从连接池获取一个ngx_connection_t结构，
     也就是每个客户端连接对于一个ngx_connection_t结构，
     并且为其分配一个ngx_http_connection_t结构，ngx_connection_t->data = ngx_http_connection_t，见ngx_http_init_connection
    在子请求处理过程中，上层父请求r的data指向第一个r下层的子请求，例如第二层的r->connection->data指向其第三层的第一个
     创建的子请求r，c->data = sr见ngx_http_subrequest,在subrequest往客户端发送数据的时候，只有data指向的节点可以先发送出去
     listen过程中，指向原始请求ngx_http_connection_t(ngx_http_init_connection ngx_http_ssl_handshake),
     接收到客户端数据后指向ngx_http_request_t(ngx_http_wait_request_handler)
     http2协议的过程中，在ngx_http_v2_connection_t(ngx_http_v2_init)
 */
    void *data;
    //如果是文件异步i/o中的ngx_event_aio_t，则它来自ngx_event_aio_t->ngx_event_t(只有读),如果是网络事件中的event,
    // 则为ngx_connection_s中的event(包括读和写)
    ngx_event_t *read;//连接对应的读事件   赋值在ngx_event_process_init，空间是从ngx_cycle_t->read_event池子中获取的
    ngx_event_t *write;//连接对应的写事件，存储在ngx_cycle_t::write_events

    // 连接的socket描述符（句柄）
    // 需使用此描述符才能收发数据
    ngx_socket_t fd;

    // 接收数据的函数指针,直接接收网络字符流的方法
    ngx_recv_pt  recv;
    ngx_send_pt  send; //直接发送网络字符流的方法
    //以ngx_chain_t链表为参数来接收网络字符流的方法
    ngx_recv_chain_pt recv_chain;

    //以ngx_chain_t链表为参数来发送网络字符流的方法
    // linux下实际上是ngx_writev_chain.c:ngx_writev_chain
    //
    // 发送limit长度（字节数）的数据
    // 如果事件not ready，即暂不可写，那么立即返回，无动作
    // 要求缓冲区必须在内存里，否则报错
    // 最后返回消费缓冲区之后的链表指针
    // 发送出错、遇到again、发送完毕，这三种情况函数结束
    // 返回的是最后发送到的链表节点指针
    //
    // 发送后需要把已经发送过的节点都回收，供以后复用
    ngx_send_chain_pt send_chain;

    // 连接对应的ngx_listening_t监听对象
    // 通过这个指针可以获取到监听端口相关的信息
    // 反过来可以操作修改监听端口
    ngx_listening_t *listening;

    // 连接上已经发送的字节数
    // ngx_send.c里发送数据成功后增加
    // 在32位系统里最大4G，可以定义宏_FILE_OFFSET_BITS=64
    off_t sent;

    ngx_log_t *log;

    // 连接的内存池
    // 默认大小是256字节
    ngx_pool_t *pool;

    // socket的类型，SOCK_STREAM 表示TCP，
    int type;

    // 客户端的sockaddr
    struct sockaddr *sockaddr;
    socklen_t socklen;
    ngx_str_t addr_text;// 客户端的sockaddr，文本形式

    ngx_str_t proxy_protocol_addr;
    in_port_t proxy_protocol_port;

    // 给https协议用的成员
    // 定义在event/ngx_event_openssl.h
    // 里面包装了OpenSSL的一些定义
#if (NGX_SSL || NGX_COMPAT)
    ngx_ssl_connection_t  *ssl;
#endif

    // 本地监听端口的socketaddr，也就是listening中的sockaddr
    // 有的时候local_sockaddr可能是0
    // 需要调用ngx_connection_local_sockaddr才能获得真正的服务器地址
    struct sockaddr *local_sockaddr;
    socklen_t local_socklen;

    // 接收客户端发送数据的缓冲区
    // 与listening中的rcvbuf不同，这个是nginx应用层的
    // 在ngx_http_wait_request_handler里分配内存
    ngx_buf_t *buffer;

    //该字段用来将当前连接以双向链表元素的形式添加到ngx_cycle_t核心结构体的reusable_connections_queue双向链表中，表示可以重用的连接
    ngx_queue_t queue;

    /*
       连接使用次数。ngx_connection t结构体每次建立一条来自客户端的连接，或者用于主动向后端服务器发起连接时（ngx_peer_connection_t也使用它），
        number都会加l
     */
    ngx_atomic_uint_t   number;

    // 处理的请求次数，在ngx_http_create_request里增加
    // 用来控制长连接里可处理的请求次数，指令keepalive_requests
    // 在stream框架里暂未使用
    ngx_uint_t requests;

    /*
    缓存中的业务类型。任何事件消费模块都可以自定义需要的标志位。这个buffered字段有8位，最多可以同时表示8个不同的业务。第三方模
    块在自定义buffered标志位时注意不要与可能使用的模块定义的标志位冲突。目前openssl模块定义了一个标志位：
        #define NGX_SSL_BUFFERED    Ox01

        HTTP官方模块定义了以下标志位：
        #define NGX HTTP_LOWLEVEL_BUFFERED   0xf0
        #define NGX_HTTP_WRITE_BUFFERED       0x10
        #define NGX_HTTP_GZIP_BUFFERED        0x20
        #define NGX_HTTP_SSI_BUFFERED         0x01
        #define NGX_HTTP_SUB_BUFFERED         0x02
        #define NGX_HTTP_COPY_BUFFERED        0x04
        #define NGX_HTTP_IMAGE_BUFFERED       Ox08
    同时，对于HTTP模块而言，buffered的低4位要慎用，在实际发送响应的ngx_http_write_filter_module过滤模块中，低4位标志位为1则惫味着
    Nginx会一直认为有HTTP模块还需要处理这个请求，必须等待HTTP模块将低4位全置为0才会正常结束请求。检查低4位的宏如下：
        #define NGX_LOWLEVEL_BUFFERED  OxOf
     */
    unsigned buffered:8; //不为0，表示有数据没有发送完毕，ngx_http_request_t->out中还有未发送的报文

    /*
     本连接记录日志时的级别，它占用了3位，取值范围是0-7，但实际上目前只定义了5个值，由ngx_connection_log_error_e枚举表示，如下：
    typedef enum{
        NGX ERROR_AIERT=0，
        NGX ERROR_ERR,
        NGX ERROR_INFO，
        NGX ERROR_IGNORE_ECONNRESET,
        NGX ERROR—IGNORE EIb:fVAL
     }
     ngx_connection_log_error_e ;
     */
    unsigned log_error:3;

    /*
     * 每次处理完一个客户端请求后，都会ngx_add_timer(rev, c->listening->post_accept_timeout);
     * 读客户端连接的数据，在ngx_http_init_connection(ngx_connection_t *c)中的ngx_add_timer(rev,
     *    c->listening->post_accept_timeout)把读事件添加到定时器中，如果超时则置1
      每次ngx_unix_recv把内核数据读取完毕后，在重新启动add epoll，等待新的数据到来，同时会启动定时器ngx_add_timer(rev,
           c->listening->post_accept_timeout);
      如果在post_accept_timeout这么长事件内没有数据到来则超时，开始处理关闭TCP流程*/
    //当ngx_event_t->timedout置1的时候，该置也同时会置1，参考ngx_http_process_request_line  ngx_http_process_request_headers
    //在ngx_http_free_request中如果超时则会设置SO_LINGER来减少time_wait状态
    unsigned timedout:1; // 是否已经超时
    unsigned error:1;// 是否已经出错

    /*
      标志位，为1时表示连接已经销毁。这里的连接指是的TCP连接，而不是ngx_connection t结构体。当destroyed为1时，ngx_connection_t结
      构体仍然存在，但其对应的套接字、内存池等已经不可用
      */
    unsigned destroyed:1;  // 是否tcp连接已经被销毁

    unsigned idle:1; //为1时表示连接处于空闲状态，如keepalive请求中丽次请求之间的状态
    unsigned            reusable:1; //为1时表示连接可重用，它与上面的queue字段是对应使用的

    // tcp连接已经关闭
    // 可以回收复用
    // 手动置这个标志位可以强制关闭连接
    unsigned            close:1;
    unsigned            shared:1;

    unsigned            sendfile:1;// 正在发送文件

    /*
    标志位，如果为1，则表示只有在连接套接字对应的发送缓冲区必须满足最低设置的大小阅值时，事件驱动模块才会分发该事件。这与上文
    介绍过的ngx_handle_write_event方法中的lowat参数是对应的
     */
    unsigned            sndlowat:1;// 是否已经设置发送数据时epoll的响应阈值
    unsigned            tcp_nodelay:2;   /* ngx_connection_tcp_nodelay_e */ //域套接字默认是disable的
    unsigned            tcp_nopush:2;    /* ngx_connection_tcp_nopush_e */

    unsigned            need_last_buf:1;

#if (NGX_HAVE_AIO_SENDFILE || NGX_COMPAT)
    unsigned            busy_count:2;
#endif

#if (NGX_THREADS || NGX_COMPAT)
    ngx_thread_task_t  *sendfile_task;
#endif

};

//ngx_set_connection_log(r->connection, clcf->error_log)
#define ngx_set_connection_log(c, l)                                         \
                                                                             \
    c->log->file = l->file;                                                  \
    c->log->next = l->next;                                                  \
    c->log->writer = l->writer;                                              \
    c->log->wdata = l->wdata;                                                \
    if (!(c->log->log_level & NGX_LOG_DEBUG_CONNECTION)) {                   \
        c->log->log_level = l->log_level;                                    \
    }

// http/ngx_http.c:ngx_http_add_listening()里调用
// ngx_stream.c:ngx_stream_optimize_servers()里调用
// 添加到cycle的监听端口数组
ngx_listening_t *ngx_create_listening(ngx_conf_t *cf, struct sockaddr *sockaddr,
    socklen_t socklen);

// 1.10新函数，专为reuseport使用
//reuseport的意思：内核支持同一个端口可以有多个socket同时进行监听而不报错误
ngx_int_t ngx_clone_listening(ngx_conf_t *cf, ngx_listening_t *ls);
ngx_int_t ngx_set_inherited_sockets(ngx_cycle_t *cycle);

// ngx_cycle.c : init_cycle()里被调用
// 创建socket, bind/listen
ngx_int_t ngx_open_listening_sockets(ngx_cycle_t *cycle);

// ngx_init_cycle()里调用，在ngx_open_listening_sockets()之后
// 配置监听端口的rcvbuf/sndbuf等参数，调用setsockopt()
void ngx_configure_listening_sockets(ngx_cycle_t *cycle);

// 在ngx_master_process_exit里被调用(os/unix/ngx_process_cycle.c)
// 遍历监听端口列表，逐个删除监听事件
void ngx_close_listening_sockets(ngx_cycle_t *cycle);

// 关闭连接，删除epoll里的读写事件
// 释放连接，加入空闲链表，可以再次使用
void ngx_close_connection(ngx_connection_t *c);

// 1.10新函数
// 检查cycle里的连接数组，如果连接空闲则设置close标志位，关闭
void ngx_close_idle_connection(ngx_cycle_t *cycle);
ngx_int_t ngx_connection_local_sockadd(ngx_connection_t *c, ngx_str_t *s,
                                       ngx_uint_t port);
ngx_int_t ngx_tcp_nodelay(ngx_connection_t *c);
ngx_int_t ngx_connection_error(ngx_connection_t *c, ngx_err_t err, char *text);

// 从全局变量ngx_cycle里获取空闲链接，即free_connections链表
// 如果没有空闲连接，调用ngx_drain_connections释放一些可复用的连接
ngx_connection_t *ngx_get_connection(ngx_socket_t s, ngx_log_t *log);

// 释放一个连接，加入空闲链表
void ngx_free_connection(ngx_connection_t *c);

// 连接加入cycle的复用队列ngx_cycle->reusable_connections_queue
// 参数reusable表示是否可以复用，即加入队列
void ngx_reusable_connection(ngx_connection_t *c, ngx_uint_t reusable);

#endif //NGX_CONNECTION_NGX_CONNECTION_H
