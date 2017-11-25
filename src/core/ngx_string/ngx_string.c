/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_string.c
* @date:      2017/11/24 下午3:36
* @desc:
*/

//
// Created by daemon.xie on 2017/11/24.
//

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_string.h>

static u_char *ngx_sprintf_num(u_char *buf, u_char *last, uint64_t ui64,
                               u_char zero, ngx_uint_t hexadecimal, ngx_uint_t width);
static void ngx_encode_base64_internal(ngx_str_t *dst, ngx_str_t *src,
                                       const u_char *basis, ngx_uint_t padding);
static ngx_int_t ngx_decode_base64_internal(ngx_str_t *dst, ngx_str_t *src,
                                            const u_char *basis);

void
ngx_strlow(u_char *dst, u_char *src, size_t n)
{
    while (n) {
        *dst = ngx_tolower(*src);
        dst++;
        src++;
        n--;
    }
}

size_t
ngx_strnlen(u_char *p, size_t n)
{
    size_t i;

    for(i = 0; i < n; i++) {
        if (p[i] == '\0') {
            return i;
        }
    }

    return n;
}

u_char *
ngx_cpystrn(u_char *dst, u_char *src, size_t n)
{
    if (n == 0) {
        return dst;
    }

    while (--n) {
        *dst = *src;

        if(*dst == '\0') {
            return dst;
        }

        dst++;
        src++;
    }

    *dst = '\0';
    return dst;
}

u_char *
ngx_pstrdup(ngx_pool_t *pool, ngx_str_t *src)
{
    u_char *dst;

    //内存申请（不对齐）
    dst = ngx_pnalloc(pool, src->len);
    if(dst == NULL){
        return NULL;
    }

    ngx_memcpy(dst, src->data, src->len);
    return dst;
}

u_char * ngx_cdecl
ngx_sprintf(u_char *buf, const char *fmt,...)
{
    u_char *p;
    //用于获取不确定个数的参数
    va_list args;

    //args 初始化
    va_start(args, fmt);
    //(void *) -1 空指针地址-1,意思是最大？
    p = ngx_vslprintf(buf, (void *) -1, fmt, args);
    va_end(args);

    return p;

}

u_char * ngx_cdecl
ngx_snprintf(u_char *buf, size_t max, const char *fmt, ...)
{
    u_char *p;
    va_list args;

    va_start(args, fmt);
    p = ngx_vslprintf(buf, buf+max, fmt, args);
    va_end(args);

    return p;
}

u_char * ngx_cdecl
ngx_slprintf(u_char *buf, u_char *last, const char *fmt, ...)
{
    u_char *p;
    va_list args;

    va_start(args, fmt);
    p = ngx_vslprintf(buf, last, fmt , args);
    va_end(args);

    return p;
}

ngx_int_t
ngx_strcasecmp(u_char *s1, u_char *s2)
{
    ngx_uint_t c1,c2;

    for ( ;; ) {
        c1 = (ngx_uint_t) *s1++;
        c2 = (ngx_uint_t) *s2++;

        //转为大写
        c1 = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;
        c2 = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;

        if (c1 == c2) {
            if(c1) {
                continue;
            }

            return 0;
        }

        return c1 - c2;
    }
}

ngx_int_t
ngx_strncasecmp(u_char *s1, u_char *s2, size_t n)
{
    ngx_uint_t c1,c2;

    while(n) {
        c1 = (ngx_uint_t) *s1++;
        c2 = (ngx_uint_t) *s2++;

        c1 = (c1 >= 'A' && c1 <= 'Z') ? ( c1| 0x20) : c1;
        c2 = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;

        if(c1 == c2) {
            if(c1) {
                n--;
                continue;
            }
        }

        return c1 - c2;
    }

    return 0;
}

u_char *
ngx_strnstr(u_char *s1, char *s2, size_t len)
{
    u_char c1,c2;
    size_t n;

    c2 = *(u_char *) s2++;

    //s2 字符串长度
    n = ngx_strlen(s2);

    do {
        //先从s1 找到首个与s2首个字符相同的字符
        do {
            if(len-- == 0) {
                return NULL;
            }

            c1 = *s1++;
            //当s1 为空的时候，表示没有满足的
            if (c1 == 0) {
                return NULL;
            }

        } while(c1 != c2);
        //当s1的长度小于s2的长度的时候，就表示没有满足的
        if(n > len) {
            return NULL;
        }
    } while(ngx_strncmp(s1, (u_char *) s2, n) != 0);

    return --s1;
}

