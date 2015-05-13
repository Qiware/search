#include "kw_tree.h"

#define DATA_LEN (10)

int main(int argc, char *argv[])
{
    int i, ret;
    kwt_tree_t *kwt;
    char *str[DATA_LEN], *str2;
    char input[1024];

    kwt = kwt_creat();
    if (NULL == kwt)
    {
        fprintf(stderr, "Create keyword-tree failed!");
        return -1;
    }

    for (i=0; i<DATA_LEN; ++i)
    {
        str[i] = (char *)calloc(1, 128);
        if (NULL == str[i])
        {
            return -1;
        }
    }

    kwt_insert(kwt, (unsigned char *)"ABC", strlen("ABC"), str[0]);
    fprintf(stderr, "ABC: %p\n", str[0]);
    kwt_insert(kwt, (unsigned char *)"ABD", strlen("ABD"), str[1]);
    fprintf(stderr, "ABD: %p\n", str[1]);
    kwt_insert(kwt, (unsigned char *)"ABDEFG", strlen("ABDEFG"), str[2]);
    fprintf(stderr, "ABDEFG: %p\n", str[2]);
    kwt_insert(kwt, (unsigned char *)"ABDEFGHI", strlen("ABDEFGHI"), str[3]);
    fprintf(stderr, "ABDEFGHI: %p\n", str[3]);

    while (1)
    {
        scanf(" %s", input);

        ret = kwt_query(kwt,(unsigned char *)input, strlen(input), (void **)&str2);
        fprintf(stderr, "ret:%d input:%s str2:%p\n", ret, input, str2);
    }

    kwt_destroy(kwt, NULL, mem_dealloc);

    return 0;
}
