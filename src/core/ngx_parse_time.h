
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_PARSE_TIME_H_INCLUDED_
#define _NGX_PARSE_TIME_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

//解析HTTP头的时间 将字符串转为秒
time_t ngx_parse_http_time(u_char *value, size_t len);

/* compatibility */
#define ngx_http_parse_time(value, len)  ngx_parse_http_time(value, len)


#endif /* _NGX_PARSE_TIME_H_INCLUDED_ */
