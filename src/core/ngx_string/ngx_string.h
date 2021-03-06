/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui
* @software:  CLion
* @file:      ngx_string.h
* @date:      2017/11/24 下午3:36
* @desc:
*/

//
// Created by daemon.xie on 2017/11/24.
//

#ifndef NGX_STRING_NGX_STRING_H
#define NGX_STRING_NGX_STRING_H

#include <ngx_config.h>
#include <ngx_core.h>

typedef struct {
    size_t len;
    u_char *data;
} ngx_str_t;

typedef struct {
    ngx_str_t key;
    ngx_str_t value;
} ngx_keyval_t;

typedef struct {
    unsigned len:28;
    unsigned  no_cacheable:1;
    unsigned  not_found:1;
    unsigned escape:1;

    u_char *data;
} ngx_variable_value_t;

#define ngx_string(str) { sizeof(str) - 1, (u_char *)str }
#define ngx_null_string {0, NULL}
#define ngx_str_set(str, text) (str)->len = sizeof(text) - 1; (str)->data = (u_char *) text
#define ngx_str_null(str) (str)->len = 0; (str)->data = NULL

//小写字母比大写字母大 0x20     c = C + 0x20
#define ngx_tolower(c) (u_char) ((c >= 'A' && c <= 'Z') ? (c | 0x20) : c)
#define ngx_toupper(c) (u_char) ((c >= 'a' && c <= 'z') ? (c & ~0x20) : c)

void ngx_strlow(u_char *dst, u_char *src, size_t n);

#define ngx_strncmp(s1, s2, n) strncmp((const char *) s1, (const char *) s2, n)

#define ngx_strcmp(s1, s2) strcmp((const char *) s1, (const char *) s2)

#define ngx_strstr(s1, s2) strstr((const char *) s1, (const char *) s2)

#define ngx_strlen(s) strlen((const char *) s)

size_t ngx_strnlen(u_char *p, size_t n);

#define ngx_strchr(s1, c) strchr((const char *) s1, (int) c)

static ngx_inline u_char *
ngx_strlchr(u_char *p, u_char *last, u_char c)
{
    while (p < last) {
        if (*p == c) {
            return p;
        }

        p++;
    }

    return NULL;
}

#define ngx_memzero(buf, n) (void) memset(buf, 0, n)
#define ngx_memset(buf, c, n) (void) memset(buf, c, n)

#if (NGX_MEMCPY_LIMIT)

void *ngx_memcpy(void *dst, const void *src, size_t n);
#define ngx_cpymem(dst, src, n) (((u_char *) ngx_memcpy(dst, src, n)) + (n))

#else

#define ngx_memcpy(dst, src, n)   (void) memcpy(dst, src, n)
//复制内存，返回复制完了dst的最后一个字符的下一个字符的指针
#define ngx_cpymem(dst, src, n)   (((u_char *) memcpy(dst, src, n)) + (n))

#endif

#define ngx_copy ngx_cpymem

//移动字符串，允许重复
#define  ngx_memmove(dst, src, n) (void) memmove(dst, src, n)
#define ngx_movemem(dst, src, n) (((u_char *) memmove(dst, src, n)) + (n))

//比较内存中的数据是否相同
#define ngx_memcmp(s1, s2, n) memcmp((const char *) s1, (const char *) s2, n)

//复制字符串，并且返回字符串的最后一个字符的下一个字符的指针
u_char *ngx_cpystr(u_char *dst, u_char *src, size_t n);
//复制字符串到pool，返回字符串的指针
u_char *ngx_pstrdup(ngx_pool_t *pool, ngx_str_t *src);

//ngx_cdecl 修饰可变参数   把各种类型的数据格式化输出到buf，最大的长度为65536
u_char * ngx_cdecl ngx_sprintf(u_char *buf, const char *fmt, ...);
//把各种类型的数据格式化输出到指定长度的buf
u_char * ngx_cdecl ngx_snprintf(u_char *buf, size_t max, const char *fmt, ...);

u_char * ngx_cdecl ngx_slprintf(u_char *buf, u_char *last, const char *fmt, ...);

u_char *ngx_vslprintf(u_char *buf, u_char *last, const char *fmt, va_list args);

#define ngx_vsnprintf(buf, max, fmt, args) ngx_vslprintf(buf, buf + (max), fmt, args)

//不分大小写比较两个字符串是否相同
ngx_int_t ngx_strcasecmp(u_char *s1, u_char *s2);

//指定长短不分大小写比较两个字符串是否相同
ngx_int_t ngx_strncasecmp(u_char *s1, u_char *s2, size_t n);

//在指定大小一个字符串中是否有子字符串
u_char *ngx_strnstr(u_char *s1, char *s2, size_t n);

//在一个字符串中是否有子指定大小的字符串
u_char *ngx_strstrn(u_char *s1, char *s2, size_t n);

// 在一个字符串中是否有子指定大小的字符串，不区分大小写
u_char *ngx_strcasestrn(u_char *s1, char *s2, size_t n);

