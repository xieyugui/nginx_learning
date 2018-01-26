
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_INET_H_INCLUDED_
#define _NGX_INET_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


#define NGX_INET_ADDRSTRLEN   (sizeof("255.255.255.255") - 1)
#define NGX_INET6_ADDRSTRLEN                                                 \
    (sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255") - 1)
#define NGX_UNIX_ADDRSTRLEN                                                  \
    (sizeof("unix:") - 1 +                                                   \
     sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path))

#if (NGX_HAVE_UNIX_DOMAIN)
#define NGX_SOCKADDR_STRLEN   NGX_UNIX_ADDRSTRLEN
#elif (NGX_HAVE_INET6)
#define NGX_SOCKADDR_STRLEN   (NGX_INET6_ADDRSTRLEN + sizeof("[]:65535") - 1)
#else
#define NGX_SOCKADDR_STRLEN   (NGX_INET_ADDRSTRLEN + sizeof(":65535") - 1)
#endif

/* compatibility */
#define NGX_SOCKADDRLEN       sizeof(ngx_sockaddr_t)

/**
sockaddr和sockaddr_in包含的数据都是一样的，但他们在使用上有区别：
程序员不应操作sockaddr，sockaddr是给操作系统用的
程序员应使用sockaddr_in来表示地址，sockaddr_in区分了地址和端口，使用更方便。

 */
typedef union {
    struct sockaddr           sockaddr;
    struct sockaddr_in        sockaddr_in;
#if (NGX_HAVE_INET6)
    struct sockaddr_in6       sockaddr_in6;
#endif
#if (NGX_HAVE_UNIX_DOMAIN)
    struct sockaddr_un        sockaddr_un;
#endif
} ngx_sockaddr_t;


typedef struct {
    in_addr_t                 addr;
    in_addr_t                 mask;
} ngx_in_cidr_t;


#if (NGX_HAVE_INET6)

typedef struct {
    struct in6_addr           addr;
    struct in6_addr           mask;
} ngx_in6_cidr_t;

#endif


typedef struct {
    ngx_uint_t                family;
    union {
        ngx_in_cidr_t         in;
#if (NGX_HAVE_INET6)
        ngx_in6_cidr_t        in6;
#endif
    } u;
} ngx_cidr_t;


typedef struct {
    struct sockaddr          *sockaddr;
    socklen_t                 socklen;
    ngx_str_t                 name;
} ngx_addr_t;


typedef struct {
    ngx_str_t                 url; //保存IP地址+端口信息（e.g. 192.168.124.129:8011 或 money.163.com）
    ngx_str_t                 host; //保存IP地址信息 //proxy_pass  http://10.10.0.103:8080/tttxx; 中的10.10.0.103
    ngx_str_t                 port_text; //保存port字符串
    ngx_str_t                 uri;

    in_port_t                 port;
    //默认端口（当no_port字段为真时，将默认端口赋值给port字段， 默认端口通常是80）
    in_port_t                 default_port;
    int                       family;//address family, AF_xxx  //AF_UNIX代表域套接字  AF_INET代表普通网络套接字

    unsigned                  listen:1; //ngx_http_core_listen中置1 //是否为指监听类的设置
    unsigned                  uri_part:1;
    unsigned                  no_resolve:1;//根据情况决定是否解析域名（将域名解析到IP地址）

    unsigned                  no_port:1;//标识url中没有显示指定端口(为1时没有指定)  uri中是否有指定端口
    unsigned                  wildcard:1; //如listen  *:80则该位置1 //标识是否使用通配符（e.g. listen *:8000;）

    socklen_t                 socklen;//sizeof(struct sockaddr_in)
    ngx_sockaddr_t            sockaddr; //sockaddr_in结构指向它
    //数组大小是naddrs字段；每个元素对应域名的IP地址信息(struct sockaddr_in)，在函数中赋值（ngx_inet_resolve_host()）
    ngx_addr_t               *addrs;
    ngx_uint_t                naddrs; //url对应的IP地址个数,IP格式的地址将默认为1

    char                     *err;//错误信息字符串
} ngx_url_t;

// 校验text是否就是ip地址
in_addr_t ngx_inet_addr(u_char *text, size_t len);
#if (NGX_HAVE_INET6)
ngx_int_t ngx_inet6_addr(u_char *p, size_t len, u_char *addr);
size_t ngx_inet6_ntop(u_char *p, u_char *text, size_t len);
#endif
//sockaddr[]转换成可读的字符串形式 /* 将二进制的地址结构，转换为文本格式 */
size_t ngx_sock_ntop(struct sockaddr *sa, socklen_t socklen, u_char *text,
    size_t len, ngx_uint_t port);

/*地址由二进制数转换为点分十进制*/
size_t ngx_inet_ntop(int family, void *addr, u_char *text, size_t len);

// 计算ip地址和掩码
ngx_int_t ngx_ptocidr(ngx_str_t *text, ngx_cidr_t *cidr);

// 用于确定是否在给定的CIDR范围内发生IPv4地址
ngx_int_t ngx_cidr_match(struct sockaddr *sa, ngx_array_t *cidrs);


ngx_int_t ngx_parse_addr(ngx_pool_t *pool, ngx_addr_t *addr, u_char *text,
    size_t len);
ngx_int_t ngx_parse_addr_port(ngx_pool_t *pool, ngx_addr_t *addr,
    u_char *text, size_t len);

/**
    ngx_parse_url()调用ngx_parse_inet_url()
    ngx_parse_inet_url()调用ngx_inet_resolve_host()
    ngx_inet_resolve_host()调用gethostbyname()
    gethostbyname()函数就是通过域名获取IP的函数
*/
ngx_int_t ngx_parse_url(ngx_pool_t *pool, ngx_url_t *u);

//封装了getaddrinfo函数，是同步阻塞解析  根据请求中的host来解析域名对应的IP
ngx_int_t ngx_inet_resolve_host(ngx_pool_t *pool, ngx_url_t *u);

//对比两个sockaddr
ngx_int_t ngx_cmp_sockaddr(struct sockaddr *sa1, socklen_t slen1,
    struct sockaddr *sa2, socklen_t slen2, ngx_uint_t cmp_port);

//通过sockadd 获取port
in_port_t ngx_inet_get_port(struct sockaddr *sa);
void ngx_inet_set_port(struct sockaddr *sa, in_port_t port);


#endif /* _NGX_INET_H_INCLUDED_ */
