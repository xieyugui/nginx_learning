/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_hash.h
* @date:      2017/12/28 下午6:09
* @desc:
 *
 * 讲解 https://segmentfault.com/a/1190000002770345
 * https://github.com/xieyugui/reading-code-of-nginx-1.9.2/blob/master/nginx-1.9.2/src/core/ngx_hash.h
 * http://blog.csdn.net/u012062760/article/details/48392187
 * http://blog.csdn.net/livelylittlefish/article/details/6636229
 * https://github.com/cc1989/nginx_report/blob/master/nginx_test/hash/ngx_hash_wildcard_t_test.c
*/

//
// Created by daemon.xie on 2017/12/28.
//

#ifndef NGX_HASH_NGX_HASH_H
#define NGX_HASH_NGX_HASH_H

#include <ngx_config.h>
#include <ngx_core.h>

//实际一个ngx_hash_elt_t暂用的空间为NGX_HASH_ELT_SIZE，比sizeof(ngx_hash_elt_t)大，因为需要存储具体的字符串name
typedef struct {
    void *value; //value，即某个key对应的值，即<key,value>中的value
    u_short len; //name长度  key的长度
    u_char name[1]; //某个要hash的数据(在nginx中表现为字符串)，即<key,value>中的key     都是小写字母
//ngx_hash_elt_t是ngx_hash_t->buckets[i]桶中的具体成员
} ngx_hash_elt_t;//hash元素结构

typedef struct {
    ngx_hash_elt_t **buckets; //hash桶(有size个桶)    指向各个桶的头部指针，也就是bucket[]数组，bucket[I]又指向每个桶中的第一个ngx_hash_elt_t成员
    ngx_uint_t size; //hash桶个数，注意是桶的个数，不是每个桶中的成员个数
} ngx_hash_t;

/*
ngx_hash_wildcard_t专用于表示前置或后置通配符的哈希表，如：前置*.test.com，后置:www.test.* ，它只是对ngx_hash_t的简单封装，
是由一个基本哈希表hash和一个额外的value指针，当使用ngx_hash_wildcard_t通配符哈希表作为容器元素时，可以使用value指向用户数据。
 */
typedef struct {
    ngx_hash_t hash;
    void *value;//value这个字段是用来存放某个已经达到末尾的通配符url对应的value值，如果通配符url没有达到末尾，这个字段为NULL
} ngx_hash_wildcard_t;

//hash表中元素ngx_hash_elt_t 预添加哈希散列元素结构 ngx_hash_key_t
typedef struct {
    ngx_str_t key;  // 索引值
    ngx_uint_t key_hash; //对应hash值,由哈希函数根据key计算出的值. 将来此元素代表的结构体会被插入bucket[key_hash % size]
    void *value; // 内容
} ngx_hash_key_t;

/*
┏━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃  ┃    表7-8 Nginx提供的两种散列方法                                                                     ┃
┣━╋━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃  ┃    散列方法                                      ┃    意义                                          ┃
┣━┻━━━━━━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_uint_t ngx_hash_key(u_char *data, size_t len)     ┃  使用BKDR算法将任意长度的字符串映射为整型        ┃
┣━━━━━━━━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                                      ┃  将字符串全小写后，再使用BKDR算法将任意长度的字  ┃
┃.gx_uint_t ngx_hash_key_lc(I_char *data, size_t len)  ┃符串映射为整型                                    ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━┛
*/
// 哈希函数
typedef ngx_uint_t (*ngx_hash_key_pt) (u_char *data, size_t len);

/*
Nginx对于server- name主机名通配符的支持规则。
    首先，选择所有字符串完全匹配的server name，如www.testweb.com。
    其次，选择通配符在前面的server name，如*.testweb.com。
    再次，选择通配符在后面的server name，如www.testweb.*。
ngx_hash_combined_t是由3个哈希表组成，一个普通hash表hash，一个包含前向通配符的hash表wc_head和一个包含后向通配符的hash表 wc_tail。
*/
typedef struct {
    ngx_hash_t hash; //普通hash，完全匹配
    ngx_hash_wildcard_t *wc_head; //前置通配符hash
    ngx_hash_wildcard_t *wc_tail; //后置通配符hash
} ngx_hash_combined_t;

