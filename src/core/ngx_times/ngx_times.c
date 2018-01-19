/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_times.c
* @date:      2018/1/17 下午5:24
* @desc:
*/

//
// Created by daemon.xie on 2018/1/17.
//

#include <ngx_config.h>
#include <ngx_core.h>

#define NGX_TIME_SLOTS 64

/*
nginx这里采用了64个slot时间，也就是每次更新时间的时候都是更新下一个
slot，如果读操作同时进行，读到的还是之前的slot，并没有被改变，当然这里只能是尽量减少了时间混乱的几率，因为slot的个数不是无限的，slot是循环的，
写操作总有几率会写到读操作的slot上。不过nginx现在实际上并没有采用多线程的方式，而且在信号处理中只是更新cached_err_log_time，所以对其他时间变量
的读访问是不会发生混乱的
*/
static ngx_uint_t slot;
static ngx_atomic_t ngx_time_lock;

//格林威治时间1970年1月1日凌晨0点0分0秒到当前时间的毫秒数
volatile ngx_msec_t ngx_current_msec;
//ngx_time_t结构体形式的当前时间
volatile ngx_time_t *ngx_cached_time;
//用于记录error_log的当前时间字符串，它的格式类似于：”1970/09/28  12：OO：OO”
volatile ngx_str_t ngx_cached_err_log_time;

//用于HTTP相关的当前时间字符串，它的格式类似于：”Mon，28  Sep  1970  06：OO：OO  GMT”
volatile ngx_str_t       ngx_cached_http_time;
//用于记录HTTP曰志的当前时间字符串，它的格式类似于：”28/Sep/1970：12：OO：00  +0600n"
volatile ngx_str_t       ngx_cached_http_log_time;
//以IS0 8601标准格式记录下的字符串形式的当前时间
volatile ngx_str_t       ngx_cached_http_log_iso8601;
volatile ngx_str_t       ngx_cached_syslog_time;

#if !(NGX_WIN32)

/*
 * localtime() and localtime_r() are not Async-Signal-Safe functions, therefore,
 * they must not be called by a signal handler, so we use the cached
 * GMT offset value. Fortunately the value is changed only two times a year.
 */

static ngx_int_t         cached_gmtoff;
#endif

static ngx_time_t        cached_time[NGX_TIME_SLOTS]; //系统当前时间，见ngx_time_update
static u_char            cached_err_log_time[NGX_TIME_SLOTS]
                                [sizeof("1970/09/28 12:00:00")];
static u_char            cached_http_time[NGX_TIME_SLOTS]
                                [sizeof("Mon, 28 Sep 1970 06:00:00 GMT")]; //年月日 时分秒 星期 格式
static u_char            cached_http_log_time[NGX_TIME_SLOTS]
                                [sizeof("28/Sep/1970:12:00:00 +0600")];
static u_char            cached_http_log_iso8601[NGX_TIME_SLOTS]
                                [sizeof("1970-09-28T12:00:00+06:00")];
static u_char            cached_syslog_time[NGX_TIME_SLOTS]
                                [sizeof("Sep 28 12:00:00")];

