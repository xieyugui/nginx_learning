/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_regex.c
* @date:      2018/1/22 下午6:19
* @desc:
*/

//
// Created by daemon.xie on 2018/1/22.
//

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_regex.h>

typedef struct {
    ngx_flag_t pcre_jit;
} ngx_regex_conf_t;

static void * ngx_libc_cdecl ngx_regex_malloc(size_t size);
static void ngx_libc_cdecl ngx_regex_free(void *p);
#if (NGX_HAVE_PCRE_JIT)
static void ngx_pcre_free_studies(void *data);
#endif

static ngx_int_t ngx_regex_module_init(ngx_cycle_t *cycle);

static void *ngx_regex_create_conf(ngx_cycle_t *cycle);
static char *ngx_regex_init_conf(ngx_cycle_t *cycle, void *conf);

static char *ngx_regex_pcre_jit(ngx_conf_t *cf, void *post, void *data);
static ngx_conf_post_t ngx_regex_pcre_jit_post = { ngx_regex_pcre_jit };

/**
 * 定义pcre_jit 模块的指令
 */
static ngx_command_t ngx_regex_commands[] = {
        { ngx_string("pcre_jit"), //名称
          NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,//作用域以及所带参数
          ngx_conf_set_flag_slot, //处理读取的数据  配置项的参数是on或者off，当为on时结构体中对应变量被设置为1，否则被设置为0
          0, //offset类型, 用来结构体的偏移的
          offsetof(ngx_regex_conf_t, pcre_jit),  //参数就是具体offset的值
          //配置项读取后的处理方法，必须是ngx_conf_post_t 结构的指针
          &ngx_regex_pcre_jit_post },

        ngx_null_command
};

//模块上下文结构体
static ngx_core_module_t ngx_regex_module_ctx = {
        ngx_string("regex"), //模块名，即ngx_core_module_ctx结构体对象的
        ngx_regex_create_conf, //解析配置项，nginx框架会调用create_conf方法
        ngx_regex_init_conf //解析配置项完成后，nginx框架会调用init_conf方法
};

//该结构体描述了整个模块的所有信息，为核心模块进行初始化和调用提供了接口
ngx_module_t  ngx_regex_module = {
        NGX_MODULE_V1,
        &ngx_regex_module_ctx,                 /* module context 上下文*/
        ngx_regex_commands,                    /* module directives 指令*/
        NGX_CORE_MODULE,                       /* module type 模块类型*/
        NULL,                                  /* init master */
        ngx_regex_module_init,                 /* init module */
        NULL,                                  /* init process */
        NULL,                                  /* init thread */
        NULL,                                  /* exit thread */
        NULL,                                  /* exit process */
        NULL,                                  /* exit master */
        NGX_MODULE_V1_PADDING
};

static ngx_pool_t *ngx_pcre_pool; //pcre 全局内存管理池
static ngx_list_t *ngx_pcre_studies; //对编译好的模式进行学习，提取可以加速匹配的信息

void
ngx_regex_init(void)
{
    // 替换回调函数，ngx_regex_malloc会在全局的ngx_pcre_pool上进行内存分配
    pcre_malloc = ngx_regex_malloc;
    //内存释放
    pcre_free = ngx_regex_free;
}

//malloc 初始化，简单的赋值内存池
static ngx_inline void
ngx_regex_malloc_init(ngx_pool_t *pool)
{
    ngx_pcre_pool = pool;
}

static ngx_inline void
ngx_regex_malloc_done(void)
{
    ngx_pcre_pool = NULL;
}

