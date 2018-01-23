
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CRYPT_H_INCLUDED_
#define _NGX_CRYPT_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

//使用 DES、Blowfish 或 MD5 加密的字符串
//key：要加密的明文  salt：密钥  encrypted：返回值
ngx_int_t ngx_crypt(ngx_pool_t *pool, u_char *key, u_char *salt,
    u_char **encrypted);


#endif /* _NGX_CRYPT_H_INCLUDED_ */
