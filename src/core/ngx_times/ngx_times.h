/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_times.h
* @date:      2018/1/17 下午5:24
* @desc:
 *
 * 参考https://github.com/xieyugui/reading-code-of-nginx-1.9.2/blob/master/nginx-1.9.2/src/core/ngx_times.c
*/

//
// Created by daemon.xie on 2018/1/17.
//

#ifndef NGX_TIMES_NGX_TIMES_H
#define NGX_TIMES_NGX_TIMES_H

#include <ngx_config.h>
#include <ngx_core.h>

typedef struct {
    time_t sec; //格林威治时间1970年1月1日凌晨o点o分o秒到当前时间的秒数
    ngx_uint_t msec; //sec成员只能精确到秒，msec别是当前时间相对于sec的毫秒偏移量
    ngx_int_t gmtoff; //时区
} ngx_time_t;

//初始化缓存时间，以及各种时间格式
void ngx_time_init(void);

//更新时间
void ngx_time_update(void);

//当信号切入时进行时间更新
void ngx_time_sigsafe_update(void);

//将时间t转换成“Mon, 28 Sep 1970 06:00:00 GMT”形式的时间
u_char *ngx_http_time(u_char *buf, time_t t);

//将时间t转换成“Mon. 28-Sep-70 06:00:00 GMT”形式适用于cookie的时间
u_char *ngx_http_cookie_time(u_char *buf, time_t t);

//将时间t转换成ngx_tm_t类型的时间
void ngx_gmtime(time_t t, ngx_tm_t *tp);

//when参数只是代表了当天的时间，只有秒+分钟+小时，顾名思义，寻找下一次这个时间点出现的时候的绝对时间time_t值
time_t ngx_next_time(time_t when);

//用来将参数timeptr所指的tm结构数据转换成从公元1970年1月1日0时0分0 秒算起至今的UTC时间所经过的秒数
#define  ngx_next_time_n "mktime()"

//ngx_time_t结构体形式的当前时间
extern volatile ngx_time_t *ngx_cached_time;

//格林威治时间1970年1月1日凌晨o点o分o秒到当前时间的秒数
#define ngx_time()  ngx_cached_time->sec

//获取当前nginx缓存的时间
#define ngx_timeofday()      (ngx_time_t *) ngx_cached_time

//1970/09/28 12:00:00
extern volatile ngx_str_t    ngx_cached_err_log_time;
//Mon, 28 Sep 1970 06:00:00 GMT
extern volatile ngx_str_t    ngx_cached_http_time;
//28/Sep/1970:12:00:00 +0600
extern volatile ngx_str_t    ngx_cached_http_log_time;
//1970-09-28T12:00:00+06:00
extern volatile ngx_str_t    ngx_cached_http_log_iso8601;
//Sep 28 12:00:00
extern volatile ngx_str_t    ngx_cached_syslog_time;

/*
 * milliseconds elapsed since epoch and truncated to ngx_msec_t,
 * used in event timers
 */
//格林威治时间1970年1月1日凌晨0点0分0秒到当前时间的毫秒数
extern volatile ngx_msec_t  ngx_current_msec;



#endif //NGX_TIMES_NGX_TIMES_H
