
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_MD5_H_INCLUDED_
#define _NGX_MD5_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct {
    uint64_t  bytes;
    uint32_t  a, b, c, d;
    u_char    buffer[64];
} ngx_md5_t;

// 初始化 MD5 Contex, 成功返回1,失败返回0
void ngx_md5_init(ngx_md5_t *ctx);
// 循环调用此函数,可以将不同的数据加在一起计算MD5,成功返回1,失败返回0
void ngx_md5_update(ngx_md5_t *ctx, const void *data, size_t size);
// 输出MD5结果数据,成功返回1,失败返回0
void ngx_md5_final(u_char result[16], ngx_md5_t *ctx);


#endif /* _NGX_MD5_H_INCLUDED_ */

/**
#include <openssl/md5.h>
#include <string.h>
#include <stdio.h>

int main()
{
    MD5_CTX ctx;
    unsigned char outmd[16];
    int i=0;

    memset(outmd,0,sizeof(outmd));
    MD5_Init(&ctx);
    MD5_Update(&ctx,"hel",3);
    MD5_Update(&ctx,"lo\n",3);
    MD5_Final(outmd,&ctx);
    for(i=0;i<16;i<i++)
    {
        printf("%02X",outmd[i]);
    }
    printf("\n");
    return 0;
}
 */