//hash初始化结构
typedef struct {
    ngx_hash_t *hash; // 用于管理哈希表的结构体
    ngx_hash_key_pt key; // 哈希函数

    /*  max_size和bucket_size的意义
        max_size表示最多分配max_size个桶，每个桶中的元素(ngx_hash_elt_t)个数 * NGX_HASH_ELT_SIZE(&names[n])不能超过bucket_size大小
        实际ngx_hash_init处理的时候并不是直接用max_size个桶，而是从x=1到max_size去试，只要ngx_hash_init参数中的names[]数组数据能全部hash
        到这x个桶中，并且满足条件:每个桶中的元素(ngx_hash_elt_t)个数 * NGX_HASH_ELT_SIZE(&names[n])不超过bucket_size大小,则说明用x
        个桶就够用了，然后直接使用x个桶存储。 见ngx_hash_init
     */
    ngx_uint_t max_size; // 哈希桶个数的最大值
    //表示每个hash桶中(hash->buckets[i->成员[i]])对应的成员所有ngx_hash_elt_t成员暂用空间和的最大值，
    //   就是每个桶暂用的所有空间最大值，通过这个值计算需要多少个桶
    ngx_uint_t bucket_size; // 哈希桶的大小

    char *name;  // 哈希表的名字
    ngx_pool_t *pool;  // 使用的内存池
    ngx_pool_t *temp_pool;  // 估算时临时使用的内存池
} ngx_hash_init_t;

#define NGX_HASH_SMALL 1 //NGX_HASH_SMALL表示初始化元素较少
#define NGX_HASH_LARGE 2 //NGX_HASH_LARGE表示初始化元素较多

#define NGX_HASH_LARGE_ASIZE 16384
#define NGX_HASH_LARGE_HSIZE 10007

#define NGX_HASH_WILDCARD_KEY 1 //通配符类型
#define NGX_HASH_READONLY_KEY 2

//初始化hash需要的所有键值对
typedef struct {
    //散列中槽总数  如果是大hash桶方式，则hsize=NGX_HASH_LARGE_HSIZE,小hash桶方式，hsize=107
    ngx_uint_t hsize;

    ngx_pool_t *pool;
    ngx_pool_t *temp_pool;

    ngx_array_t keys; //存放不包含通配符的<key,value>键值对
    ngx_array_t *keys_hash; //用来检测冲突的

    ngx_array_t dns_wc_head; //存放包含前缀通配符的<key,value>键值对
    ngx_array_t *dns_wc_head_hash; //用来检测冲突的

    ngx_array_t dns_wc_tail; //存放包含后缀通配符的<key,value>键值对
    ngx_array_t *dns_wc_tail_hash; //用来检测冲突的
} ngx_hash_keys_arrays_t;

/*
显而易见，ngx_table_elt_t是为HTTP头部“量身订制”的，其中key存储头部名称（如Content-Length），value存储对应的值（如“1024”），
lowcase_key是为了忽略HTTP头部名称的大小写（例如，有些客户端发来的HTTP请求头部是content-length，Nginx希望它与大小写敏感的
Content-Length做相同处理，有了全小写的lowcase_key成员后就可以快速达成目的了），hash用于快速检索头部（它的用法在3.6.3节中进行详述）。
*/
typedef struct {
    ngx_uint_t hash; //等于ngx_http_request_s->header_hash ,这是通过key value字符串计算出的hash值
    ngx_str_t key;
    ngx_str_t value;
    u_char *lowcase_key; //存放的是本结构体中key的小写字母字符串
} ngx_table_elt_t;

//通过给定的key和name在hash表中查找对应的<name,value>键值对，并将查找到的value值返回，参数中的key是name通过hash计算出来的
void *ngx_hash_find(ngx_hash_t *hash, ngx_uint_t key, u_char *name, size_t len);

//根据name在前缀通配符hash中查找对应的value值。有了hash表后，查找是件相对更容易的事，从后往前的获取name的每个字段（根据.分割
void *ngx_hash_find_wc_head(ngx_hash_wildcard_t *hwc, u_char *name, size_t len);

//根据name在前缀通配符hash中查找对应的value值。有了hash表后，查找是件相对更容易的事，从前往后的获取name的每个字段（根据.分割
void *ngx_hash_find_wc_tail(ngx_hash_wildcard_t *hwc, u_char *name, size_t len);

//先完全匹配，完后前正则匹配，最后是后正则匹配
void *ngx_hash_find_combined(ngx_hash_combined_t *hash, ngx_uint_t key, u_char *name, size_t len);

ngx_int_t ngx_hash_init(ngx_hash_init_t *hinit, ngx_hash_key_t *names, ngx_uint_t );
#endif //NGX_HASH_NGX_HASH_H
