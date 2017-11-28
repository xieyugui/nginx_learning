#include <stdio.h>
#include <ngx_config.h>
#include <ngx_core.h>

volatile ngx_cycle_t  *ngx_cycle;
void ngx_log_error_core(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
                        const char *fmt, ...)
{
}

int main() {
    //基础测试
    ngx_str_t str = ngx_string("xie");
    printf("string=%s,len=%lu\n",str.data, str.len);
    ngx_str_t str1 = ngx_null_string;
    printf("string1=%s,len=%lu\n",str1.data, str1.len);
    ngx_str_t str2, str3;
    ngx_str_set(&str2, "hello world");
    printf("string2=%s,len=%lu\n",str2.data, str2.len);
    ngx_str_null(&str3);
    printf("string3=%s,len=%lu\n",str3.data, str3.len);

    //字符大小写转换
    u_char low_c = 'A';
    printf("low_char=%c\n", ngx_tolower(low_c));
    printf("upp_char=%c\n", ngx_toupper(low_c));
    //字符串转大小写
    ngx_str_t low_str = ngx_string("xIE");
    ngx_str_t l_str = ngx_string("I");
    //不能是常量
    u_char *xie;
    xie = (u_char *)malloc(sizeof(u_char)* 10);
    strncpy(xie, low_str.data, low_str.len);
    xie[low_str.len] = '\0';
    printf("xie=%s\n", xie);
    xie[0] = 'A';
    printf("xie=%s\n", xie);
    ngx_strlow(xie, xie, low_str.len);

    printf("low_str=%.*s\n", low_str.len, low_str.data);
    printf("ngx_strncmp=%lu\n", ngx_strncmp(low_str.data, low_str.data,low_str.len));
    printf("ngx_strcmp=%lu\n", ngx_strcmp(low_str.data, low_str.data));
    printf("ngx_strstr=%s\n",ngx_strstr(low_str.data, l_str.data));
    printf("ngx_strlen=%lu\n", ngx_strlen(low_str.data));
    //不能是常量
    printf("ngx_strnlen=%d\n", ngx_strnlen(xie, 4));
    printf("ngx_strchr=%s\n", ngx_strchr(low_str.data, 'I'));

    ngx_memzero(xie,3);
    printf("ngx_memzero=%s\n", xie);
    ngx_memset(xie, 'x', 4);
    printf("ngx_memzero=%s\n", xie);

    //ngx_pstrdup
    ngx_pool_t *pool = ngx_create_pool(1024, NULL);
    if (pool == NULL)
    {
        printf("create pool failed!\n");
    }
    u_char *str_dup = ngx_pstrdup(pool, &low_str);
    printf("ngx_pstrdup=%s\n",str_dup);
    ngx_destroy_pool(pool);

    xie = ngx_sprintf(xie, "xieyugui=%lu",low_str.len);
    printf("ngx_sprintf=%s\n", xie);

    printf("ngx_strcasecmp=%d\n", ngx_strcasecmp("x","X"));


    free(xie);
    return 0;
}