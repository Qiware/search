#include "mem_pool.h"
#include "xml_tree.h"

#define MEM_SIZE (1 * 1024 * 1024)

int main(int argc, char *argv[])
{
    xml_tree_t *xml;
    mem_pool_t *pool;
    xml_option_t option;

    memset(&option, 0, sizeof(option));

    plog_init(LOG_LEVEL_DEBUG, "test.log");

    pool = mem_pool_creat(MEM_SIZE);
    if (NULL == pool)
    {
        return -1;
    }

    option.pool = (void *)pool;
    option.alloc = (mem_alloc_cb_t)mem_pool_alloc;
    option.dealloc = (mem_dealloc_cb_t)mem_pool_dealloc;

    xml = xml_creat(argv[1], &option);
    if (NULL == xml)
    {
        fprintf(stdout, "Create XML failed!");
        return -1;
    }

    xml_fwrite(xml, "output.xml");

    xml_destroy(xml);
    return 0;
}
