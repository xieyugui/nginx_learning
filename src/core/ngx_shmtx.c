/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_shmtx.c
* @date:      2018/3/2 上午9:24
* @desc:
*/

//
// Created by daemon.xie on 2018/3/2.
//

#include <ngx_config.h>
#include <ngx_core.h>


#if (NGX_HAVE_ATOMIC_OPS)

static void ngx_shmtx_wakeup(ngx_shmtx_t *mtx);

//初始化mtx互斥锁
ngx_int_t
ngx_shmtx_create(ngx_shmtx_t *mtx, ngx_shmtx_sh_t *addr, u_char *name)
{
    mtx->lock = &addr->lock; //直接执行共享内存空间addr中的lock区间中

    if (mtx->spin == (ngx_uint_t) -1) { //注意，当spin值为-1时，表示不能使用信号量，这时直接返回成功
        return NGX_OK;
    }

    mtx->spin = 2048; //spin值默认为2048

    //同时使用信号量
#if (NGX_HAVE_POSIX_SEM)
    mtx->wait = &addr->wait;

    /*
    int  sem init (sem_t  sem,  int pshared,  unsigned int value) ,
    其中，参数sem即为我们定义的信号量，而参数pshared将指明sem信号量是用于进程间同步还是用于线程间同步，当pshared为0时表示线程间同步，
    而pshared为1时表示进程间同步。由于Nginx的每个进程都是单线程的，因此将参数pshared设为1即可。参数value表示信号量sem的初始值。
     */
    //以多进程使用的方式初始化sem信号量，sem初始值为0
    if (sem_init(&mtx->sem, 1, 0) == -1) {
        ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, ngx_errno,
                      "sem_init() failed");
    } else {
        //semaphore为1时表示获取锁将可能使用到的信号量
        mtx->semaphore = 1; //在信号量初始化成功后，设置semaphore标志位为1
    }

#endif

    return NGX_OK;
}

//销毁mtx互斥锁
void
ngx_shmtx_destroy(ngx_shmtx_t *mtx)
{
#if (NGX_HAVE_POSIX_SEM) //支持信号量时才有代码需要执行

    if (mtx->semaphore) { //当这把锁的spin值不为(ngx_uint_t)    -1时，且初始化信号量成功，semaphore标志位才为1
        if (sem_destroy(&mtx->sem) == -1) { //销毁信号量
            ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, ngx_errno,
                          "sem_destroy() failed");
        }
    }

#endif
}

/*
首先是判断mtx的lock域是否等于0，如果不等于，那么就直接返回false好了，如果等于的话，那么就要调用原子操作ngx_atomic_cmp_set了，
它用于比较mtx的lock域，如果等于零，那么设置为当前进程的进程id号，否则返回false。
*/ //不管能不能获得到锁都返回  nginx是原子变量和信号量合作以实现高效互斥锁的
ngx_uint_t
ngx_shmtx_trylock(ngx_shmtx_t *mtx)
{
    return (*mtx->lock == 0 && ngx_atomic_cmp_set(mtx->lock, 0, ngx_pid));
}

