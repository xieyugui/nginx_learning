/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_sha1.h
* @date:      2018/1/23 上午11:19
* @desc:
*/

//
// Created by daemon.xie on 2018/1/23.
//
#ifndef _NGX_SHA1_H_INCLUDED_
#define _NGX_SHA1_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


#if (NGX_HAVE_OPENSSL_SHA1_H)
#include <openssl/sha.h>
#else
#include <sha.h>
#endif


typedef SHA_CTX  ngx_sha1_t;

//SHA1_Init() 是一个初始化参数，它用来初始化一个 SHA_CTX 结构，该结构存放弄了生成 SHA1 散列值的一些参数，在应用中可以不用关系该结构的内容。
#define ngx_sha1_init    SHA1_Init
//SHA1_Update() 函数正是可以处理大文件的关键。它可以反复调用，比如说我们要计算一个 5G 文件的散列值，我们可以将该文件分割成多个小的数据块，
// 对每个数据块分别调用一次该函数，这样在最后就能够应用 SHA1_Final() 函数正确计算出这个大文件的 sha1 散列值。
#define ngx_sha1_update  SHA1_Update
#define ngx_sha1_final   SHA1_Final


#endif /* _NGX_SHA1_H_INCLUDED_ */


/**
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>

static const char hex_chars[] = "0123456789abcdef";

void convert_hex(unsigned char *md, unsigned char *mdstr)
{
    int i;
    int j = 0;
    unsigned int c;

    for (i = 0; i < 20; i++) {
        c = (md[i] >> 4) & 0x0f;
        mdstr[j++] = hex_chars[c];
        mdstr[j++] = hex_chars[md[i] & 0x0f];
    }
    mdstr[40] = '\0';
}

int main(int argc, char **argv)
{
    SHA_CTX shactx;
    char data[] = "hello groad.net";
    char md[SHA_DIGEST_LENGTH];
    char mdstr[40];

    SHA1_Init(&shactx);
    SHA1_Update(&shactx, data, 6);
    SHA1_Update(&shactx, data+6, 9);
    SHA1_Final(md, &shactx);
    convert_hex(md, mdstr);
    printf ("Result of SHA1 : %s\n", mdstr);
    return 0;
}
 */