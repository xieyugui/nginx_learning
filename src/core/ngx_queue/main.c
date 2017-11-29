#include <stdio.h>
#include <ngx_config.h>
#include <ngx_core.h>

typedef struct {
    int x;
    int y;
} my_point_t;

typedef struct {
    my_point_t point;
    ngx_queue_t queue;
} my_point_queue_t;


//sort from small to big
ngx_int_t my_point_cmp(const ngx_queue_t* lhs, const ngx_queue_t* rhs)
{
    my_point_queue_t *pt1 = ngx_queue_data(lhs, my_point_queue_t, queue);
    my_point_queue_t *pt2 = ngx_queue_data(rhs, my_point_queue_t, queue);

    if (pt1->point.x < pt2->point.x)
        return 0;
    else if (pt1->point.x > pt2->point.x)
        return 1;
    else if (pt1->point.y < pt2->point.y)
        return 0;
    else if (pt1->point.y > pt2->point.y)
        return 1;
    return 1;
}

int main() {
    ngx_pool_t *pool;
    ngx_queue_t *myque;
    my_point_queue_t *point;
    my_point_t points[6] = {
            {10, 1}, {20, 9}, {9, 9}, {90, 80}, {5, 3}, {50, 20}
    };
    int i;

    pool = ngx_create_pool(1024, NULL);
    myque = ngx_palloc(pool, sizeof(ngx_queue_t));
    ngx_queue_init(myque);

    for(i = 0; i < 6; i++) {
        point = (my_point_queue_t *) ngx_palloc(pool, sizeof(my_point_queue_t));
        point->point.x = points[i].x;
        point->point.y = points[i].y;
        ngx_queue_init(&point->queue);

        ngx_queue_insert_head(myque, &point->queue);
    }

    ngx_queue_sort(myque, my_point_cmp);

    ngx_destroy_pool(pool);


    return 0;
}