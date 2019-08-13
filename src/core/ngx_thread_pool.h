/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui
* @software:  CLion
* @file:      ngx_thread_pool.h
* @date:      2018/3/6 下午4:04
* @desc:
 *
 * https://github.com/chronolaw/annotated_nginx/blob/master/nginx/src/core/ngx_thread_pool.h
*/

//
// Created by daemon.xie on 2018/3/6.
//

#ifndef NGINX_LEARNING_NGX_THREAD_POOL_H
#define NGINX_LEARNING_NGX_THREAD_POOL_H

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>

struct ngx_thread_task_s {
    // 链表指针，多个task形成一个链表
    ngx_thread_task_t *next; //指向下一个提交的任务
    ngx_uint_t id; //任务id  每添加一个任务就自增加

    // 用户使用的数据，也就是handler的data参数
    // 用这个参数传递给线程要处理的各种数据
    // 比较灵活的方式是传递一个指针，而不是真正的数据结构内容
    // 例如 struct ctx {xxx *params;};
    //
    // 由ngx_thread_task_alloc分配内存空间并赋值
    void *ctx; //执行回调函数的参数
    // 由线程里的线程执行的函数，真正的工作
    // 执行用户定义的操作，通常是阻塞的
    // 参数data就是上面的ctx
    // handler不能直接看到task，但可以在ctx里存储task指针
    void (*handler)(void *data, ngx_log_t *log);//回调函数   执行完handler后会通过ngx_notify执行event->handler
    // 任务关联的事件对象
    // event.active表示任务是否已经放入任务队列
    // 这里的event并不关联任何socket读写或定时器对象
    // 仅用到它的handler/data成员，当线程完成任务时回调
    // event->data要存储足够的信息，才能够完成请求
    // 可以使用r->ctx，里面存储请求r、连接c等
    ngx_event_t event;//一个任务和一个事件对应  事件在通过ngx_notify在ngx_thread_pool_handler中执行
};

// 线程池结构体
// 线程的数量默认为32个线程
// 任务等待队列默认是65535
typedef struct ngx_thread_pool_s ngx_thread_pool_t;

// 根据配置创建线程池结构体对象,添加进线程池模块配置结构体里的数组
ngx_thread_pool_t *ngx_thread_pool_add(ngx_conf_t *cf, ngx_str_t *name);

// 根据名字获取线程池
// 遍历线程池数组，找到名字对应的结构体
ngx_thread_pool_t *ngx_thread_pool_get(ngx_cycle_t *cycle, ngx_str_t *name);


// 创建一个线程任务结构体
// 参数size是用户数据ctx的大小，位于task之后
ngx_thread_task_t *ngx_thread_task_alloc(ngx_pool_t *pool, size_t *size);

// 把任务放入线程池，由线程执行
// 锁定互斥量，防止多线程操作的竞态
// 如果等待处理的任务数大于设置的最大队列数,那么添加任务失败
// 操作完waiting、queue、ngx_thread_pool_task_id后解锁
ngx_int_t ngx_thread_task_post(ngx_thread_pool_t *tp, ngx_thread_task_t *task);

#endif //NGINX_LEARNING_NGX_THREAD_POOL_H