//将一个正则表达式编译为一个内部结构，匹配多个字符串时可以加快匹配速度
ngx_int_t
ngx_regex_compile(ngx_regex_compile_t *rc)
{
    int n, erroff;//正则表达式错误偏移
    char *p;
    pcre *re; //编译好的模式
    const char *errstr;//返回的错误信息
    ngx_regex_elt_t *elt;

    //ngx_regex_malloc_init会把rc->pool复制给全局ngx_pcre_pool,供ngx_pcre_malloc使用
    ngx_regex_malloc_init(rc->pool);

    /**
     * 将一个正则表达式编译为一个内部结构，匹配多个字符串时可以加快匹配速度。
        参数：
        pattern: 包含正则表达式的c字符串
        options: 0或者其他参数选项
        errptr: 返回的错误信息
        erroffset: 正则表达式错误偏移
        tableptr: 字符数组或空
     */
    re = pcre_compile((const char *) rc->pattern.data, (int) rc->options,
                    &errstr, &erroff, NULL);

    //释放ngx_pcre_pool
    ngx_regex_malloc_done();

    //报错处理
    if (re == NULL) {
        if ((size_t) erroff == rc->pattern.len) {
            rc->err.len = ngx_snprintf(rc->err.data, rc->err.len,
                                       "pcre_compile() failed: %s in \"%V\"",
                                       errstr, &rc->pattern)
                          - rc->err.data;

        } else {
            rc->err.len = ngx_snprintf(rc->err.data, rc->err.len,
                                       "pcre_compile() failed: %s in \"%V\" at \"%s\"",
                                       errstr, &rc->pattern, rc->pattern.data + erroff)
                          - rc->err.data;
        }

        return NGX_ERROR;
    }

    //申请regex空间
    rc->regex = ngx_pcalloc(rc->pool, sizeof(ngx_regex_t));
    if (rc->regex == NULL) {
        goto nomem;
    }

    rc->regex->code = re;

    /* do not study at runtime */

    if (ngx_pcre_studies != NULL) {
        elt = ngx_list_push(ngx_pcre_studies);
        if (elt == NULL) {
            goto nomem;
        }

        elt->regex = rc->regex;
        elt->name = rc->pattern.data;
    }

    //PCRE_INFO_CAPTURECOUNT: 得到的是所有子模式的个数,包含命名子模式和非命名子模式
    /**
     * 返回编译好的模式信息。
        参数：
        code: 编译好的模式，pcre_compile的返回值。
        extra: pcre_study()的返回值，或NULL
        what: 要返回什么信息
        where: 返回的结果
     */
    // 需要捕获结果的个数
    n = pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &rc->captures);
    if (n < 0) {
        p = "pcre_fullinfo(\"%V\", PCRE_INFO_CAPTURECOUNT) failed: %d";
        goto failed;
    }
    //下面查找各项，将错误信息输出
    if (rc->captures == 0) {
        return NGX_OK;
    }

    /* See if there are any named substrings, and if so, show them by name. First
    we have to extract the count of named parentheses from the pattern. */
    // 捕获结果设置别名的个数
    n = pcre_fullinfo(re, NULL, PCRE_INFO_NAMECOUNT, &rc->named_captures);
    if (n < 0) {
        p = "pcre_fullinfo(\"%V\", PCRE_INFO_NAMECOUNT) failed: %d";
        goto failed;
    }

    if (rc->named_captures == 0) {
        return NGX_OK;
    }

    /* Before we can access the substrings, we must extract the table for
    translating names to numbers, and the size of each entry in the table. */
    //  捕获数组每个元素的大小
    n = pcre_fullinfo(re, NULL, PCRE_INFO_NAMEENTRYSIZE, &rc->name_size);   /* where to put the answer */

    if (n < 0) {
        p = "pcre_fullinfo(\"%V\", PCRE_INFO_NAMEENTRYSIZE) failed: %d";
        goto failed;
    }

    //  指向捕获数组首地址
    n = pcre_fullinfo(re, NULL, PCRE_INFO_NAMETABLE, &rc->names);  /* where to put the answer */

    if (n < 0) {
        p = "pcre_fullinfo(\"%V\", PCRE_INFO_NAMETABLE) failed: %d";
        goto failed;
    }

    return NGX_OK;

failed:

    rc->err.len = ngx_snprintf(rc->err.data, rc->err.len, p, &rc->pattern, n)
                  - rc->err.data;
    return NGX_ERROR;

nomem:

    rc->err.len = ngx_snprintf(rc->err.data, rc->err.len,
                               "regex \"%V\" compilation failed: no memory",
                               &rc->pattern)
                  - rc->err.data;
    return NGX_ERROR;
}


ngx_int_t
ngx_regex_exec_array(ngx_array_t *a, ngx_str_t *s, ngx_log_t *log)
{
    ngx_int_t         n;
    ngx_uint_t        i;
    ngx_regex_elt_t  *re;

    re = a->elts;

    for (i = 0; i < a->nelts; i++) {

        //使用编译好的模式进行匹配，采用与Perl相似的算法，返回匹配串的偏移位置
        /**
         * regex
         * s 需要匹配的字符串
         * captures 指向一个结果的整型数组
         * size 数组大小
         */
        n = ngx_regex_exec(re[i].regex, s, NULL, 0);

        if (n == NGX_REGEX_NO_MATCHED) {
            continue;
        }

        if (n < 0) {
            ngx_log_error(NGX_LOG_ALERT, log, 0,
                          ngx_regex_exec_n " failed: %i on \"%V\" using \"%s\"",
                          n, s, re[i].name);
            return NGX_ERROR;
        }

        /* match */

        return NGX_OK;
    }

    return NGX_DECLINED;
}

//在全局的ngx_pcre_pool上进行内存分配 size
static void * ngx_libc_cdecl
ngx_regex_malloc(size_t size)
{
    ngx_pool_t      *pool;
    pool = ngx_pcre_pool;

    if (pool) {
        return ngx_palloc(pool, size);
    }

    return NULL;
}

//内存释放，nothing
static void ngx_libc_cdecl
ngx_regex_free(void *p)
{
    return;
}

//什么pcre jit  JIT的不是PCRE自身的C代码，而是PCRE里面那个正则表达式虚拟机的中间代码
#if (NGX_HAVE_PCRE_JIT)

