/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_regex.h
* @date:      2018/1/22 下午6:18
* @desc:
 *
 * 参考 https://github.com/vislee/leevis.com/issues/65
 *
*/

//
// Created by daemon.xie on 2018/1/22.
//

#ifndef NGX_REGEX_NGX_REGEX_H
#define NGX_REGEX_NGX_REGEX_H

#include <ngx_config.h>
#include <ngx_core.h>

#include <pcre.h>

#define  NGX_REGEX_NO_MATCHED PCRE_ERROR_NOMATCH /* -1 */

#define NGX_REGEX_CASELESS PCRE_CASELESS

typedef struct {
    pcre *code;
    pcre_extra *extra;
} ngx_regex_t;

typedef struct {
    ngx_str_t pattern; /* 正则字符串 */
    ngx_pool_t *pool; /* 编译正则表达式从哪分配内存 */
    ngx_int_t options; /* pcre_compile 的options ngx目前仅用到PCRE_CASELESS，表示忽略大小写*/

    ngx_regex_t *regex; /* regex->code 编译后的结果，即pcre_compile返回 */
    int captures; /* pcre_fullinfo PCRE_INFO_CAPTURECOUNT 的值。捕获变量的个数 */
    int named_captures; /* 捕获变量设置了别名的个数 */
    int name_size; /* 捕获变量结构长度 */
    u_char *names; /* 捕获变量别名结构数组。别名下标占2个字节剩下的就是变量的名字。index=2*(x[0]<<8 + x[0])*/
    ngx_str_t err;
} ngx_regex_compile_t;

typedef struct {
    ngx_regex_t *regex;
    u_char *name;
} ngx_regex_elt_t;

void ngx_regex_init(void);
ngx_int_t ngx_regex_compile(ngx_regex_compile_t *rc);

#define ngx_regex_exec(re, s, captures, size)   \
    pcre_exec(re->code, re->extra, (const char *) (s)->data, (s)->len, 0, 0, captures, size)

#define ngx_regex_exec_n "pcre_exec()"

ngx_int_t ngx_regex_exec_array(ngx_array_t *a, ngx_str_t *s, ngx_log_t *log);


#endif //NGX_REGEX_NGX_REGEX_H
