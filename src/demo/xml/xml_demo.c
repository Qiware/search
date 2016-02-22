#include "mem_pool.h"
#include "xml_tree.h"

#define MEM_SIZE (1 * 1024 * 1024)

int main(int argc, char *argv[])
{
    xml_opt_t opt;
    xml_tree_t *xml;
    mem_pool_t *pool;
    log_cycle_t *log;

    memset(&opt, 0, sizeof(opt));

    log = log_init(LOG_LEVEL_DEBUG, "test.log");
    if (NULL == log) {
        return -1;
    }

    pool = mem_pool_creat(MEM_SIZE);
    if (NULL == pool) {
        return -1;
    }

    opt.log = log;
    opt.pool = (void *)pool;
    opt.alloc = (mem_alloc_cb_t)mem_pool_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_pool_dealloc;

    xml = xml_creat(argv[1], &opt);
    if (NULL == xml) {
        fprintf(stdout, "Create XML failed!");
        return -1;
    }

    xml_fwrite(xml, "output.xml");

    xml_destroy(xml);
    return 0;
}
