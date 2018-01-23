
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_MURMURHASH_H_INCLUDED_
#define _NGX_MURMURHASH_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

//MurmurHash算法：高运算性能，低碰撞率；MurmurHash只适用于已知长度的、长度比较长的字符
uint32_t ngx_murmur_hash2(u_char *data, size_t len);


#endif /* _NGX_MURMURHASH_H_INCLUDED_ */
