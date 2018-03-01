/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_resolver.h
* @date:      2018/2/28 上午10:07
* @desc:
 * https://www.cnblogs.com/sddai/p/5703394.html
 * https://github.com/vislee/leevis.com/issues/106
 * http://blog.sina.com.cn/s/blog_7303a1dc0102vixb.html
*/

//
// Created by daemon.xie on 2018/2/28.
//

#include <ngx_config.h>
#include <ngx_core.h>

#ifndef NGINX_LEARNING_NGX_RESOLVER_H
#define NGINX_LEARNING_NGX_RESOLVER_H

#define NGX_RESOLVE_A         1 //A记录 ipv4
#define NGX_RESOLVE_CNAME     5 //CNAME记录：通常称别名解析
//Domain Name Pointer. 是一个指针记录，用于将一个IP地址映射到对应的主机名,也可以看成是A记录的反向,通过IP访问域名,原来是通过域名访问IP)
#define NGX_RESOLVE_PTR       12
// MX记录 ：MX（Mail Exchanger）记录是邮件交换记录，它指向一个邮件服务器，用于电子邮件系统发邮件时根据收信人的地址后缀来定位邮件服务器
#define NGX_RESOLVE_MX        15
//TXT记录：TXT记录，一般指某个主机名或域名的说明
#define NGX_RESOLVE_TXT       16
#if (NGX_HAVE_INET6)
#define NGX_RESOLVE_AAAA      28 //AAAA记录：该记录是将域名解析到一个指定的IPV6的IP上
#endif
//服务定位器
#define NGX_RESOLVE_SRV       33
//代表名称 DNAME 会为名称和其子名称产生别名，与 CNAME 不同，在其标签别名不会重复。但与 CNAME 记录相同的是，DNS将会继续尝试查找新的名字
#define NGX_RESOLVE_DNAME     39

#define NGX_RESOLVE_FORMERR   1
#define NGX_RESOLVE_SERVFAIL  2
#define NGX_RESOLVE_NXDOMAIN  3
#define NGX_RESOLVE_NOTIMP    4
#define NGX_RESOLVE_REFUSED   5
#define NGX_RESOLVE_TIMEDOUT  NGX_ETIMEDOUT


#define NGX_NO_RESOLVER       (void *) -1

#define NGX_RESOLVER_MAX_RECURSION    50


typedef struct ngx_resolver_s  ngx_resolver_t;

// resolver 信息，通过resolver指令制定的解析域名的服务器
typedef struct {
    ngx_connection_t         *udp;
    ngx_connection_t         *tcp; // tcp 连接的connection
    struct sockaddr          *sockaddr; // resolver 地址
    socklen_t                 socklen;
    ngx_str_t                 server;  // resolver域名
    ngx_log_t                 log;
    ngx_buf_t                *read_buf; // tcp 读缓存区
    ngx_buf_t                *write_buf; // tcp 写缓存区
    ngx_resolver_t           *resolver;
} ngx_resolver_connection_t;


typedef struct ngx_resolver_ctx_s  ngx_resolver_ctx_t;

typedef void (*ngx_resolver_handler_pt)(ngx_resolver_ctx_t *ctx);


typedef struct {
    struct sockaddr          *sockaddr;
    socklen_t                 socklen;
    ngx_str_t                 name;
    u_short                   priority;
    u_short                   weight;
} ngx_resolver_addr_t;


typedef struct {
    ngx_str_t                 name;
    u_short                   priority;
    u_short                   weight;
    u_short                   port;
} ngx_resolver_srv_t;


typedef struct {
    ngx_str_t                 name;
    u_short                   priority;
    u_short                   weight;
    u_short                   port;

    ngx_resolver_ctx_t       *ctx;
    ngx_int_t                 state;

    ngx_uint_t                naddrs;
    ngx_addr_t               *addrs;
} ngx_resolver_srv_name_t;


