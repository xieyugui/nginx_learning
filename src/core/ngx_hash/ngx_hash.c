/*
* @author:    daemon.xie
* @license:   Apache Licence
* @contact:   xieyugui@migu.cn 
* @software:  CLion
* @file:      ngx_hash.c
* @date:      2017/12/28 下午6:09
* @desc:
*/

//
// Created by daemon.xie on 2017/12/28.
//

#include <ngx_config.h>
#include <ngx_core.h>

//通过给定的key和name在hash表中查找对应的<name,value>键值对，并将查找到的value值返回，参数中的key是name通过hash计算出来的。
// 这个函数的实现很简单，就是通过key找到要查找的键值对在哪个桶中，然后遍历这个桶中的每个元素找key等于name的元素
void *
ngx_hash_find(ngx_hash_t *hash, ngx_uint_t key, u_char *name, size_t len)
{
    ngx_uint_t i;
    ngx_hash_elt_t *elt;

    //由key找到所在的bucket(该bucket中保存其elts地址)
    elt = hash->buckets[key % hash->size];

    if (elt == NULL) {
        return NULL;
    }

    while (elt->value) {
        if (len != (size_t) elt->len) { //先判断长度
            goto next;
        }

        for (i = 0; i < len; i++) {
            if (name[i] != elt->name[i]) { //接着比较name的内容(此处按字符匹配)
                goto next;
            }
        }

        return elt->value; //匹配成功，直接返回该ngx_hash_elt_t结构的value字段

    next:
        //注意此处从elt->name[0]地址处向后偏移，故偏移只需加该elt的len即可，然后在以4字节对齐
        elt = (ngx_hash_elt_t *) ngx_align_ptr(&elt->name[0] + elt->len, sizeof(void *));

        continue;
    }

    return NULL;
}

//根据name在前缀通配符hash中查找对应的value值
void *
ngx_hash_find_wc_head(ngx_hash_wildcard_t *hwc, u_char *name, size_t len)
{
    void *value;
    ngx_uint_t i, n, key;

    n = len;

    //从后往前搜索第一个dot，则n 到 len-1 即为关键字中最后一个 子关键字
    while (n) { //name中最后面的字符串，如 AA.BB.CC.DD，则这里获取到的就是DD
        if (name[n - 1] == '.') {
            break;
        }

        n--;
    }

    key = 0;

    //n 到 len-1 即为关键字中最后一个 子关键字，计算其hash值
    for (i = n; i < len; i++) {
        key = ngx_hash(key, name[i]);
    }

    //调用普通查找找到关键字的value
    value = ngx_hash_find(&hwc->hash, key, &name[n], len - n);

    if (value) {

        //从其中可以看出. 末位为1往往表明不支持exact匹配.  为0相反
        //倒数第二位为1往往表明ngx_hash_elt_t结构体(即散列表元素)的value指向的是下一级散列表的地址. 为0相反
        //除此之外, 要考虑实现的优先级. 先考虑哪种情况呢？ 肯定是最复杂的情况. 就是 (11, 10)的情况, 不仅有下一级哈希, 还要考虑是否支持精确匹配

        /*
         * the 2 low bits of value have the special meaning:
         *     00 - value is data pointer for both "example.com"
         *          and "*.example.com";
         *     01 - value is data pointer for "*.example.com" only;
         *     10 - value is pointer to wildcard hash allowing
         *          both "example.com" and "*.example.com";
         *     11 - value is pointer to wildcard hash allowing
         *          "*.example.com" only.
         */
        //这里的判断是"和2与得出结果为1", 那么上面的4中情况就会有两种符合
        //即低两位为 "10"和"11"的情况. 根据概括的规则, 这两种情况的共同点是散列表元素的value指向的是下一级散列表的地址
        if((uintptr_t) value & 2) {

            if(n == 0) { //搜索到了最后一个子关键字且没有通配符，如"example.com"的example
                if ((uintptr_t) value & 1) {
                    return NULL;
                }

                hwc = (ngx_hash_wildcard_t *)
                        ((uintptr_t) value & (uintptr_t) ~3);

                return hwc->value;
            }

            hwc = (ngx_hash_wildcard_t *) ((uintptr_t) value & (uintptr_t) ~3);

            value = ngx_hash_find_wc_head(hwc, name, n - 1);

            if (value) {
                return value;
            }

            return hwc->value;
        }

        //如果运行到现在还没有结束, 那么还剩下 01, 00 的情况了, 这两种情况都没有下一级哈希的
        //上面概括到：末位为1往往表明不支持exact匹配
        if ((uintptr_t) value & 1) {

            if (n == 0) {

                /* "example.com" */

                return NULL;
            }

            return (void *) ((uintptr_t) value & (uintptr_t) ~3);
        }

        return value;
    }

    return hwc->value;
}

