/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_syslog.h
* @date:      2018/3/6 下午3:46
* @desc:
*/

//
// Created by daemon.xie on 2018/3/6.
//

#ifndef NGINX_LEARNING_NGX_SYSLOG_H
#define NGINX_LEARNING_NGX_SYSLOG_H

/**
# syslog表示使用syslog协议
# server=10.26.2.65 指明远程服务器地址，也可以指定本地
# facility=local7 指明设备管道使用local7
# tag=nginx 标签表示在日志文件中显示时候的标题
# severity=info 表示日志级别
access_log syslog:server=10.26.2.65,facility=local7,tag=nginx,severity=info;
 */

typedef struct {
    ngx_pool_t       *pool;
    ngx_uint_t        facility;
    ngx_uint_t        severity;
    ngx_str_t         tag;

    ngx_addr_t        server;
    ngx_connection_t  conn;
    unsigned          busy:1;
    unsigned          nohostname:1;
} ngx_syslog_peer_t;

//解析配置
char *ngx_syslog_process_conf(ngx_conf_t *cf, ngx_syslog_peer_t *peer);
u_char *ngx_syslog_add_header(ngx_syslog_peer_t *peer, u_char *buf);
void ngx_syslog_writer(ngx_log_t *log, ngx_uint_t level, u_char *buf,
                       size_t len);
ssize_t ngx_syslog_send(ngx_syslog_peer_t *peer, u_char *buf, size_t len);

#endif //NGINX_LEARNING_NGX_SYSLOG_H