typedef struct {
    ngx_rbtree_node_t         node;
    ngx_queue_t               queue;

    /* PTR: resolved name, A: name to resolve */
    u_char                   *name;

#if (NGX_HAVE_INET6)
    /* PTR: IPv6 address to resolve (IPv4 address is in rbtree node key) */
    struct in6_addr           addr6;
#endif

    u_short                   nlen; // 要解析的域名的长度
    u_short                   qlen; // 发送dns解析命令的长度

    u_char                   *query; // dns解析命令字符串
#if (NGX_HAVE_INET6)
    u_char                   *query6;
#endif

    union {
        in_addr_t             addr;
        in_addr_t            *addrs;
        u_char               *cname;
        ngx_resolver_srv_t   *srvs;
    } u;

    u_char                    code;
    u_short                   naddrs;
    u_short                   nsrvs;
    u_short                   cnlen;

#if (NGX_HAVE_INET6)
    union {
        struct in6_addr       addr6;
        struct in6_addr      *addrs6;
    } u6;

    u_short                   naddrs6;
#endif

    time_t                    expire;
    time_t                    valid;
    uint32_t                  ttl;

    unsigned                  tcp:1; // 是否通过tcp方式方法查询请求
#if (NGX_HAVE_INET6)
    unsigned                  tcp6:1;
#endif

    ngx_uint_t                last_connection; // dns域名服务器地址下标

    ngx_resolver_ctx_t       *waiting;
} ngx_resolver_node_t;


struct ngx_resolver_s {
    /* has to be pointer because of "incomplete type" */
    ngx_event_t              *event;
    void                     *dummy;
    ngx_log_t                *log;

    /* event ident must be after 3 pointers as in ngx_connection_t */
    ngx_int_t                 ident;

    /* simple round robin DNS peers balancer */
    // ngx_resolver_connection_t 结构的数组，resolver服务地址
    ngx_array_t               connections;
    ngx_uint_t                last_connection;

    ngx_rbtree_t              name_rbtree;
    ngx_rbtree_node_t         name_sentinel;

    ngx_rbtree_t              srv_rbtree;
    ngx_rbtree_node_t         srv_sentinel;

    ngx_rbtree_t              addr_rbtree;
    ngx_rbtree_node_t         addr_sentinel;

    ngx_queue_t               name_resend_queue;
    ngx_queue_t               srv_resend_queue;
    ngx_queue_t               addr_resend_queue;

    ngx_queue_t               name_expire_queue;
    ngx_queue_t               srv_expire_queue;
    ngx_queue_t               addr_expire_queue;

#if (NGX_HAVE_INET6)
    ngx_uint_t                ipv6;                 /* unsigned  ipv6:1; */
    ngx_rbtree_t              addr6_rbtree;
    ngx_rbtree_node_t         addr6_sentinel;
    ngx_queue_t               addr6_resend_queue;
    ngx_queue_t               addr6_expire_queue;
#endif

    time_t                    resend_timeout;
    time_t                    tcp_timeout; // tcp 连接超时时间
    time_t                    expire;
    time_t                    valid;

    ngx_uint_t                log_level;
};


struct ngx_resolver_ctx_s {
    ngx_resolver_ctx_t       *next;
    ngx_resolver_t           *resolver;
    ngx_resolver_node_t      *node;

    /* event ident must be after 3 pointers as in ngx_connection_t */
    ngx_int_t                 ident;

    ngx_int_t                 state;
    ngx_str_t                 name; // 要解析的域名
    ngx_str_t                 service;

    time_t                    valid;
    ngx_uint_t                naddrs;
    ngx_resolver_addr_t      *addrs; // 解析的IP
    // 如果域名本来就是ip，则把ip解析到该内存。不用向dns服务器发起请求
    ngx_resolver_addr_t       addr;
    struct sockaddr_in        sin;

    ngx_uint_t                count;
    ngx_uint_t                nsrvs;
    ngx_resolver_srv_name_t  *srvs;

    ngx_resolver_handler_pt   handler;  // 解析后的回调函数
    void                     *data;
    ngx_msec_t                timeout; // 域名解析超时

    ngx_uint_t                quick;  /* unsigned  quick:1; */
    ngx_uint_t                recursion;
    ngx_event_t              *event;  // 只用做超时ngx_resolver_set_timeout函数初始化
};

// 创建域名解析结构体，用于后续的域名解析
ngx_resolver_t *ngx_resolver_create(ngx_conf_t *cf, ngx_str_t *names,
                                    ngx_uint_t n);

// 初始化域名解析上下文ngx_resolver_ctx_t
ngx_resolver_ctx_t *ngx_resolve_start(ngx_resolver_t *r,
                                      ngx_resolver_ctx_t *temp);

// 构建dns查询并发起查询请求
ngx_int_t ngx_resolve_name(ngx_resolver_ctx_t *ctx);

// dns查询结束，清理资源
void ngx_resolve_name_done(ngx_resolver_ctx_t *ctx);
ngx_int_t ngx_resolve_addr(ngx_resolver_ctx_t *ctx);
void ngx_resolve_addr_done(ngx_resolver_ctx_t *ctx);
char *ngx_resolver_strerror(ngx_int_t err);


#endif //NGINX_LEARNING_NGX_RESOLVER_H