u_char *
ngx_strstrn(u_char *s1, char *s2, size_t n)
{
    u_char c1, c2;

    c2 = *(char *)s2++;

    do {

        do{
            c1 = *s1++;

            if (c1 == 0) {
                return NULL;
            }

        } while(c1 != c2);


    } while(ngx_strncmp(s1, (u_char *) s2, n) != 0);


    return --s1;
}

u_char *
ngx_strcasestrn(u_char *s1, char *s2, size_t n)
{
    ngx_uint_t  c1, c2;

    c2 = (ngx_uint_t) *s2++;
    c2 = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;

    do {
        do {
            c1 = (ngx_uint_t) *s1++;

            if (c1 == 0) {
                return NULL;
            }

            c1 = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;

        } while (c1 != c2);

    } while (ngx_strncasecmp(s1, (u_char *) s2, n) != 0);

    return --s1;
}

u_char *
ngx_strlcasestrn(u_char *s1, u_char *last, u_char *s2, size_t n)
{
    ngx_uint_t c1,c2;

    c2 = (ngx_uint_t) *s2++;
    c2 = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;
    //s1 开始位置
    last -= n;

    do{
        do {
            if (s1 >= last) {
                return NULL;
            }

            c1 = (ngx_uint_t) *s1++;

            c1 = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;

        } while(c1 != c2);
    } while(ngx_strncmp(s1,s2,n) != 0);

    return --s1;
}

ngx_int_t
ngx_rstrncmp(u_char *s1, u_char *s2, size_t n)
{
    if (n == 0) {
        return 0;
    }

    n--;

    for ( ;; ) {
        if (s1[n] != s2[n]) {
            return s1[n] - s2[n];
        }

        if (n == 0) {
            return 0;
        }

        n--;
    }
}

ngx_int_t
ngx_rstrncasecmp(u_char *s1, u_char *s2, size_t n)
{
    u_char  c1, c2;

    if (n == 0) {
        return 0;
    }

    n--;

    for ( ;; ) {
        c1 = s1[n];
        if (c1 >= 'a' && c1 <= 'z') {
            c1 -= 'a' - 'A';
        }

        c2 = s2[n];
        if (c2 >= 'a' && c2 <= 'z') {
            c2 -= 'a' - 'A';
        }

        if (c1 != c2) {
            return c1 - c2;
        }

        if (n == 0) {
            return 0;
        }

        n--;
    }
}

ngx_int_t
ngx_memn2cmp(u_char *s1, u_char *s2, size_t n1, size_t n2)
{
    size_t     n;
    ngx_int_t  m, z;

    if (n1 <= n2) {
        n = n1;
        z = -1;

    } else {
        n = n2;
        z = 1;
    }

    m = ngx_memcmp(s1, s2, n);

    if (m || n1 == n2) {
        return m;
    }

    return z;
}

ngx_int_t
ngx_dns_strcmp(u_char *s1, u_char *s2)
{
    ngx_uint_t  c1, c2;

    for ( ;; ) {
        c1 = (ngx_uint_t) *s1++;
        c2 = (ngx_uint_t) *s2++;

        c1 = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;
        c2 = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;

        if (c1 == c2) {

            if (c1) {
                continue;
            }

            return 0;
        }

        /* in ASCII '.' > '-', but we need '.' to be the lowest character */

        c1 = (c1 == '.') ? ' ' : c1;
        c2 = (c2 == '.') ? ' ' : c2;

        return c1 - c2;
    }
}


ngx_int_t
ngx_filename_cmp(u_char *s1, u_char *s2, size_t n)
{
    ngx_uint_t  c1, c2;

    while (n) {
        c1 = (ngx_uint_t) *s1++;
        c2 = (ngx_uint_t) *s2++;

#if (NGX_HAVE_CASELESS_FILESYSTEM)
        c1 = tolower(c1);
        c2 = tolower(c2);
#endif

        if (c1 == c2) {

            if (c1) {
                n--;
                continue;
            }

            return 0;
        }

        /* we need '/' to be the lowest character */

        if (c1 == 0 || c2 == 0) {
            return c1 - c2;
        }

        c1 = (c1 == '/') ? 0 : c1;
        c2 = (c2 == '/') ? 0 : c2;

        return c1 - c2;
    }

    return 0;
}