/*
阻塞式获取互斥锁的ngx_shmtx_lock方法较为复杂，在不支持信号量时它与14.3.3节介绍的自旋锁几乎完全相同，但在支持了信号量后，它
将有可能使进程进入睡眠状态。
*/
//这里可以看出支持原子操作的系统，他的信号量其实就是自旋和信号量的结合   nginx是原子变量和信号量合作以实现高效互斥锁的
void
ngx_shmtx_lock(ngx_shmtx_t *mtx)
{
    ngx_uint_t i,n;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0, "shmtx lock");

    //一个死循环，不断的去看是否获取了锁，直到获取了之后才退出
    //所以支持原子变量的
    for ( ;; ) {
        if (*mtx->lock == 0 && ngx_atomic_cmp_set(mtx->lock, 0, ngx_pid)) {
            return;
        }
        //仅在多处理器状态下spin值才有意义，否则PAUSE指令是不会执行的
        if (ngx_ncpu > 1) {
            //循环执行PAUSE，检查锁是否已经释放
            for (n = 1; n < mtx->spin; n <<= 1) {
                //随着长时间没有获得到锁，将会执行更多次PAUSE才会检查锁
                for (i = 0; i < n; i++) {
                    //架构体系中专门为了自旋锁而提供的指令，它会告诉CPU现在处于自旋锁等待状态，
                    // 通常一个CPU会将自己置于节能状态，降低功耗。但是当前进程并没有让出正在使用的处理器。
                    ngx_cpu_pause();
                }

                //再次由共享内存中获得lock原子变量的值
                if (*mtx->lock == 0
                    && ngx_atomic_cmp_set(mtx->lock, 0, ngx_pid))
                {
                    return;
                }
            }
        }

#if (NGX_HAVE_POSIX_SEM) //支持信号量时才继续执行

            if (mtx->semaphore) {//semaphore标志位为1才使用信号量
                (void) ngx_atomic_fetch_add(mtx->wait, 1);

                //重新获取一次可能虚共享内存中的lock原子变量
                if (*mtx->lock == 0 && ngx_atomic_cmp_set(mtx->lock, 0, ngx_pid)) {
                    (void) ngx_atomic_fetch_add(mtx->wait, -1);
                    return;
                }

                ngx_log_debug1(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
                           "shmtx wait %uA", *mtx->wait);

                //如果没有拿到锁，这时Nginx进程将会睡眠，直到其他进程释放了锁
                /*
                    检查信号量sem的值，如果sem值为正数，则sem值减1，表示拿到了信号量互斥锁，同时sem wait方法返回o。如果sem值为0或
                    者负数，则当前进程进入睡眠状态，等待其他进程使用ngx_shmtx_unlock方法释放锁（等待sem信号量变为正数），到时Linux内核
                    会重新调度当前进程，继续检查sem值是否为正，重复以上流程
                */
                while (sem_wait(&mtx->sem) == -1) {
                    ngx_err_t  err;

                    err = ngx_errno;

                    if (err != NGX_EINTR) {//当EINTR信号出现时，表示sem wait只是被打断，并不是出错
                        ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, err,
                                      "sem_wait() failed while waiting on shmtx");
                        break;
                    }
                }

                ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
                               "shmtx awoke");

                continue; //循环检查lock锁的值，注意，当使用信号量后不会调用sched_yield
            }

#endif
        //在一个for循环之内usleep(1) 暂时让出处理器 放弃当前的执行机会，等待下次调度
        ngx_sched_yield(); //在不使用信号量时，调用sched_yield将会使当前进程暂时“让出”处理器
    }
}

/*
ngx_shmtx_unlock方法会释放锁，虽然这个释放过程不会阻塞进程，但设置原子变量lock值时是可能失败的，因为多进程在同时修改lock值，
而ngx_atomic_cmp_s et方法要求参数old的值与lock值相同时才能修改成功，因此，ngx_atomic_cmp_set方法会在循环中反复执行，直到返回
成功为止。
*/
//判断锁的lock域与当前进程的进程id是否相等，如果相等的话，那么就将lock设置为0，然后就相当于释放了锁。
void
ngx_shmtx_unlock(ngx_shmtx_t *mtx)
{
    if (mtx->spin != (ngx_uint_t) -1) {
        ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0, "shmtx unlock");
    }

    if (ngx_atomic_cmp_set(mtx->lock, ngx_pid, 0)) {
        ngx_shmtx_wakeup(mtx);
    }
}

ngx_uint_t
ngx_shmtx_force_unlock(ngx_shmtx_t *mtx, ngx_pid_t pid)
{
    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
                   "shmtx forced unlock");

    if (ngx_atomic_cmp_set(mtx->lock, pid, 0)) {
        ngx_shmtx_wakeup(mtx);
        return 1;
    }

    return 0;
}

static void
ngx_shmtx_wakeup(ngx_shmtx_t *mtx)
{
#if (NGX_HAVE_POSIX_SEM)
    ngx_atomic_uint_t  wait;

    if (!mtx->semaphore) {
        return;
    }

    for ( ;; ) {

        wait = *mtx->wait;
        //如果lock锁原先的值为o，也就是说，并没有让某个进程持有锁，这时直接返回；或者，semaphore标志位为0，表示不需要使用信号量，也立即返回
        if ((ngx_atomic_int_t) wait <= 0) {
            return;
        }

        if (ngx_atomic_cmp_set(mtx->wait, wait, wait - 1)) {
            break;
        }
    }

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
                   "shmtx wake %uA", wait);

    //释放信号量锁时是不会使进程睡眠的
    //通过sem_post将信号量sem加1，表示当前进程释放了信号量互斥锁，通知其他进程的sem_wait继续执行
    if (sem_post(&mtx->sem) == -1) {
        ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, ngx_errno,
                      "sem_post() failed while wake shmtx");
    }

#endif
}

#else  //else后的锁是文件锁实现的ngx_shmtx_t锁   //不支持原子操作，则通过文件锁实现

