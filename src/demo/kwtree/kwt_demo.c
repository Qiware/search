#include "kw_tree.h"
#include "xml_tree.h"

#define DATA_LEN (10)

static int proto_load_conf(kwt_tree_t *kwt, const char *path, log_cycle_t *log);

/* 初始化日志模块 */
log_cycle_t *demo_init_log(const char *_path)
{
    int level;
    log_cycle_t *log;
    char path[FILE_PATH_MAX_LEN];

    level = log_get_level("debug");

    snprintf(path, sizeof(path), "%s.log", _path);

    log = log_init(level, path);
    if (NULL == log) {
        fprintf(stderr, "Init log failed! level:%d", level);
        return NULL;
    }

    return log;
}

int main(int argc, char *argv[])
{
    kwt_opt_t opt;
    kwt_tree_t *kwt;
    log_cycle_t *log;

    log = demo_init_log(argv[0]);
    if (NULL == log) {
        fprintf(stderr, "Initialize log failed!\n");
        return -1;
    }

    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    kwt = kwt_creat(&opt);
    if (NULL == kwt) {
        fprintf(stderr, "Create keyword-tree failed!");
        return -1;
    }

    return proto_load_conf(kwt, argv[1], log);
}

static int proto_load_conf(kwt_tree_t *kwt, const char *path, log_cycle_t *log)
{
    int count = 0, oct;
    xml_opt_t opt;
    xml_tree_t *xml;
    char hex[1024];
    xml_node_t *protocol, *words, *word, *key, *oct_node;

    opt.log = log;
    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    xml = xml_creat(path, &opt);
    if (NULL == xml) {
        return -1;
    }

    protocol = xml_query(xml, ".orphic.protocols.protocol");
    for (; NULL != protocol; protocol = protocol->next) {
        words = xml_search(xml, protocol, "key-words.words");
        for (; NULL != words; words = words->next) {
            word = xml_search(xml, words, "word");
            for (; NULL != word; word = word->next) {
                key = xml_search(xml, word, "value");
                if (NULL == key) {
                    assert(0);
                }

                oct_node = xml_search(xml, word, "octet");
                if (NULL == oct_node) {
                    oct = 0;
                }
                else {
                    oct = atoi(oct_node->value.str);
                }
                
                ++count;
                if (0 == oct) {
                    if (kwt_insert(kwt, (u_char *)key->value.str, key->value.len, (void *)1)) {
                        assert(0);
                    }
                }
                else {
                    str_to_hex(key->value.str, key->value.len, hex);
                    if (kwt_insert(kwt, (u_char *)hex, key->value.len/2, (void *)1)) {
                        assert(0);
                    }
                }
            }
        }
    }

    xml_destroy(xml);

    fprintf(stderr, "count: %d", count);
    kwt_print(kwt);
    pause();

    return 0;
}

int kwt_test(void)
{
    int i, ret;
    kwt_opt_t opt;
    kwt_tree_t *kwt;
    char *str[DATA_LEN], *str2;
    char input[1024];

    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

 
    kwt = kwt_creat(&opt);
    if (NULL == kwt) {
        fprintf(stderr, "Create keyword-tree failed!");
        return -1;
    }

    for (i=0; i<DATA_LEN; ++i) {
        str[i] = (char *)calloc(1, 128);
        if (NULL == str[i]) {
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

    while (1) {
        scanf(" %s", input);

        ret = kwt_query(kwt,(unsigned char *)input, strlen(input), (void **)&str2);
        fprintf(stderr, "ret:%d input:%s str2:%p\n", ret, input, str2);
    }

    kwt_destroy(kwt, NULL, mem_dealloc);

    return 0;
}
