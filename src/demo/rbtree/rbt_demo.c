/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: rbt_test.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # 2016年01月08日 星期五 09时02分24秒 #
 ******************************************************************************/
#include "comm.h"
#include "rb_tree.h"

#define SID_NUM     (10)

typedef struct
{
    uint64_t id;
} rbt_data_t;

void rbt_trav_print_hdl(rbt_data_t *data, void *args)
{
    static uint32_t idx = 0;

    fprintf(stderr, "idx:%u id:%lu\n", ++idx, data->id);
}

static rtb_data_cmp_cb(const rbt_data_t *data1, const rbt_data_t *data2)
{
    return (data1->id - data2->id);
}

int main(void)
{
    int idx, n;
    rbt_tree_t *rbt;
    rbt_data_t *data, key;

    rbt = rbt_creat(NULL, (cmp_cb_t)rtb_data_cmp_cb);
    if (NULL == rbt) {
        return -1;
    }

    for (n=0; n<2; n++) {
        for (idx=0; idx<SID_NUM; ++idx) {
            data = (rbt_data_t *)calloc(1, sizeof(rbt_data_t));
            data->id = idx;
            rbt_insert(rbt, (void *)data);
        }

        for (idx=0; idx<SID_NUM; ++idx) {
            //rbt_delete(rbt, (void *)&idx, sizeof(idx), (void **)&data);
            //free(data);
            key.id = idx;
            data = rbt_query(rbt, &key);
            fprintf(stderr, "idx:%u:%lu data:%p\n", idx, data->id, data);
        }
    }

    rbt_trav(rbt, (trav_cb_t)rbt_trav_print_hdl, NULL);
    rbt_print(rbt);

    key.id = 3;
    data = rbt_query(rbt, &key);
    fprintf(stderr, "data:%p\n", data);

    return 0;
}