static char  *week[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static char  *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
//初始化nginx环境的当前时间
void
ngx_time_init(void)
{
    ngx_cached_err_log_time.len = sizeof("1970/09/28 12:00:00") - 1;
    ngx_cached_http_time.len = sizeof("Mon, 28 Sep 1970 06:00:00 GMT") - 1;
    ngx_cached_http_log_time.len = sizeof("28/Sep/1970:12:00:00 +0600") - 1;
    ngx_cached_http_log_iso8601.len = sizeof("1970-09-28T12:00:00+06:00") - 1;
    ngx_cached_syslog_time.len = sizeof("Sep 28 12:00:00") - 1;

    ngx_cached_time = &cached_time[0];

    ngx_time_update();
}

void
ngx_time_update(void)
{
    u_char          *p0, *p1, *p2, *p3, *p4;
    ngx_tm_t         tm, gmt;
    time_t           sec;
    ngx_uint_t       msec;
    ngx_time_t      *tp;
    struct timeval   tv;

    if (!ngx_trylock(&ngx_time_lock)) {
        return;
    }

    ngx_gettimeofday(&tv);

    sec = tv.tv_sec;
    msec = tv.tv_usec / 1000;

    ngx_current_msec = (ngx_msec_t) sec * 1000 + msec;

    tp = &cached_time[slot];//读当前时间缓存

    if (tp->sec == sec) {//如果缓存的时间秒=当前时间秒，直接更新当前slot元素的msec并返回，否则更新下一个slot数组元素；
        tp->msec = msec;
        ngx_unlock(&ngx_time_lock);
        return;
    }

    if (slot == NGX_TIME_SLOTS - 1) {
        slot = 0;
    } else {
        slot++;
    }

    tp = &cached_time[slot];

    tp->sec = sec;
    tp->msec = msec;

    ngx_gmtime(sec, &gmt);


    p0 = &cached_http_time[slot][0];

    (void) ngx_sprintf(p0, "%s, %02d %s %4d %02d:%02d:%02d GMT",
                       week[gmt.ngx_tm_wday], gmt.ngx_tm_mday,
                       months[gmt.ngx_tm_mon - 1], gmt.ngx_tm_year,
                       gmt.ngx_tm_hour, gmt.ngx_tm_min, gmt.ngx_tm_sec);

#if (NGX_HAVE_GETTIMEZONE)

    tp->gmtoff = ngx_gettimezone();
    ngx_gmtime(sec + tp->gmtoff * 60, &tm);

#elif (NGX_HAVE_GMTOFF)

    ngx_localtime(sec, &tm);
    cached_gmtoff = (ngx_int_t) (tm.ngx_tm_gmtoff / 60);
    tp->gmtoff = cached_gmtoff;

#else

    ngx_localtime(sec, &tm);
    cached_gmtoff = ngx_timezone(tm.ngx_tm_isdst);
    tp->gmtoff = cached_gmtoff;

#endif


    p1 = &cached_err_log_time[slot][0];

    (void) ngx_sprintf(p1, "%4d/%02d/%02d %02d:%02d:%02d",
                       tm.ngx_tm_year, tm.ngx_tm_mon,
                       tm.ngx_tm_mday, tm.ngx_tm_hour,
                       tm.ngx_tm_min, tm.ngx_tm_sec);


    p2 = &cached_http_log_time[slot][0];

    (void) ngx_sprintf(p2, "%02d/%s/%d:%02d:%02d:%02d %c%02d%02d",
                       tm.ngx_tm_mday, months[tm.ngx_tm_mon - 1],
                       tm.ngx_tm_year, tm.ngx_tm_hour,
                       tm.ngx_tm_min, tm.ngx_tm_sec,
                       tp->gmtoff < 0 ? '-' : '+',
                       ngx_abs(tp->gmtoff / 60), ngx_abs(tp->gmtoff % 60));

    p3 = &cached_http_log_iso8601[slot][0];

    (void) ngx_sprintf(p3, "%4d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",
                       tm.ngx_tm_year, tm.ngx_tm_mon,
                       tm.ngx_tm_mday, tm.ngx_tm_hour,
                       tm.ngx_tm_min, tm.ngx_tm_sec,
                       tp->gmtoff < 0 ? '-' : '+',
                       ngx_abs(tp->gmtoff / 60), ngx_abs(tp->gmtoff % 60));

    p4 = &cached_syslog_time[slot][0];

    (void) ngx_sprintf(p4, "%s %2d %02d:%02d:%02d",
                       months[tm.ngx_tm_mon - 1], tm.ngx_tm_mday,
                       tm.ngx_tm_hour, tm.ngx_tm_min, tm.ngx_tm_sec);


    /*
  可以看到ngx_memory_barrier()之后是四条赋值语句，如果没有 ngx_memory_barrier()，编译器可能会将 ngx_cached_time = tp ，
  ngx_cached_http_time.data = p0，ngx_cached_err_log_time.data = p1， ngx_cached_http_log_time.data = p2 分别和之前的
  tp = &cached_time[slot] , p0 = &cached_http_time[slot][0] , p1 = &cached_err_log_time[slot][0] , p2 = &cached_http_log_time[slot][0]
  合并优化掉，这样的后果是 ngx_cached_time，ngx_cached_http_time，ngx_cached_err_log_time， ngx_cached_http_log_time这四个时间缓存的
  不一致性时长增大了，因为在最后一个ngx_sprintf执行完后这四个时间缓存才一致，在这之前如果有其他地方正在读时间缓存就可能导致读到的时间
  不正确或者不一致，而采用ngx_memory_barrier() 后，时间缓存更新到一致的 状态只需要几个时钟周期，因为只有四条赋值指令，显然在这么短的时
  间内发生读时间缓存的概率会小的多了。从这里可以看出Igor考虑是非常细致的。
  */
    ngx_memory_barrier();//禁止编译器对后面的语句优化，如果没有这个限制，编译器可能将前后两部分代码合并，可能导致这6个时间更新出现间隔，期间若被读取会出现时间不一致的情况

    ngx_cached_time = tp;
    ngx_cached_http_time.data = p0;
    ngx_cached_err_log_time.data = p1;
    ngx_cached_http_log_time.data = p2;
    ngx_cached_http_log_iso8601.data = p3;
    ngx_cached_syslog_time.data = p4;

    ngx_unlock(&ngx_time_lock);
}

#if !(NGX_WIN32)
//它会在每次执行信号处理函数的时候被调用，也就是在ngx_signal_handler（）函数中。
void
ngx_time_sigsafe_update(void)
{
    u_char          *p, *p2;
    ngx_tm_t         tm;
    time_t           sec;
    ngx_time_t      *tp;
    struct timeval   tv;

    if (!ngx_trylock(&ngx_time_lock)) {
        return;
    }

    ngx_gettimeofday(&tv);

    sec = tv.tv_sec;

    tp = &cached_time[slot];

    if (tp->sec == sec) {
        ngx_unlock(&ngx_time_lock);
        return;
    }

    if (slot == NGX_TIME_SLOTS - 1) {
        slot = 0;
    } else {
        slot++;
    }

    tp = &cached_time[slot];

    tp->sec = 0;

    ngx_gmtime(sec + cached_gmtoff * 60, &tm);

    p = &cached_err_log_time[slot][0];

    (void) ngx_sprintf(p, "%4d/%02d/%02d %02d:%02d:%02d",
                       tm.ngx_tm_year, tm.ngx_tm_mon,
                       tm.ngx_tm_mday, tm.ngx_tm_hour,
                       tm.ngx_tm_min, tm.ngx_tm_sec);

    p2 = &cached_syslog_time[slot][0];

    (void) ngx_sprintf(p2, "%s %2d %02d:%02d:%02d",
                       months[tm.ngx_tm_mon - 1], tm.ngx_tm_mday,
                       tm.ngx_tm_hour, tm.ngx_tm_min, tm.ngx_tm_sec);


    /*
    可以看到ngx_memory_barrier()之后是四条赋值语句，如果没有 ngx_memory_barrier()，编译器可能会将 ngx_cached_time = tp ，
    ngx_cached_http_time.data = p0，ngx_cached_err_log_time.data = p1， ngx_cached_http_log_time.data = p2 分别和之前的
    tp = &cached_time[slot] , p0 = &cached_http_time[slot][0] , p1 = &cached_err_log_time[slot][0] , p2 = &cached_http_log_time[slot][0]
    合并优化掉，这样的后果是 ngx_cached_time，ngx_cached_http_time，ngx_cached_err_log_time， ngx_cached_http_log_time这四个时间缓存的
    不一致性时长增大了，因为在最后一个ngx_sprintf执行完后这四个时间缓存才一致，在这之前如果有其他地方正在读时间缓存就可能导致读到的时间
    不正确或者不一致，而采用ngx_memory_barrier() 后，时间缓存更新到一致的 状态只需要几个时钟周期，因为只有四条赋值指令，显然在这么短的时
    间内发生读时间缓存的概率会小的多了。从这里可以看出Igor考虑是非常细致的。
    */
    ngx_memory_barrier();

    //在信号处理中只是更新cached_err_log_time ngx_cached_syslog_time，所以对其他时间变量的读访问是不会发生混乱的
    ngx_cached_err_log_time.data = p;
    ngx_cached_syslog_time.data = p2;

    ngx_unlock(&ngx_time_lock);
}

#endif


u_char *
ngx_http_time(u_char *buf, time_t t)
{
    ngx_tm_t  tm;

    ngx_gmtime(t, &tm);

    return ngx_sprintf(buf, "%s, %02d %s %4d %02d:%02d:%02d GMT",
                       week[tm.ngx_tm_wday],
                       tm.ngx_tm_mday,
                       months[tm.ngx_tm_mon - 1],
                       tm.ngx_tm_year,
                       tm.ngx_tm_hour,
                       tm.ngx_tm_min,
                       tm.ngx_tm_sec);
}

u_char *
ngx_http_cookie_time(u_char *buf, time_t t)
{
    ngx_tm_t  tm;

    ngx_gmtime(t, &tm);

    /*
     * Netscape 3.x does not understand 4-digit years at all and
     * 2-digit years more than "37"
     */

    return ngx_sprintf(buf,
                       (tm.ngx_tm_year > 2037) ?
                       "%s, %02d-%s-%d %02d:%02d:%02d GMT":
                       "%s, %02d-%s-%02d %02d:%02d:%02d GMT",
                       week[tm.ngx_tm_wday],
                       tm.ngx_tm_mday,
                       months[tm.ngx_tm_mon - 1],
                       (tm.ngx_tm_year > 2037) ? tm.ngx_tm_year:
                       tm.ngx_tm_year % 100,
                       tm.ngx_tm_hour,
                       tm.ngx_tm_min,
                       tm.ngx_tm_sec);
}

void
ngx_gmtime(time_t t, ngx_tm_t *tp)
{
    ngx_int_t   yday;
    ngx_uint_t  n, sec, min, hour, mday, mon, year, wday, days, leap;

    /* the calculation is valid for positive time_t only */

    n = (ngx_uint_t) t;

    days = n / 86400;

    /* January 1, 1970 was Thursday */

    wday = (4 + days) % 7;

    n %= 86400;
    hour = n / 3600;
    n %= 3600;
    min = n / 60;
    sec = n % 60;

    /*
     * the algorithm based on Gauss' formula,
     * see src/http/ngx_http_parse_time.c
     */

    /* days since March 1, 1 BC */
    days = days - (31 + 28) + 719527;

    /*
     * The "days" should be adjusted to 1 only, however, some March 1st's go
     * to previous year, so we adjust them to 2.  This causes also shift of the
     * last February days to next year, but we catch the case when "yday"
     * becomes negative.
     */

    year = (days + 2) * 400 / (365 * 400 + 100 - 4 + 1);

    yday = days - (365 * year + year / 4 - year / 100 + year / 400);

    if (yday < 0) {
        leap = (year % 4 == 0) && (year % 100 || (year % 400 == 0));
        yday = 365 + leap + yday;
        year--;
    }

    /*
     * The empirical formula that maps "yday" to month.
     * There are at least 10 variants, some of them are:
     *     mon = (yday + 31) * 15 / 459
     *     mon = (yday + 31) * 17 / 520
     *     mon = (yday + 31) * 20 / 612
     */

    mon = (yday + 31) * 10 / 306;

    /* the Gauss' formula that evaluates days before the month */

    mday = yday - (367 * mon / 12 - 30) + 1;

    if (yday >= 306) {

        year++;
        mon -= 10;

        /*
         * there is no "yday" in Win32 SYSTEMTIME
         *
         * yday -= 306;
         */

    } else {

        mon += 2;

        /*
         * there is no "yday" in Win32 SYSTEMTIME
         *
         * yday += 31 + 28 + leap;
         */
    }

    tp->ngx_tm_sec = (ngx_tm_sec_t) sec;
    tp->ngx_tm_min = (ngx_tm_min_t) min;
    tp->ngx_tm_hour = (ngx_tm_hour_t) hour;
    tp->ngx_tm_mday = (ngx_tm_mday_t) mday;
    tp->ngx_tm_mon = (ngx_tm_mon_t) mon;
    tp->ngx_tm_year = (ngx_tm_year_t) year;
    tp->ngx_tm_wday = (ngx_tm_wday_t) wday;
}

time_t
ngx_next_time(time_t when)
{
    time_t     now, next;
    struct tm  tm;

    now = ngx_time();

    ngx_libc_localtime(now, &tm);

    tm.tm_hour = (int) (when / 3600);
    when %= 3600;
    tm.tm_min = (int) (when / 60);
    tm.tm_sec = (int) (when % 60);

    next = mktime(&tm);

    if (next == -1) {
        return -1;
    }

    if (next - now > 0) {
        return next;
    }

    tm.tm_mday++;

    /* mktime() should normalize a date (Jan 32, etc) */

    next = mktime(&tm);

    if (next != -1) {
        return next;
    }

    return -1;
}
