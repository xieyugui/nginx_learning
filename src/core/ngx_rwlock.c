/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_rwlock.c
* @date:      2018/3/1 下午3:30
* @desc:
*/

//
// Created by daemon.xie on 2018/3/1.
//

#include <ngx_config.h>
#include <ngx_core.h>

//在有原子变量支持的情况下
#if (NGX_HAVE_ATOMIC_OPS)

#define NGX_RWLOCK_SPIN   2048
#define NGX_RWLOCK_WLOCK  ((ngx_atomic_uint_t) -1)

void
ngx_rwlock_wlock(ngx_atomic_t *lock)
{
    ngx_uint_t i, n;

    for ( ;; ) {

        //先判断一次锁，然后尝试获取锁（compare and set)
        //ngx_atomic_cmp_set 当原子变量lock与old相等时,才能把set设置到lock中
        if (*lock == 0 && ngx_atomic_cmp_set(lock, 0, NGX_RWLOCK_WLOCK)) {
            //如果成功获取到就直接return。
            return;
        }

        //在cpu大于1的情况下才会去自旋
        if (ngx_ncpu > 1) {
            for (n = 1; n < NGX_RWLOCK_SPIN; n <<= 1) {

                for(i = 0; i < n; i++) {
                    //架构体系中专门为了自旋锁而提供的指令，它会告诉CPU现在处于自旋锁等待状态，
                    // 通常一个CPU会将自己置于节能状态，降低功耗。但是当前进程并没有让出正在使用的处理器。
                    ngx_cpu_pause();
                }

                if (*lock == 0 && ngx_atomic_cmp_set(lock, 0, NGX_RWLOCK_WLOCK))
                {
                    return;
                }
            }
        }
        //在一个for循环之内usleep(1) 暂时让出处理器
        ngx_sched_yield();
    }
}

void
ngx_rwlock_rlock(ngx_atomic_t *lock)
{
    ngx_uint_t         i, n;
    ngx_atomic_uint_t  readers;

    for ( ;; ) {
        readers = *lock;

        if (readers != NGX_RWLOCK_WLOCK
            && ngx_atomic_cmp_set(lock, readers, readers + 1))
        {
            return;
        }

        if (ngx_ncpu > 1) {

            for (n = 1; n < NGX_RWLOCK_SPIN; n <<= 1) {

                for (i = 0; i < n; i++) {
                    ngx_cpu_pause();
                }

                readers = *lock;

                //尝试去获取锁，尝试获取锁的次数并不多，而且时间间隔会越来越大
                if (readers != NGX_RWLOCK_WLOCK
                    && ngx_atomic_cmp_set(lock, readers, readers + 1))
                {
                    return;
                }
            }
        }

        ngx_sched_yield();
    }
}

void
ngx_rwlock_unlock(ngx_atomic_t *lock)
{
    ngx_atomic_uint_t  readers;

    readers = *lock;

    if (readers == NGX_RWLOCK_WLOCK) {
        *lock = 0;
        return;
    }

    for ( ;; ) {

        if (ngx_atomic_cmp_set(lock, readers, readers - 1)) {
            return;
        }

        readers = *lock;
    }
}


#else

#if (NGX_HTTP_UPSTREAM_ZONE || NGX_STREAM_UPSTREAM_ZONE)

#error ngx_atomic_cmp_set() is not defined!

#endif


#endif