/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_rwlock.h
* @date:      2018/3/1 下午3:30
* @desc:
 * http://blog.csdn.net/sina_yangyang/article/details/47026707
*/

//
// Created by daemon.xie on 2018/3/1.
//

#ifndef NGINX_LEARNING_NGX_RWLOCK_H
#define NGINX_LEARNING_NGX_RWLOCK_H

#include <ngx_config.h>
#include <ngx_core.h>

void ngx_rwlock_wlock(ngx_atomic_t *lock);
void ngx_rwlock_rlock(ngx_atomic_t *lock);
void ngx_rwlock_unlock(ngx_atomic_t *lock);

#endif //NGINX_LEARNING_NGX_RWLOCK_H