/*
 * ngx_hash_find_wc_tail与前置通配符查找差不多，这里value低两位仅有两种标志，更加简单：
 * 00 - value 是指向 用户自定义数据
 * 11 - value的指向下一个哈希表
 */
void *
ngx_hash_find_wc_tail(ngx_hash_wildcard_t *hwc, u_char *name, size_t len)
{
    void *value;
    ngx_uint_t i, key;

    key = 0;
    //从前往后搜索第一个dot，则0 到 i 即为关键字中第一个 子关键字
    for (i = 0; i < len; i++) {
        if (name[i] == '.') {
            break;
        }

        key = ngx_hash(key, name[i]);
    }

    if (i == len) {
        return NULL;
    }

    value = ngx_hash_find(&hwc->hash, key, name, i);

    if (value) {

        if((uintptr_t) value & 2) {
            i++;

            hwc = (ngx_hash_wildcard_t *) ((uintptr_t) value & (uintptr_t) ~3);

            value = ngx_hash_find_wc_tail(hwc, &name[i], len - i);

            if (value) {
                return value;
            }

            return hwc->value;
        }

        return value;
    }

    return hwc->value;
}

//一次从常规，前正则，后正则顺序查找
void *
ngx_hash_find_combined(ngx_hash_combined_t *hash, ngx_uint_t key, u_char *name, size_t len)
{
    void *value;

    if (hash->hash.buckets) {
        value = ngx_hash_find(&hash->hash, key, name, len);

        if (value) {
            return value;
        }
    }

    if (len == 0) {
        return NULL;
    }

    if (hash->wc_head && hash->wc_head->hash.buckets) {
        value = ngx_hash_find_wc_head(hash->wc_head, name, len);

        if (value) {
            return value;
        }
    }

    if (hash->wc_tail && hash->wc_tail->hash.buckets) {
        value = ngx_hash_find_wc_tail(hash->wc_tail, name, len);

        if (value) {
            return value;
        }
    }

    return NULL;
}

/*
NGX_HASH_ELT_SIZE宏用来计算ngx_hash_elt_t结构大小，定义如下。
在32位平台上，sizeof(void*)=4，(name)->key.len即是ngx_hash_elt_t结构中name数组保存的内容的长度，
 其中的"+2"是要加上该结构中len字段(u_short类型)的大小。

 该式后半部分即是(name)->key.len+2以4字节对齐的大小
*/
#define NGX_HASH_ELT_SIZE(name) \
    (sizeof(void *) + ngx_align((name)->key.len + 2, sizeof(void *)))