//This function is used to free the memory used for the data generated by a call to pcre[16|32]_study()
//study的内存释放
static void
ngx_pcre_free_studies(void *data)
{
    ngx_list_t *studies = data;

    ngx_uint_t        i;
    ngx_list_part_t  *part;
    ngx_regex_elt_t  *elts;

    part = &studies->part;
    elts = part->elts;

    for (i = 0 ; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            elts = part->elts;
            i = 0;
        }

        if (elts[i].regex->extra != NULL) {
            pcre_free_study(elts[i].regex->extra);
        }
    }
}

#endif

//nginx  初始化模块的时候调用
static ngx_int_t
ngx_regex_module_init(ngx_cycle_t *cycle)
{
    int               opt;
    const char       *errstr;
    ngx_uint_t        i;
    ngx_list_part_t  *part;
    ngx_regex_elt_t  *elts;

    opt = 0;

#if (NGX_HAVE_PCRE_JIT)
    {
    ngx_regex_conf_t    *rcf;
    ngx_pool_cleanup_t  *cln;

    rcf = (ngx_regex_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_regex_module);

    if (rcf->pcre_jit) {
        opt = PCRE_STUDY_JIT_COMPILE;

        /*
         * The PCRE JIT compiler uses mmap for its executable codes, so we
         * have to explicitly call the pcre_free_study() function to free
         * this memory.
         */

        cln = ngx_pool_cleanup_add(cycle->pool, 0);
        if (cln == NULL) {
            return NGX_ERROR;
        }

        cln->handler = ngx_pcre_free_studies;
        cln->data = ngx_pcre_studies;
    }
    }
#endif

    ngx_regex_malloc_init(cycle->pool);

    //ngx_pcre_studies 对编译好的模式进行学习，提取可以加速匹配的信息
    part = &ngx_pcre_studies->part;
    elts = part->elts;

    for (i = 0 ; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            elts = part->elts;
            i = 0;
        }

        /*
          对编译后的正则表达式结构(struct real_pcre)进行分析和学习,学习的结果是一个数据结构(struct pcre_extra),这个数据结构连同编译
          后的规则(struct real_pcre)可以一起送给pcre_exec单元进行匹配.
          pcre_study（）的引入主要是为了加速正则表达式匹配的速度.(为什么学习后就能加速呢?)这个还是比较有用的,可以将正则表达式编译,
          学习后保存到一个文件或内存中,这样进行匹配的时候效率比较搞.snort中就是这样做的.
          */
        elts[i].regex->extra = pcre_study(elts[i].regex->code, opt, &errstr);

        if (errstr != NULL) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                          "pcre_study() failed: %s in \"%s\"",
                          errstr, elts[i].name);
        }

#if (NGX_HAVE_PCRE_JIT)
        if (opt & PCRE_STUDY_JIT_COMPILE) {
            int jit, n;

            jit = 0;
            n = pcre_fullinfo(elts[i].regex->code, elts[i].regex->extra,
                              PCRE_INFO_JIT, &jit);

            if (n != 0 || jit != 1) {
                ngx_log_error(NGX_LOG_INFO, cycle->log, 0,
                              "JIT compiler does not support pattern: \"%s\"",
                              elts[i].name);
            }
        }
#endif
    }

    ngx_regex_malloc_done();

    ngx_pcre_studies = NULL;

    return NGX_OK;
}

//解析配置项，nginx框架会调用create_conf方法
static void *
ngx_regex_create_conf(ngx_cycle_t *cycle)
{
    ngx_regex_conf_t  *rcf;

    //是否参数配置了启用 pcre jit
    rcf = ngx_pcalloc(cycle->pool, sizeof(ngx_regex_conf_t));
    if (rcf == NULL) {
        return NULL;
    }

    rcf->pcre_jit = NGX_CONF_UNSET;

    //如果启用了，就开启study学习模式
    ngx_pcre_studies = ngx_list_create(cycle->pool, 8, sizeof(ngx_regex_elt_t));
    if (ngx_pcre_studies == NULL) {
        return NULL;
    }

    return rcf;
}

//解析配置项完成后，nginx框架会调用init_conf方法
static char *
ngx_regex_init_conf(ngx_cycle_t *cycle, void *conf)
{
    ngx_regex_conf_t *rcf = conf;

    ngx_conf_init_value(rcf->pcre_jit, 0);

    return NGX_CONF_OK;
}


//配置项读取后的处理方法，必须是ngx_conf_post_t 结构的指针
static char *
ngx_regex_pcre_jit(ngx_conf_t *cf, void *post, void *data)
{
    ngx_flag_t  *fp = data;

    if (*fp == 0) {
        return NGX_CONF_OK;
    }

#if (NGX_HAVE_PCRE_JIT)
    {
    int  jit, r;

    jit = 0;
    //  Availability of just-in-time compiler support (1=yes 0=no) 检查是否可用
    r = pcre_config(PCRE_CONFIG_JIT, &jit);

    if (r != 0 || jit != 1) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                           "PCRE library does not support JIT");
        *fp = 0;
    }
    }
#else
    ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                       "nginx was built without PCRE JIT support");
    *fp = 0;
#endif

    return NGX_CONF_OK;
}