ngx_int_t
ngx_atoi(u_char *line, size_t n)
{
    ngx_int_t value, cutoff, cutlim;

    if(n == 0) {
        return NGX_ERROR;
    }

    cutoff = NGX_MAX_INT_T_VALUE / 10;
    cutlim = NGX_MAX_INT_T_VALUE % 10;

    for (value = 0; n--; line++){
        if (*line < '0' || *line > '9') {
            return NGX_ERROR;
        }
        if (value >= cutoff && (value > cutoff || *line - '0' > cutlim)) {
            return NGX_ERROR;
        }

        value = value * 10 + (*line - '0');
    }

    return value;
}

ngx_int_t
ngx_atofp(u_char *line, size_t n, size_t point)
{
    ngx_int_t value, cutoff, cutlim;
    ngx_uint_t dot;

    if (n == 0) {
        return NGX_ERROR;
    }

    cutoff = NGX_MAX_INT_T_VALUE / 10;
    cutlim = NGX_MAX_INT_T_VALUE % 10;

    //小数点标记，不能出现超过1
    dot = 0;

    for (value = 0; n--; line++) {

        if (point == 0) {
            return NGX_ERROR;
        }

        if (*line == '.') {
            if(dot) {
                return NGX_ERROR;
            }

            dot = 1;
            continue;
        }

        if (*line < '0' || *line > '9') {
            return NGX_ERROR;
        }

        if (value >= cutoff && (value > cutoff || *line - '0' > cutlim)) {
            return NGX_ERROR;
        }

        value = value * 10 + (*line - '0');
        point -= dot;
    }

    //剩余的直接补0
    while(point --) {
        if (value > cutoff) {
            return NGX_ERROR;
        }

        value = value * 10;
    }

    return value;

}

ssize_t
ngx_atosz(u_char *line, size_t n)
{
    ssize_t  value, cutoff, cutlim;

    if (n == 0) {
        return NGX_ERROR;
    }

    cutoff = NGX_MAX_SIZE_T_VALUE / 10;
    cutlim = NGX_MAX_SIZE_T_VALUE % 10;

    for (value = 0; n--; line++) {
        if (*line < '0' || *line > '9') {
            return NGX_ERROR;
        }

        if (value >= cutoff && (value > cutoff || *line - '0' > cutlim)) {
            return NGX_ERROR;
        }

        value = value * 10 + (*line - '0');
    }

    return value;
}

off_t
ngx_atoof(u_char *line, size_t n)
{
    off_t  value, cutoff, cutlim;

    if (n == 0) {
        return NGX_ERROR;
    }

    cutoff = NGX_MAX_OFF_T_VALUE / 10;
    cutlim = NGX_MAX_OFF_T_VALUE % 10;

    for (value = 0; n--; line++) {
        if (*line < '0' || *line > '9') {
            return NGX_ERROR;
        }

        if (value >= cutoff && (value > cutoff || *line - '0' > cutlim)) {
            return NGX_ERROR;
        }

        value = value * 10 + (*line - '0');
    }

    return value;
}


time_t
ngx_atotm(u_char *line, size_t n)
{
    time_t  value, cutoff, cutlim;

    if (n == 0) {
        return NGX_ERROR;
    }

    cutoff = NGX_MAX_TIME_T_VALUE / 10;
    cutlim = NGX_MAX_TIME_T_VALUE % 10;

    for (value = 0; n--; line++) {
        if (*line < '0' || *line > '9') {
            return NGX_ERROR;
        }

        if (value >= cutoff && (value > cutoff || *line - '0' > cutlim)) {
            return NGX_ERROR;
        }

        value = value * 10 + (*line - '0');
    }

    return value;
}

ngx_int_t
ngx_hextoi(u_char *line, size_t n)
{
    u_char     c, ch;
    ngx_int_t  value, cutoff;

    if (n == 0) {
        return NGX_ERROR;
    }

    cutoff = NGX_MAX_INT_T_VALUE / 16;

    for (value = 0; n--; line++) {
        if (value > cutoff) {
            return NGX_ERROR;
        }

        ch = *line;

        if (ch >= '0' && ch <= '9') {
            value = value * 16 + (ch - '0');
            continue;
        }

        c = (u_char) (ch | 0x20);

        if (c >= 'a' && c <= 'f') {
            value = value * 16 + (c - 'a' + 10);
            continue;
        }

        return NGX_ERROR;
    }

    return value;
}