//names参数是ngx_hash_key_t结构的数组，即键-值对<key,value>数组，nelts表示该数组元素的个数
ngx_int_t
ngx_hash_init(ngx_hash_init_t *hinit, ngx_hash_key_t *names, ngx_uint_t nelts)
{
    u_char *elts;
    size_t len;
    u_short *test;
    ngx_uint_t i, n, key,
                size,   //实际需要桶的个数
                start, bucket_size;
    ngx_hash_elt_t *elt, **buckets;

    if (hinit->max_size == 0) {
        ngx_log_error(NGX_LOG_EMERG, hinit->pool->log, 0,
                      "could not build %s, you should "
                              "increase %s_max_size: %i",
                      hinit->name, hinit->name, hinit->max_size);
        return NGX_ERROR;
    }

    for (n = 0; n < nelts; n++) {
        //检查names数组的每一个元素，判断桶的大小是否够分配
        //names[n]成员空间一定要小于等于bucket_size /* 每个桶至少能存放一个元素 + 一个void指针
        //要加上sizeof(void *)，因为bucket最后需要k-v对结束标志，是void * value来做的。
        if (hinit->bucket_size < NGX_HASH_ELT_SIZE(&names[n]) + sizeof(void *))
        {
            //有任何一个元素，桶的大小不够为该元素分配空间，则退出
            ngx_log_error(NGX_LOG_EMERG, hinit->pool->log, 0,
                          "could not build the %s, you should "
                                  "increase %s_bucket_size: %i",
                          hinit->name, hinit->name, hinit->bucket_size);
            return NGX_ERROR;
        }

    }

    //分配2*max_size个字节的空间保存hash数据(该内存分配操作不在nginx的内存池中进行，因为test只是临时的)

    /* max_size是bucket的最大数量, 这里的test是用来做探测用的，探测的目标是在当前bucket的数量下，冲突发生的是否频繁。
     * 过于频繁则说明当前的bucket数量过少，需要调整。那么如何判定冲突过于频繁呢？就是利用这个test数组，它总共有max_size个
     * 元素，即最大的bucket。每个元素会累计落到该位置关键字长度，当大于256个字节，即u_short所表示的最大大小时，则判定
     * bucket过少，引起了严重的冲突。后面会看到具体的处理。
     */
    //test数组中存放每个bucket的当前容量，如果某一个key的容量大于了bucket size就意味着需要加大hash桶的个数了
    test = ngx_alloc(hinit->max_size * sizeof(u_short), hinit->pool->log);
    if (test == NULL) {
        return NGX_ERROR;
    }

    // 实际可用空间为定义的bucket_size减去末尾的void *(结尾标识)，末尾的void* 指向NULL
    bucket_size = hinit->bucket_size - sizeof(void *);
    //每个桶能放多少个元素(nelt)，从而由总元素推算出需要多少个桶
    /*
     * 这里考虑NGX_HASH_ELT_SIZE中，由于对齐的缘故，一个关键字最少需要占用两个指针的大小。
     * 在这个前提下，来估计所需要的bucket最小数量，即考虑元素越小，从而一个bucket容纳的数量就越多，
     * 自然使用的bucket的数量就越少，但最少也得有一个。
     */
    start = nelts / (bucket_size / (2 * sizeof(void *)));
    start = start ? start : 1;
    /*
     * 调整max_size，即bucket数量的最大值，依据是：bucket超过10000，且总的bucket数量与元素个数比值小于100
     * 那么bucket最大值减少1000，至于这几个判断值的由来，尚不清楚，经验值或者理论值。
     */
    if (hinit->max_size > 10000 && nelts && hinit->max_size / nelts < 100) {
        start = hinit->max_size - 1000;
    }

    //size从start开始，逐渐加大bucket的个数，直到恰好满足所有具有相同hash％size的元素都在同一个bucket，这样hash的size就能确定了
    for (size = start; size <= hint->max_size; size++) {

        //每次递归新的size的时候需要将旧test的数据清空
        ngx_memzero(test, size * sizeof(u_short));

        for (n = 0; n < nelts; n++) {
            //标记1：此块代码是检查bucket大小是否够分配hash数据
            if (names[n].key.data == NULL) {
                continue;
            }

            //计算key和names中所有name长度，并保存在test[key]中
            key = names[n].key_hash % size;
            //开始叠加每个bucket的size
            test[key] = (u_short) (test[key] + NGX_HASH_ELT_SIZE(&names[n]));

            //这里终于用到了bucket_size，大于这个值，则说明这个size不合适啊goto next，调整一下桶的数目
            //如果某个bucket的size超过了bucket_size，那么加大bucket的个数，使得元素分布更分散一些
            if (test[key] > (u_short) bucket_size) {
                goto next;
            }
        }
        goto found;

    next:

        continue;
    }

    size = hinit->max_size;

    //走到这里表面，在names中的元素入hash桶的时候，可能会造成某些hash桶的暂用空间会比实际的bucket_size大
    ngx_log_error(NGX_LOG_WARN, hinit->pool->log, 0,
                  "could not build optimal %s, you should increase "
                          "either %s_max_size: %i or %s_bucket_size: %i; "
                          "ignoring %s_bucket_size",
                  hinit->name, hinit->name, hinit->max_size,
                  hinit->name, hinit->bucket_size, hinit->name);
//找到合适的bucket
found:
    //到这里后把所有的test[i]数组赋值为4，预留给NULL指针
    for (i = 0; i < size; i++) {
        //将test数组前size个元素初始化为4，提前赋值4的原因是，hash桶的成员列表尾部会有一个NULL，提前把这4字节空间预留
        test[i] = sizeof(void *);
    }

    for (n = 0; n < nelts; n++) {
        if (name[n].key.data == NULL) {
            continue;
        }
        //计算key和names中所有name长度，并保存在test[key]中
        key = names[n].key_hash % size;
        test[key] = (u_short) (test[key] + NGX_HASH_ELT_SIZE(&names[n]));
    }

    len = 0;

    //计算总空间
    for (i = 0; i < size; i++) {
        if (test[i] == sizeof(void *)) {
            continue;
        }
        //对test[i]按ngx_cacheline_size对齐(32位平台，ngx_cacheline_size=32)
        test[i] = (u_short) (ngx_align(test[i], ngx_cacheline_size));

        len += test[i];
    }

    /*
     * 向内存池申请bucket元素所占的内存空间，
     * 注意：若前面没有申请hash表头结构，则在这里将和ngx_hash_wildcard_t一起申请
     */
    if (hinit->hash == NULL) {
        //在内存池中分配hash头及buckets数组(size个ngx_hash_elt_t*结构  桶的个数)
        //值得注意的是, 这里申请的并不是单纯的基本哈希表结构的内存, 而是包含基本哈希表的通配符哈希表
        hinit->hash = ngx_pcalloc(hinit->pool, sizeof(ngx_hash_wildcard_t)
                                                + size * sizeof(ngx_hash_elt_t *));
        if (hinit->hash == NULL) {
            ngx_free(test);
            return NGX_ERROR;
        }

        buckets = (ngx_hash_elt_t **) ((u_char *) hinit->hash + sizeof(ngx_hash_wildcard_t));
    } else {
        buckets = ngx_pcalloc(hinit->pool, size * sizeof(ngx_hash_elt_t *));
        if (buckets == NULL) {
            ngx_free(test);
            return NGX_ERROR;
        }
    }

    //接着分配elts
    elts = ngx_palloc(hinit->pool, len + ngx_cacheline_size);
    if (elts == NULL) {
        ngx_free(test);
        return NGX_ERROR;
    }

    //将elts地址按ngx_cacheline_size=32对齐
    elts = ngx_align_ptr(elts, ngx_cacheline_size);

    for (i = 0; i < size; i++) {
        if (test[i] == sizeof(void *)) {
            continue;
        }

        //每个桶头指针buckets[i]指向自己桶中的成员首地址
        buckets[i] = (ngx_hash_elt_t *) elts;
        elts += test[i];

    }

    for (i = 0; i < size; i++) {
        test[i] = 0;
    }

    //把所有的name数据入队hash表中
    for (n = 0; n < nelts; n++) {
        if (names[n].key.data == NULL) {
            continue;
        }
        //计算key，即将被hash的数据在第几个bucket，并计算其对应的elts位置，也就是在该buckets[i]桶中的具体位置
        key = names[n].key_hash % size;
        elt = (ngx_hash_elt_t *) ((u_char *) buckets[key] + test[key]);

        //对ngx_hash_elt_t结构赋值
        elt->value = names[n].value;
        elt->len = (u_short) names[n].key.len;
        ngx_strlow(elt->name, names[n].key.data, names[n].key.len);

        //计算下一个要被hash的数据的长度偏移，下一次就从该桶的下一个位置存储
        test[key] = (u_short) (test[key] + NGX_HASH_ELT_SIZE(&names[n]));
    }


    //为每个桶的成员列表最尾部添加一个ngx_hash_elt_t成员，起value=NULL，标识这是该桶中的最后一个ngx_hash_elt_t
    for (i = 0; i < size; i++) {
        if (buckets[i] == NULL) {
            continue;
        }

        //test[i]相当于所有被hash的数据总长度
        elt = (ngx_hash_elt_t *) ((u_char *) buckets[i] + test[i]);

        elt->value = NULL;
    }

    ngx_free(test);//释放该临时空间

    hinit->hash->buckets = buckets;
    hinit->hash->size = size;

    return NGX_OK;
}

