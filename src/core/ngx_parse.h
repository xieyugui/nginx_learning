
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_PARSE_H_INCLUDED_
#define _NGX_PARSE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

//单位转换 k,m 转换
ssize_t ngx_parse_size(ngx_str_t *line);
off_t ngx_parse_offset(ngx_str_t *line);
//将y,M,w,d,h,m,s 转为秒   is_sec 是否从年开始，还是从月开始
ngx_int_t ngx_parse_time(ngx_str_t *line, ngx_uint_t is_sec);


#endif /* _NGX_PARSE_H_INCLUDED_ */
