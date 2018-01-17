#include <stdio.h>
#include <ngx_config.h>
#include <ngx_core.h>

#define Max_Num 7
#define Max_Size 1024
#define Bucket_Size 64

#define NGX_HASH_ELT_SIZE(name)               \
    (sizeof(void *) + ngx_align((name)->key.len + 2, sizeof(void *)))

/* for hash test */
static ngx_str_t urls[Max_Num] = {
        ngx_string("www.baidu.com"),  //220.181.111.147
        ngx_string("www.sina.com.cn"),  //58.63.236.35
        ngx_string("www.google.com"),  //74.125.71.105
        ngx_string("www.qq.com"),  //60.28.14.190
        ngx_string("www.163.com"),  //123.103.14.237
        ngx_string("www.sohu.com"),  //219.234.82.50
        ngx_string("www.abo321.org")  //117.40.196.26
};

static char* values[Max_Num] = {
        "220.181.111.147",
        "58.63.236.35",
        "74.125.71.105",
        "60.28.14.190",
        "123.103.14.237",
        "219.234.82.50",
        "117.40.196.26"
};

#define Max_Url_Len 15
#define Max_Ip_Len 15

#define Max_Num2 2

/* for finding test */
static ngx_str_t urls2[Max_Num2] = {
        ngx_string("www.china.com"),  //60.217.58.79
        ngx_string("www.csdn.net")  //117.79.157.242
};

ngx_array_t* add_urls_to_array(ngx_pool_t *pool)
{
    int loop;
    char prefix[] = "          ";
    ngx_array_t *a = ngx_array_create(pool, Max_Num, sizeof(ngx_hash_key_t));

    for (loop = 0; loop < Max_Num; loop++)
    {
        ngx_hash_key_t *hashkey = (ngx_hash_key_t*)ngx_array_push(a);
        hashkey->key = urls[loop];
        hashkey->key_hash = ngx_hash_key_lc(urls[loop].data, urls[loop].len);
        hashkey->value = (void*)values[loop];
        /** for debug
        printf("{key = (\"%s\"%.*s, %d), key_hash = %-10ld, value = \"%s\"%.*s}, added to array\n",
            hashkey->key.data, Max_Url_Len - hashkey->key.len, prefix, hashkey->key.len,
            hashkey->key_hash, hashkey->value, Max_Ip_Len - strlen(hashkey->value), prefix);
        */
    }

    return a;
}

ngx_hash_t* init_hash(ngx_pool_t *pool, ngx_array_t *array)
{
    ngx_int_t result;
    ngx_hash_init_t hinit;

    ngx_cacheline_size = 32;  //here this variable for nginx must be defined
    hinit.hash = NULL;  //if hinit.hash is NULL, it will alloc memory for it in ngx_hash_init
    hinit.key = &ngx_hash_key_lc;  //hash function
    hinit.max_size = Max_Size;
    hinit.bucket_size = Bucket_Size;
    hinit.name = "my_hash_sample";
    hinit.pool = pool;  //the hash table exists in the memory pool
    hinit.temp_pool = NULL;

    result = ngx_hash_init(&hinit, (ngx_hash_key_t*)array->elts, array->nelts);
    if (result != NGX_OK)
        return NULL;

    return hinit.hash;
}

void find_test(ngx_hash_t *hash, ngx_str_t addr[], int num)
{
    ngx_uint_t key;
    int loop;
    char prefix[] = "          ";

    for (loop = 0; loop < num; loop++)
    {
        key = ngx_hash_key_lc(addr[loop].data, addr[loop].len);
        void *value = ngx_hash_find(hash, key, addr[loop].data, addr[loop].len);
        if (value)
        {
            printf("(url = \"%s\"%.*s, key = %-10ld) found, (ip = \"%s\")\n",
                   addr[loop].data, Max_Url_Len - addr[loop].len, prefix, key, (char*)value);
        }
        else
        {
            printf("(url = \"%s\"%.*s, key = %-10d) not found!\n",
                   addr[loop].data, Max_Url_Len - addr[loop].len, prefix, key);
        }
    }
}

int main() {
    ngx_pool_t *pool = NULL;
    ngx_array_t *array = NULL;
    ngx_hash_t *hash;

    pool = ngx_create_pool(1024, NULL);
    array = add_urls_to_array(pool);

    hash = init_hash(pool, array);
    if (hash == NULL)
    {
        printf("Failed to initialize hash!\n");
        return -1;
    }

    find_test(hash, urls, Max_Num);
    printf("\n");

    find_test(hash, urls2, Max_Num2);

    //release
    ngx_array_destroy(array);
    ngx_destroy_pool(pool);

    return 0;

    return 0;
}