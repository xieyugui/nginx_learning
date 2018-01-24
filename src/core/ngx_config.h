
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CONFIG_H_INCLUDED_
#define _NGX_CONFIG_H_INCLUDED_


#include <ngx_auto_headers.h>


#if defined __DragonFly__ && !defined __FreeBSD__
#define __FreeBSD__        4
#define __FreeBSD_version  480101
#endif


#if (NGX_FREEBSD)
#include <ngx_freebsd_config.h>


#elif (NGX_LINUX)
#include <ngx_linux_config.h>


#elif (NGX_SOLARIS)
#include <ngx_solaris_config.h>


#elif (NGX_DARWIN)
#include <ngx_darwin_config.h>


#elif (NGX_WIN32)
#include <ngx_win32_config.h>


#else /* POSIX */
#include <ngx_posix_config.h>

#endif


#ifndef NGX_HAVE_SO_SNDLOWAT
#define NGX_HAVE_SO_SNDLOWAT     1
#endif


#if !(NGX_WIN32)

#define ngx_signal_helper(n)     SIG##n
#define ngx_signal_value(n)      ngx_signal_helper(n)

#define ngx_random               random

/**
Master进程能够接收并处理如下的信号：

ERM, INT（快速退出，当前的请求不执行完成就退出）
QUIT （优雅退出，执行完当前的请求后退出）
HUP （重新加载配置文件，用新的配置文件启动新worker进程，并优雅的关闭旧的worker进程）
USR1 （重新打开日志文件）
USR2 （平滑的升级nginx二进制文件）
WINCH （优雅的关闭worker进程）
Worker进程也可以接收并处理一些信号：

TERM, INT （快速退出）
QUIT （优雅退出）
USR1 （重新打开日志文件）
 */
/* TODO: #ifndef */
#define NGX_SHUTDOWN_SIGNAL      QUIT //优雅退出，执行完当前的请求后退出
#define NGX_TERMINATE_SIGNAL     TERM //快速退出
#define NGX_NOACCEPT_SIGNAL      WINCH //优雅的关闭worker进程
#define NGX_RECONFIGURE_SIGNAL   HUP //重新加载配置文件，用新的配置文件启动新worker进程，并优雅的关闭旧的worker进程

#if (NGX_LINUXTHREADS)
#define NGX_REOPEN_SIGNAL        INFO
#define NGX_CHANGEBIN_SIGNAL     XCPU
#else
#define NGX_REOPEN_SIGNAL        USR1 //重新打开日志文件
#define NGX_CHANGEBIN_SIGNAL     USR2 //平滑的升级nginx二进制文件
#endif

#define ngx_cdecl
#define ngx_libc_cdecl

#endif
//intptr_t 和uintptr_t 类型用来存放指针地址。它们提供了一种可移植且安全的方法声明指针，而且和系统中使用的指针长度相同，对于把指针转化成整数形式来说很有用
typedef intptr_t        ngx_int_t;
typedef uintptr_t       ngx_uint_t;
//一般用用配置项中的 ON | OFF选项标记  1代表ON 0代表OFF  初始值一定要设置为NGX_CONF_UNSET，否则报错，见ngx_conf_set_flag_slot
typedef intptr_t        ngx_flag_t;


#define NGX_INT32_LEN   (sizeof("-2147483648") - 1)
#define NGX_INT64_LEN   (sizeof("-9223372036854775808") - 1)

#if (NGX_PTR_SIZE == 4)
#define NGX_INT_T_LEN   NGX_INT32_LEN
#define NGX_MAX_INT_T_VALUE  2147483647

#else
#define NGX_INT_T_LEN   NGX_INT64_LEN
#define NGX_MAX_INT_T_VALUE  9223372036854775807
#endif


#ifndef NGX_ALIGNMENT
#define NGX_ALIGNMENT   sizeof(unsigned long)    /* platform word */
#endif

#define ngx_align(d, a)     (((d) + (a - 1)) & ~(a - 1))
#define ngx_align_ptr(p, a)                                                   \
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))


#define ngx_abort       abort


/* TODO: platform specific: array[NGX_INVALID_ARRAY_INDEX] must cause SIGSEGV */
#define NGX_INVALID_ARRAY_INDEX 0x80000000


/* TODO: auto_conf: ngx_inline   inline __inline __inline__ */
#ifndef ngx_inline
#define ngx_inline      inline
#endif

#ifndef INADDR_NONE  /* Solaris */
#define INADDR_NONE  ((unsigned int) -1)
#endif

#ifdef MAXHOSTNAMELEN
#define NGX_MAXHOSTNAMELEN  MAXHOSTNAMELEN
#else
#define NGX_MAXHOSTNAMELEN  256
#endif


#define NGX_MAX_UINT32_VALUE  (uint32_t) 0xffffffff
#define NGX_MAX_INT32_VALUE   (uint32_t) 0x7fffffff


#if (NGX_COMPAT)

#define NGX_COMPAT_BEGIN(slots)  uint64_t spare[slots];
#define NGX_COMPAT_END

#else

#define NGX_COMPAT_BEGIN(slots)
#define NGX_COMPAT_END

#endif


#endif /* _NGX_CONFIG_H_INCLUDED_ */
