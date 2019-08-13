/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui
* @software:  CLion
* @file:      ngx_proxy_protocol.h
* @date:      2018/2/28 上午9:15
* @desc:
*/

//
// Created by daemon.xie on 2018/2/28.
//

#ifndef NGINX_LEARNING_NGX_PROXY_PROTOCOL_H
#define NGINX_LEARNING_NGX_PROXY_PROTOCOL_H


#include <ngx_config.h>
#include <ngx_core.h>


#define NGX_PROXY_PROTOCOL_MAX_HEADER  107


u_char *ngx_proxy_protocol_read(ngx_connection_t *c, u_char *buf,
                                u_char *last);
u_char *ngx_proxy_protocol_write(ngx_connection_t *c, u_char *buf,
                                 u_char *last);

#endif //NGINX_LEARNING_NGX_PROXY_PROTOCOL_H