u_char *
ngx_hex_dump(u_char *dst, u_char *src, size_t len)
{
    static u_char  hex[] = "0123456789abcdef";

    while (len--) {
        //有空研究一下-_-
        *dst++ = hex[*src >> 4];
        *dst++ = hex[*src++ & 0xf];
    }

    return dst;
}

void
ngx_encode_base64(ngx_str_t *dst, ngx_str_t *src)
{
    static u_char   basis64[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    ngx_encode_base64_internal(dst, src, basis64, 1);
}

void
ngx_encode_base64url(ngx_str_t *dst, ngx_str_t *src)
{
    static u_char   basis64[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    ngx_encode_base64_internal(dst, src, basis64, 0);
}

static void
ngx_encode_base64_internal(ngx_str_t *dst, ngx_str_t *src, const u_char *basis,
                           ngx_uint_t padding)
{

}


ngx_int_t
ngx_decode_base64(ngx_str_t *dst, ngx_str_t *src)
{
    return NGX_OK;
}


ngx_int_t
ngx_decode_base64url(ngx_str_t *dst, ngx_str_t *src)
{

    return NGX_OK;
}


static ngx_int_t
ngx_decode_base64_internal(ngx_str_t *dst, ngx_str_t *src, const u_char *basis)
{

    return NGX_OK;
}

u_char *
ngx_vslprintf(u_char *buf, u_char *last, const char *fmt, va_list args)
{
    u_char                *p, zero;
    int                    d;
    double                 f;
    size_t                 len, slen;
    int64_t                i64;
    uint64_t               ui64, frac;
    ngx_msec_t             ms;
    ngx_uint_t             width, sign, hex, max_width, frac_width, scale, n;
    ngx_str_t             *v;
    ngx_variable_value_t  *vv;

    while (*fmt && buf < last) {

        /*
         * "buf < last" means that we could copy at least one character:
         * the plain character, "%%", "%c", and minus without the checking
         */

        if (*fmt == '%') {

            i64 = 0;
            ui64 = 0;

            zero = (u_char) ((*++fmt == '0') ? '0' : ' ');
            width = 0;
            sign = 1;
            hex = 0;
            max_width = 0;
            frac_width = 0;
            slen = (size_t) -1;

            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + *fmt++ - '0';
            }


            for ( ;; ) {
                switch (*fmt) {

                    case 'u':
                        sign = 0;
                        fmt++;
                        continue;

                    case 'm':
                        max_width = 1;
                        fmt++;
                        continue;

                    case 'X':
                        hex = 2;
                        sign = 0;
                        fmt++;
                        continue;

                    case 'x':
                        hex = 1;
                        sign = 0;
                        fmt++;
                        continue;

                    case '.':
                        fmt++;

                        while (*fmt >= '0' && *fmt <= '9') {
                            frac_width = frac_width * 10 + *fmt++ - '0';
                        }

                        break;

                    case '*':
                        //返回参数值
                        slen = va_arg(args, size_t);
                        fmt++;
                        continue;

                    default:
                        break;
                }

                break;
            }


            switch (*fmt) {

                case 'V':
                    v = va_arg(args, ngx_str_t *);

                    len = ngx_min(((size_t) (last - buf)), v->len);
                    buf = ngx_cpymem(buf, v->data, len);
                    fmt++;

                    continue;

                case 'v':
                    vv = va_arg(args, ngx_variable_value_t *);

                    len = ngx_min(((size_t) (last - buf)), vv->len);
                    buf = ngx_cpymem(buf, vv->data, len);
                    fmt++;

                    continue;

                case 's':
                    p = va_arg(args, u_char *);

                    if (slen == (size_t) -1) {
                        while (*p && buf < last) {
                            *buf++ = *p++;
                        }

                    } else {
                        len = ngx_min(((size_t) (last - buf)), slen);
                        buf = ngx_cpymem(buf, p, len);
                    }

                    fmt++;

                    continue;

                case 'O':
                    i64 = (int64_t) va_arg(args, off_t);
                    sign = 1;
                    break;

                case 'P':
                    i64 = (int64_t) va_arg(args, ngx_pid_t);
                    sign = 1;
                    break;

                case 'T':
                    i64 = (int64_t) va_arg(args, time_t);
                    sign = 1;
                    break;

                case 'M':
                    ms = (ngx_msec_t) va_arg(args, ngx_msec_t);
                    if ((ngx_msec_int_t) ms == -1) {
                        sign = 1;
                        i64 = -1;
                    } else {
                        sign = 0;
                        ui64 = (uint64_t) ms;
                    }
                    break;

                case 'z':
                    if (sign) {
                        i64 = (int64_t) va_arg(args, ssize_t);
                    } else {
                        ui64 = (uint64_t) va_arg(args, size_t);
                    }
                    break;

                case 'i':
                    if (sign) {
                        i64 = (int64_t) va_arg(args, ngx_int_t);
                    } else {
                        ui64 = (uint64_t) va_arg(args, ngx_uint_t);
                    }

                    if (max_width) {
                        width = NGX_INT_T_LEN;
                    }

                    break;

                case 'd':
                    if (sign) {
                        i64 = (int64_t) va_arg(args, int);
                    } else {
                        ui64 = (uint64_t) va_arg(args, u_int);
                    }
                    break;

                case 'l':
                    if (sign) {
                        i64 = (int64_t) va_arg(args, long);
                    } else {
                        ui64 = (uint64_t) va_arg(args, u_long);
                    }
                    break;

                case 'D':
                    if (sign) {
                        i64 = (int64_t) va_arg(args, int32_t);
                    } else {
                        ui64 = (uint64_t) va_arg(args, uint32_t);
                    }
                    break;

                case 'L':
                    if (sign) {
                        i64 = va_arg(args, int64_t);
                    } else {
                        ui64 = va_arg(args, uint64_t);
                    }
                    break;

                case 'A':
                    if (sign) {
                        i64 = (int64_t) va_arg(args, ngx_atomic_int_t);
                    } else {
                        ui64 = (uint64_t) va_arg(args, ngx_atomic_uint_t);
                    }

                    if (max_width) {
                        width = NGX_ATOMIC_T_LEN;
                    }

                    break;

                case 'f':
                    f = va_arg(args, double);

                    if (f < 0) {
                        *buf++ = '-';
                        f = -f;
                    }

                    ui64 = (int64_t) f;
                    frac = 0;

                    if (frac_width) {

                        scale = 1;
                        for (n = frac_width; n; n--) {
                            scale *= 10;
                        }

                        frac = (uint64_t) ((f - (double) ui64) * scale + 0.5);

                        if (frac == scale) {
                            ui64++;
                            frac = 0;
                        }
                    }

                    buf = ngx_sprintf_num(buf, last, ui64, zero, 0, width);

                    if (frac_width) {
                        if (buf < last) {
                            *buf++ = '.';
                        }

                        buf = ngx_sprintf_num(buf, last, frac, '0', 0, frac_width);
                    }

                    fmt++;

                    continue;

#if !(NGX_WIN32)
                case 'r':
                    i64 = (int64_t) va_arg(args, rlim_t);
                    sign = 1;
                    break;
#endif

                case 'p':
                    ui64 = (uintptr_t) va_arg(args, void *);
                    hex = 2;
                    sign = 0;
                    zero = '0';
                    width = 2 * sizeof(void *);
                    break;

                case 'c':
                    d = va_arg(args, int);
                    *buf++ = (u_char) (d & 0xff);
                    fmt++;

                    continue;

                case 'Z':
                    *buf++ = '\0';
                    fmt++;

                    continue;

                case 'N':
#if (NGX_WIN32)
                    *buf++ = CR;
                if (buf < last) {
                    *buf++ = LF;
                }
#else
                    *buf++ = LF;
#endif
                    fmt++;

                    continue;

                case '%':
                    *buf++ = '%';
                    fmt++;

                    continue;

                default:
                    *buf++ = *fmt++;

                    continue;
            }

            if (sign) {
                if (i64 < 0) {
                    *buf++ = '-';
                    ui64 = (uint64_t) -i64;

                } else {
                    ui64 = (uint64_t) i64;
                }
            }

            buf = ngx_sprintf_num(buf, last, ui64, zero, hex, width);

            fmt++;

        } else {
            *buf++ = *fmt++;
        }
    }

    return buf;
}

static u_char *
ngx_sprintf_num(u_char *buf, u_char *last, uint64_t ui64, u_char zero,
                ngx_uint_t hexadecimal, ngx_uint_t width)
{
    u_char         *p, temp[NGX_INT64_LEN + 1];
    /*
     * we need temp[NGX_INT64_LEN] only,
     * but icc issues the warning
     */
    size_t          len;
    uint32_t        ui32;
    static u_char   hex[] = "0123456789abcdef";
    static u_char   HEX[] = "0123456789ABCDEF";

    p = temp + NGX_INT64_LEN;

    if (hexadecimal == 0) {

        if (ui64 <= (uint64_t) NGX_MAX_UINT32_VALUE) {

            /*
             * To divide 64-bit numbers and to find remainders
             * on the x86 platform gcc and icc call the libc functions
             * [u]divdi3() and [u]moddi3(), they call another function
             * in its turn.  On FreeBSD it is the qdivrem() function,
             * its source code is about 170 lines of the code.
             * The glibc counterpart is about 150 lines of the code.
             *
             * For 32-bit numbers and some divisors gcc and icc use
             * a inlined multiplication and shifts.  For example,
             * unsigned "i32 / 10" is compiled to
             *
             *     (i32 * 0xCCCCCCCD) >> 35
             */

            ui32 = (uint32_t) ui64;

            do {
                *--p = (u_char) (ui32 % 10 + '0');
            } while (ui32 /= 10);

        } else {
            do {
                *--p = (u_char) (ui64 % 10 + '0');
            } while (ui64 /= 10);
        }

    } else if (hexadecimal == 1) {

        do {

            /* the "(uint32_t)" cast disables the BCC's warning */
            *--p = hex[(uint32_t) (ui64 & 0xf)];

        } while (ui64 >>= 4);

    } else { /* hexadecimal == 2 */

        do {

            /* the "(uint32_t)" cast disables the BCC's warning */
            *--p = HEX[(uint32_t) (ui64 & 0xf)];

        } while (ui64 >>= 4);
    }

    /* zero or space padding */

    len = (temp + NGX_INT64_LEN) - p;

    while (len++ < width && buf < last) {
        *buf++ = zero;
    }

    /* number safe copy */

    len = (temp + NGX_INT64_LEN) - p;

    if (buf + len > last) {
        len = last - buf;
    }

    return ngx_cpymem(buf, p, len);
}

/*
 * ngx_utf8_decode() decodes two and more bytes UTF sequences only
 * the return values:
 *    0x80 - 0x10ffff         valid character
 *    0x110000 - 0xfffffffd   invalid sequence
 *    0xfffffffe              incomplete sequence
 *    0xffffffff              error
 */

uint32_t
ngx_utf8_decode(u_char **p, size_t n)
{

    return 0xffffffff;
}


size_t
ngx_utf8_length(u_char *p, size_t n)
{

    return 0;
}


u_char *
ngx_utf8_cpystrn(u_char *dst, u_char *src, size_t n, size_t len)
{

    return dst;
}

uintptr_t
ngx_escape_uri(u_char *dst, u_char *src, size_t size, ngx_uint_t type)
{

    return (uintptr_t) dst;
}


void
ngx_unescape_uri(u_char **dst, u_char **src, size_t size, ngx_uint_t type)
{

}


uintptr_t
ngx_escape_html(u_char *dst, u_char *src, size_t size)
{

    return (uintptr_t) dst;
}


uintptr_t
ngx_escape_json(u_char *dst, u_char *src, size_t size)
{

    return (uintptr_t) dst;
}

void
ngx_str_rbtree_insert_value(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node,
ngx_rbtree_node_t *sentinel)
{

}

ngx_str_node_t *
ngx_str_rbtree_lookup(ngx_rbtree_t *rbtree, ngx_str_t *val, uint32_t hash)
{

    return NULL;
}

void
ngx_sort(void *base, size_t n, size_t size,
    ngx_int_t (*cmp)(const void *, const void *))
{
    u_char *p1, *p2, *p;

    p = ngx_alloc(size, ngx_cycle->log);
    if(p == NULL){
        return;
    }

    //冒泡排序
    //4,3,2,1
    for (p1 = (u_char *) base + size;
         p1 < (u_char *) base + n *size;
            p1 += size)
    {
        //将base+size 拷贝到p中保留
        //p1 = p = 3
        ngx_memcpy(p, p1, size);

        for (p2 = p1;
             p2 > (u_char *) base && cmp(p2 - size, p) > 0;
             p2 -= size)
        {
            //将4 赋值给3，覆盖3
            ngx_memcpy(p2, p2 - size, size);
        }
        //经过for 循环 p2 -=size
        //将保留在p 中的3 赋值给base 第一个位置，覆盖4
        ngx_memcpy(p2, p, size);
    }

    ngx_free(p);
}

#if (NGX_MEMCPY_LIMIT)

void *
ngx_memcpy(void *dst, const void *src, size_t n)
{
    return memcpy(dst, src, n);
}

#endif