ngx_int_t
ngx_shmtx_create(ngx_shmtx_t *mtx, ngx_shmtx_sh_t *addr, u_char *name)
{
    //不用在调用ngx_shmtx_create方法前先行赋值给ngx_shmtx_t结构体中的成员
    if (mtx->name) {
        /*
         如果ngx_shmtx_t中的name成员有值，那么如果与name参数相同，意味着mtx互斥锁已经初始化过了；否则，需要先销毁mtx中的互斥锁再重新分配mtx
          */
        if (ngx_strcmp(name, mtx->name) == 0) {
            mtx->name = name; //如果name参数与ngx_shmtx_t中的name成员桐同，则表示已经初始化了
            return NGX_OK; //既然曾经初始化过，证明fd句柄已经打开过，直接返回成功即可
        }

        /*
           如果ngx_s hmtx_t中的name与参数name不一致，说明这一次使用了一个新的文件作为文件锁，那么先调用ngx_shmtx_destory方法销毁原文件锁
          */
        ngx_shmtx_destroy(mtx);
    }

    mtx->fd = ngx_open_file(name, NGX_FILE_RDWR, NGX_FILE_CREATE_OR_OPEN,
                            NGX_FILE_DEFAULT_ACCESS);

    if (mtx->fd == NGX_INVALID_FILE) { //一旦文件因为各种原因（如权限不够）无法打开，通常会出现无法运行错误
        ngx_log_error(NGX_LOG_EMERG, ngx_cycle->log, ngx_errno,
                      ngx_open_file_n " \"%s\" failed", name);
        return NGX_ERROR;
    }

    //由于只需要这个文件在内核中的INODE信息，所以可以把文件删除，只要fd可用就行
    if (ngx_delete_file(name) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, ngx_errno,
                      ngx_delete_file_n " \"%s\" failed", name);
    }

    mtx->name = name;

    return NGX_OK;
}


void
ngx_shmtx_destroy(ngx_shmtx_t *mtx)
{
    if (ngx_close_file(mtx->fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, ngx_errno,
                      ngx_close_file_n " \"%s\" failed", mtx->name);
    }
}

//ngx_shmtx_trylock方法试图使用非阻塞的方式获得锁，返回1时表示获取锁成功，返回0表示获取锁失败
ngx_uint_t
ngx_shmtx_trylock(ngx_shmtx_t *mtx)  //ngx_shmtx_unlock和ngx_shmtx_lock对应
{
    ngx_err_t  err;
    printf("yang test xxxxxxxxxxxxxxxxxxxxxx 1111111111111111111111\r\n");

    //由14.7节介绍过的ngx_t rylock_fd方法实现非阻塞互斥文件锁的获取
    err = ngx_trylock_fd(mtx->fd);

    if (err == 0) {
        return 1;
    }

    if (err == NGX_EAGAIN) { //如果err错误码是NGX EAGAIN，则农示现在锁已经被其他进程持有了
        return 0;
    }

#if __osf__ /* Tru64 UNIX */

    if (err == NGX_EACCES) {
        return 0;
    }

#endif

    ngx_log_abort(err, ngx_trylock_fd_n " %s failed", mtx->name);

    return 0;
}


/*
ngx_shmtx_lock方法将会在获取锁失败时阻塞代码的继续执行，它会使当前进程处于睡眠状态，等待其他进程释放锁后内核唤醒它。
可见，它是通过14.7节介绍的ngx_lock_fd方法实现的，如下所示。
*/
void
ngx_shmtx_lock(ngx_shmtx_t *mtx) //ngx_shmtx_unlock和ngx_shmtx_lock对应
{
    ngx_err_t  err;

    err = ngx_lock_fd(mtx->fd);

    if (err == 0) { //ngx_lock_fd方法返回0时表示成功地持有锁，返回-1时表示出现错误
        return;
    }

    ngx_log_abort(err, ngx_lock_fd_n " %s failed", mtx->name);
}

//ngx_shmtx_lock方法没有返回值，因为它一旦返回就相当于获取到互斥锁了，这会使得代码继续向下执行。
//ngx_shmtx unlock方法通过调用ngx_unlock_fd方法来释放文件锁
void
ngx_shmtx_unlock(ngx_shmtx_t *mtx)
{
    ngx_err_t  err;

    err = ngx_unlock_fd(mtx->fd);

    if (err == 0) {
        return;
    }

    ngx_log_abort(err, ngx_unlock_fd_n " %s failed", mtx->name);
}


ngx_uint_t
ngx_shmtx_force_unlock(ngx_shmtx_t *mtx, ngx_pid_t pid)
{
    return 0;
}



#endif