/*
这里的传入参数names数组是经过处理后的字符串数组。对于字符串数组"*.test.yexin.com"、".yexin.com"、"*.example.com"、"*.example.org"、
 ".yexin.org"处理完成之后就变成了 "com.yexin.test."、"com.yexin"、"com.example."、"org.example."、"org.yexin"
遍历数组中元素找出当前字符串的第一个key字段并放入curr_names数组把所有字符串"com.yexin.test."、"com.yexin"、"com.example."、
 "org.example."、"org.yexin"的第一个key字段提取出来，那么只有两个不相同的字段“com”、"org"

 可以参考http://blog.csdn.net/a987073381/article/details/52357990

 对于“*.abc.com”将会构造出2个hash表，第一个hash表中有一个key为com的表项，该表项的value包含有指向第二个hash表的指针，
 而第二个hash表中有一个表项abc，该表项的value包含有指向*.abc.com对应的value的指针。那么查询的时候，比如查询www.abc.com的时候，
 先查com，通过查com可以找到第二级的hash表，在第二级hash表中，再查找abc，依次类推，直到在某一级的hash表中查到的表项对应的value对应
 一个真正的值而非一个指向下一级hash表的指针的时候，查询过程结束
 */
ngx_int_t
ngx_hash_wildcard_init(ngx_hash_init_t *hinit, ngx_hash_key_t *names, ngx_uint_t nelts)
{
    size_t len,dot_len;
    ngx_uint_t i, n, dot;
    ngx_array_t curr_names, next_names;
    ngx_hash_key_t *name, *next_name;
    ngx_hash_init_t h;
    ngx_hash_wildcard_t *wdc;

    //初始化临时动态数组curr_names,curr_names是存放当前关键字的数组
    if (ngx_array_init(&curr_names, hinit->temp_pool, nelts, sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    //初始化临时动态数组next_names,next_names是存放关键字去掉后剩余关键字
    if (ngx_array_init(&next_names, hinit->temp_pool, nelts,
                       sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    for (n = 0; n < nelts; n = i) {
        dot = 0;

        for (len = 0; len < names[n].key.len; len++) {//查找 dot，len的长度为.前面的字符串长度
            if (names[n].key.data[len] == '.') {
                dot = 1;
                break;
            }
        }

        name = ngx_array_push(&curr_names);
        if (name == NULL) {
            return NGX_ERROR;
        }

        name->key.len = len;
        name->key.data = names[n].key.data;
        name->key_hash = hinit->key(name->key.data, name->key.len);
        name->value = names[n].value; //如果有子hash，则value会在后面指向子hash

        //从子dot开始，比如com.xie ,此时len 为.  ++ 之后就从x开始
        dot_len = len + 1;

        if (dot) {
            len++;
        }

        //每次清零
        next_names.nelts = 0;

        //如果names[n] dot后还有剩余关键字，将剩余关键字放入next_names中
        if (names[n].key.len != len) {
            next_name = ngx_array_push(&next_names);
            if (next_name == NULL) {
                return NGX_ERROR;
            }

            //com.xie 把xie 存入next_name
            next_name->key.len = names[n].key.len - len;
            next_name->key.data = names[n].key.data + len;
            next_name->key_hash = 0;
            next_name->value = names[n].value;
        }

        //如果上面搜索到的关键字没有dot，从n+1遍历names，将关键字比它长的全部放入next_name
        //搜索后面有没有和当前元素相同key字段的元素
        for (i = n + 1; i < nelts; i++) {
            //前len个关键字相同
            if (ngx_strncmp(names[n].key.data, names[i].key.data, len) != 0) {
                break;
            }

            if (!dot && names[i].key.len > len && names[i].key.data[len] != '.')
            {
                break;
            }

            //如果有则把除去当前key字段的剩余字符串装入该数组
            next_name = ngx_array_push(&next_names);
            if (next_name == NULL) {
                return NGX_ERROR;
            }


            next_name->key.len = names[i].key.len - dot_len;
            next_name->key.data = names[i].key.data + dot_len;
            next_name->key_hash = 0;
            next_name->value = names[i].value;
        }

        //如果next_names数组中有元素，递归处理该元素
        if (next_names.nelts) {

            h = *hinit;
            h.hash = NULL;


            if (ngx_hash_wildcard_init(&h, (ngx_hash_key_t *) next_names.elts,
                                       next_names.nelts)//递归，创建一个新的哈希表
                != NGX_OK)
            {
                return NGX_ERROR;
            }

            wdc = (ngx_hash_wildcard_t *) h.hash;

            //将用户value值放入新的hash表，也就是hinit中
            if (names[n].key.len == len) {
                wdc->value = names[n].value;
            }

            //并将当前value值指向新的hash表
            //将后缀组成的下一级hash地址作为当前字段的value保存下来
            name->value = (void *) ((uintptr_t) wdc | (dot ? 3 : 2)); //2只有在后缀通配符的情况下才会出现
        } else if (dot) {//表示后面已经没有子hash了,value指向具体的key-value中的value字符串
            //只有一个，而且不是后缀通配符
            name->value = (void *) ((uintptr_t) name->value | 1);
        }

    }

    //将最外层hash初始化
    if (ngx_hash_init(hinit, (ngx_hash_key_t *) curr_names.elts,
                      curr_names.nelts)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    return NGX_OK;
}

//对数据字符串data计算出key值
ngx_uint_t
ngx_hash_key(u_char *data, size_t len)
{
    ngx_uint_t i, key;

    key = 0;

    for (i = 0; i < len; i++) {
        key = ngx_hash(key, data[i]);
    }

    return key;
}

ngx_uint_t
ngx_hash_key_lc(u_char *data, size_t len)
{
    ngx_uint_t  i, key;

    key = 0;

    for (i = 0; i < len; i++) {
        key = ngx_hash(key, ngx_tolower(data[i]));
    }

    return key;
}

ngx_uint_t
ngx_hash_strlow(u_char *dst, u_char *src, size_t n)
{
    ngx_uint_t key;

    key = 0;

    while (n--) {
        *dst = ngx_tolower(*src);
        key = ngx_hash(key, *dst);
        dst++;
        src++;
    }

    return key;
}

ngx_int_t
ngx_hash_keys_array_init(ngx_hash_keys_arrays_t *ha, ngx_uint_t type)
{
    ngx_uint_t asize;

    if(type == NGX_HASH_SMALL) {
        asize = 4;
        ha->hsize = 107;
    } else {
        asize = NGX_HASH_LARGE_ASIZE;
        ha->hsize = NGX_HASH_LARGE_ASIZE;
    }

    //初始化 存放非通配符关键字的数组
    if(ngx_array_init(&ha->keys, ha->temp_pool, asize, sizeof(ngx_hash_key_t))
            != NGX_OK)
    {
        return NGX_ERROR;
    }

    //初始化存放前置通配符处理好的关键字 数组
    if (ngx_array_init(&ha->dns_wc_head, ha->temp_pool, asize, sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    //初始化存放后置通配符处理好的关键字 数组
    if (ngx_array_init(&ha->dns_wc_tail, ha->temp_pool, asize, sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    ha->keys_hash = ngx_pcalloc(ha->temp_pool, sizeof(ngx_array_t) * ha->hsize);
    //只开辟有多少个桶对应的头，每个桶中用来存储数据的空间在后面的ngx_hash_add_key分片空间
    if (ha->keys_hash == NULL) {
        return NGX_ERROR;
    }

    // 该数组在调用的过程中用来保存和检测是否有冲突的前向通配符的key值，也就是是否有重复。
    ha->dns_wc_head_hash = ngx_pcalloc(ha->temp_pool,
                                       sizeof(ngx_array_t) * ha->hsize);
    if (ha->dns_wc_head_hash == NULL) {
        return NGX_ERROR;
    }

    // 该数组在调用的过程中用来保存和检测是否有冲突的后向通配符的key值，也就是是否有重复。
    ha->dns_wc_tail_hash = ngx_pcalloc(ha->temp_pool,
                                       sizeof(ngx_array_t) * ha->hsize);
    if (ha->dns_wc_tail_hash == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}

ngx_int_t
ngx_hash_add_key(ngx_hash_keys_arrays_t *ha, ngx_str_t *key, void *value, ngx_uint_t flags)
{
    size_t len;
    u_char *p;
    ngx_str_t *name;
    ngx_uint_t i, k, n,
        skip,
        last;
    ngx_array_t *keys, *hwc;
    ngx_hash_key_t *hk;

    last = key->len;

    if (flags & NGX_HASH_WILDCARD_KEY) {
        n = 0;

        for (i = 0; i < key->len; i++) {

            if(key->data[i] == '*') {
                if (++n > 1) { //通配符*只能出现一次，出现多次说明错误返回
                    return NGX_DECLINED;
                }
            }

            //不能出现两个连续的..，便是出错，直接返回
            if (key->data[i] == '.' && key->data[i + 1] == '.') {
                return NGX_DECLINED;
            }
        }

        //首字符是.，".example.com"说明是前向通配符
        if (key->len > 1 && key->data[0] == '.') {
            skip = 1;
            goto wildcard;
        }

        if (key->len > 2) {

            if (key->data[0] == '*' && key->data[1] == '.') {//"*.example.com"
                skip = 2;
                goto wildcard;
            }

            if (key->data[i - 2] == '.' && key->data[i - 1] == '*') {//"www.example.*"
                skip = 0;
                last -= 2;
                goto wildcard;
            }
        }

        if (n) {
            return NGX_DECLINED;
        }
    }

    /* exact hash */
    /* 说明是精确匹配server_name类型，例如"www.example.com" */
    k = 0;

    //把字符串key为源来计算hash，一个字符一个字符的算
    for (i = 0; i < last; i++) {
        if(!(flags & NGX_HASH_READONLY_KEY)) {
            key->data[i] = ngx_tolower(key->data[i]);
        }
        k = ngx_hash(k, key->data[i]);
    }

    k %= ha->hsize;

    /* check conflicts in exact hash */

    name = ha->keys_hash[k].elts;

    if (name) {
        for (i = 0; i < ha->keys_hash[k].nelts; i++) {
            if (last != name[i].len) {
                continue;
            }

            if (ngx_strncmp(key->data, name[i].data, last) == 0) { //已经存在一个相同的
                return NGX_BUSY;
            }
        }
    } else {
        //每个桶中的元素个数默认
        //桶的头部指针在ngx_hash_keys_array_init分配，桶中存储数据的空间在这里分配
        if (ngx_array_init(&ha->keys_hash[k], ha->temp_pool, 4,
                           sizeof(ngx_str_t))
            != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

    //存放key到ha->keys_hash[]桶中
    name = ngx_array_push(&ha->keys_hash[k]);
    if (name == NULL) {
        return NGX_ERROR;
    }

    *name = *key;

    hk = ngx_array_push(&ha->keys);
    if (hk == NULL) {
        return NGX_ERROR;
    }
    //ha->keys中存放的是key  value对
    hk->key = *key;
    hk->key_hash = ngx_hash_key(key->data, last);
    hk->value = value;

    return NGX_OK;

wildcard:
    /* wildcard hash */
    //以参数中的key字符串计算hash key
    k = ngx_hash_strlow(&key->data[skip], &key->data[skip], last - skip);

    k %= ha->hsize;

    //".example.com"，".example.com"除了添加到hash桶keys_hash[]外，还会添加到dns_wc_tail_hash[]桶中
    if (skip == 1) {

        /* check conflicts in exact hash for ".example.com" */

        name = ha->keys_hash[k].elts;

        if (name) {
            len = last - skip; //除去开头的.
            //字符串长度和内容完全匹配
            for (i = 0; i < ha->keys_hash[k].nelts; i++) {
                if (len != name[i].len) {
                    continue;
                }

                if (ngx_strncmp(&key->data[1], name[i].data, len) == 0) {
                    return NGX_BUSY;
                }
            }
        } else {
            if (ngx_array_init(&ha->keys_hash[k], ha->temp_pool, 4,
                               sizeof(ngx_str_t)) //每个槽中的元素个数默认4字节
                //开辟每个槽中的空间，用来存放对应的节点到该槽中，槽的头节点在ngx_hash_keys_array_init中已经分配好
                != NGX_OK)
            {
                return NGX_ERROR;
            }
        }

        name = ngx_array_push(&ha->keys_hash[k]);
        if (name == NULL) {
            return NGX_ERROR;
        }

        name->len = last - 1; //把".example.com"字符串开头的.去掉
        name->data = ngx_pnalloc(ha->temp_pool, name->len);
        if (name->data == NULL) {
            return NGX_ERROR;
        }

        //".example.com"去掉开头的.后变为"example.com"存储到name中，但是key还是原来的".example.com"
        //".example.com"去掉开头的.后变为"example.com"存储到ha->keys_hash[i]桶中
        ngx_memcpy(name->data, &key->data[1], name->len);
    }

    //前置匹配的通配符"*.example.com"  ".example.com"
    if (skip) {
        /*
         * convert "*.example.com" to "com.example.\0"
         *      and ".example.com" to "com.example\0"
         */
        p = ngx_pnalloc(ha->temp_pool, last);
        if (p == NULL) {
            return NGX_ERROR;
        }

        len = 0;
        n = 0;

        for (i = last - 1; i; i--) {
            if (key->data[i] == '.') {
                ngx_memcpy(&p[n], &key->data[i + 1], len);
                n += len;
                p[n++] = '.';
                len = 0;
                continue;
            }

            len++;
        }

        if (len) {
            ngx_memcpy(&p[n], &key->data[1], len);
            n += len;
        }

        /* key中数据"*.example.com"，p中数据"com.example.\0"   key中数据".example.com" p中数据"com.example\0" */
        p[n] = '\0';

        hwc = &ha->dns_wc_head;
        keys = &ha->dns_wc_head_hash[k];

    } else {//后置匹配
        last++; //+1是用来存储\0字符

        p = ngx_pnalloc(ha->temp_pool, last);
        if (p == NULL) {
            return NGX_ERROR;
        }

        //key中数据为"www.example.*"， p中数据为"www.example\0"
        ngx_cpystrn(p, key->data, last);

        hwc = &ha->dns_wc_tail;
        keys = &ha->dns_wc_tail_hash[k];
    }

    /* check conflicts in wildcard hash */
    name = keys->elts;

    if (name) {
        len = last - skip;
        //查看是否已经有存在的了
        for (i = 0; i < keys->nelts; i++) {
            if (len != name[i].len) {
                continue;
            }

            if (ngx_strncmp(key->data + skip, name[i].data, len) == 0) {
                return NGX_BUSY;
            }
        }

    } else {//说明是第一次出现前置通配符或者后置通配符
        //初始化桶ha->dns_wc_head_hash[i]或者桶ha->dns_wc_tail_hash[i]中的元素个数
        if (ngx_array_init(keys, ha->temp_pool, 4, sizeof(ngx_str_t)) != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

    name = ngx_array_push(keys);
    if (name == NULL) {
        return NGX_ERROR;
    }

    name->len = last - skip;
    name->data = ngx_pnalloc(ha->temp_pool, name->len);
    if (name->data == NULL) {
        return NGX_ERROR;
    }

    /* 前置匹配key字符串存放到&ha->dns_wc_head; 后置匹配key字符串存放到&ha->dns_wc_tail hash表中 */
    ngx_memcpy(name->data, key->data + skip, name->len);

    /* add to wildcard hash */

    hk = ngx_array_push(hwc);
    if (hk == NULL) {
        return NGX_ERROR;
    }

    hk->key.len = last - 1;
    //到这里,p中的数据就有源key"*.example.com", ".example.com", and "www.example.*"变为了"com.example.\0" "com.example\0"  "www.example\0"
    hk->key.data = p;
    hk->key_hash = 0;
    hk->value = value; //以ngx_http_add_referer为例，假设key为，*.example.com/test/xxx,则value为字符串/test/xxx，否则为NGX_HTTP_REFERER_NO_URI_PART

    return NGX_OK;
}