//该函数是判断s2是否在以s1开始以last结束的字符串里面，不区分大小写，如果在，返回s2在以s1开始以last结束的字符串的位置
u_char *ngx_strlcasestrn(u_char *s1, u_char *last, u_char *s2, size_t n);

//比较s1和s2的前n个字符，区分大小写，相等返回0，如果n为0，也返回0
ngx_int_t ngx_rstrncmp(u_char *s1, u_char *s2, size_t n);

//比较s1和s2的前n个字符，不区分大小写，相等返回0，如果n为0，也返回0
ngx_int_t ngx_rstrncasecmp(u_char *s1, u_char *s2, size_t n);

//s1的前n1个和s2的前n2个字符比较
ngx_int_t ngx_memn2cmp(u_char *s1, u_char *s2, size_t n1, size_t n2);

//比较dns 可能带有特殊字符.
ngx_int_t ngx_dns_strcmp(u_char *s1, u_char *s2);

//比较文件名，可能带有特殊字符/
ngx_int_t ngx_filename_cmp(u_char *s1, u_char *s2, size_t n);

//指定长度的字符串转换成数字
ngx_int_t ngx_atoi(u_char * line, size_t n);

//将一个固定长度的定点小数字符串转换为ngx_int_t类型的正整数。转换结果会左移point指定的10进制位数
//ngx_atofp("10.5", 4, 2) 返回1050
ngx_int_t ngx_atofp(u_char *line, size_t n, size_t point);

//指定长度的字符串转换成ssize_t类型数字  ssize_t是signed size_t
ssize_t ngx_atosz(u_char *line, size_t n);

//指定长度的字符串转换成off_t类型数字  off_t = int74_t == long long int
off_t ngx_atoof(u_char *line, size_t n);

//指定长度的字符串转换成time_t类型数字  long int
time_t ngx_atotm(u_char *line, size_t n);

//指定长度的字符串转换成十六进制数字
ngx_int_t ngx_hextoi(u_char *line, size_t n);

//把数字转换成16进制的字符串
u_char *ngx_hex_dump(u_char *dst, u_char *src, size_t len);

//将len base64 encoded 之后的长度
#define ngx_base64_encoded_length(len) (((len + 2) / 3) * 4)
#define ngx_base64_decoded_length(len)  (((len + 3) / 4) * 3)

//base64编码
void ngx_encode_base64(ngx_str_t *dst, ngx_str_t *src);
//url 已经有一些特殊字符
void ngx_encode_base64url(ngx_str_t *dst, ngx_str_t *src);
ngx_int_t ngx_decode_base64(ngx_str_t *dst, ngx_str_t *src);
ngx_int_t ngx_decode_base64url(ngx_str_t *dst, ngx_str_t *src);

//把 utf8字符解码成双字节的 unicode或是单字节字符，但是该函数会移动*p的值
uint32_t ngx_utf8_decode(u_char **p, size_t n);
//得到utf8编码的字符占几个字节
size_t ngx_utf8_length(u_char *p, size_t n);
//赋值utf8字符串，保证完整的复制
u_char *ngx_utf8_cpystrn(u_char *dst, u_char *src, size_t n, size_t len);

#define NGX_ESCAPE_URI            0
#define NGX_ESCAPE_ARGS           1
#define NGX_ESCAPE_URI_COMPONENT  2
#define NGX_ESCAPE_HTML           3
#define NGX_ESCAPE_REFRESH        4
#define NGX_ESCAPE_MEMCACHED      5
#define NGX_ESCAPE_MAIL_AUTH      6

#define NGX_UNESCAPE_URI       1
#define NGX_UNESCAPE_REDIRECT  2

//对uri进行编码
uintptr_t ngx_escape_uri(u_char *dst, u_char *src, size_t size,
                         ngx_uint_t type);

//对uri的进行解码
void ngx_unescape_uri(u_char **dst, u_char **src, size_t size, ngx_uint_t type);
//对html进行编码
uintptr_t ngx_escape_html(u_char *dst, u_char *src, size_t size);
uintptr_t ngx_escape_json(u_char *dst, u_char *src, size_t size);

typedef struct {
    ngx_rbtree_node_t node;
    ngx_str_t   str;
} ngx_str_node_t;

//向红黑树中添加数据节点，每个数据节点的关键字可以不是唯一的，但它们是以字符串作为唯一标识，存放在ngx_str_node_t结构体的str成员中
void ngx_str_rbtree_insert_value(ngx_rbtree_node_t *temp,ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);
ngx_str_node_t *ngx_str_rbtree_lookup(ngx_rbtree_t *rbtree, ngx_str_t *name,
                                      uint32_t hash);

void ngx_sort(void *base, size_t n, size_t size,
    ngx_int_t(*cmp)(const void *, const void *));

#define ngx_qsort             qsort

#define ngx_value_helper(n)   #n
#define ngx_value(n)          ngx_value_helper(n)
#endif //NGX_STRING_NGX_STRING_H
