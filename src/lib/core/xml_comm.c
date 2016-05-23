/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: xml_comm.c
 ** 版本号: 1.0
 ** 描  述: XML格式的解析
 **        解析XML格式的文件、字串: 在此主要使用的是压栈的思路，其效率比使用递
 **        归算法的处理思路效率更高.对比数据如下:
 **        1. 创建XML: 高5%~10%
 **        2. 查找效率: 高30%左右
 **        3. 释放效率: 高20%左右
 ** 作  者: # Qifeng.zou # 2013.02.18 #
 ** 修  改:
 **     1. # 增加转义字符的处理 # Qifeng.zou # 2014.01.06 #
 ******************************************************************************/
#include "log.h"
#include "comm.h"
#include "xml_tree.h"
#include "xml_comm.h"

static int xml_parse_version(xml_tree_t *xml, xml_parse_t *parse);
static int xml_parse_note(xml_tree_t *xml, xml_parse_t *parse);
static int xml_parse_mark(xml_tree_t *xml, Stack_t *stack, xml_parse_t *parse);
static int xml_parse_end(xml_tree_t *xml, Stack_t *stack, xml_parse_t *parse);

static int xml_mark_get_name(xml_tree_t *xml, Stack_t *stack, xml_parse_t *parse);
static int xml_mark_has_attr(xml_parse_t *parse);
static int xml_mark_get_attr(xml_tree_t *xml, Stack_t *stack, xml_parse_t *parse);
static int xml_mark_is_end(xml_parse_t *parse);
static int xml_mark_has_value(xml_parse_t *parse);
static int xml_mark_get_value(xml_tree_t *xml, Stack_t *stack, xml_parse_t *parse);

#if defined(__XML_ESC_PARSE__)
static const xml_esc_t *xml_esc_get(const char *str);
static int xml_esc_free(xml_esc_split_t *split);
static int xml_esc_size(const xml_esc_split_t *split);
static int xml_esc_merge(const xml_esc_split_t *sp, char *dst);
static int xml_esc_split(
        xml_tree_t *xml,
        const xml_esc_t *esc,
        const char *str, int len,
        xml_esc_split_t *split);

/* 转义字串对应的长度: 必须与xml_esc_e的顺序一致 */
static const xml_esc_t g_xml_esc_str[] =
{
    {XML_ESC_LT,     XML_ESC_LT_STR,    '<',  XML_ESC_LT_LEN}   /* &lt; */
    , {XML_ESC_GT,   XML_ESC_GT_STR,    '>',  XML_ESC_GT_LEN}   /* &gt; */
    , {XML_ESC_AMP,  XML_ESC_AMP_STR,   '&',  XML_ESC_AMP_LEN}  /* &amp; */
    , {XML_ESC_APOS, XML_ESC_APOS_STR,  '\'', XML_ESC_APOS_LEN} /* &apos; */
    , {XML_ESC_QUOT, XML_ESC_QUOT_STR,  '"',  XML_ESC_QUOT_LEN} /* &quot; */

    /* 未知类型: 只有&开头才判断是否为转义字符。未知，则是首字母& */
    , {XML_ESC_UNKNOWN, XML_ESC_UNKNOWN_STR, '&',  XML_ESC_UNKNOWN_LEN}
};
#endif /*__XML_ESC_PARSE__*/

/******************************************************************************
 **函数名称: xml_node_creat
 **功    能: 创建XML结点
 **输入参数:
 **     xml: XML树
 **     type: 结点类型(xml_node_type_e)
 **输出参数: NONE
 **返    回: 结点地址
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.02.18 #
 ******************************************************************************/
xml_node_t *xml_node_creat(xml_tree_t *xml, xml_node_type_e type)
{
    xml_node_t *node;

    node = (xml_node_t*)xml->alloc(xml->pool, sizeof(xml_node_t));
    if (NULL == node) {
        return NULL;
    }

    xml_node_init(node, type);

    return node;
}

/******************************************************************************
 **函数名称: xml_node_creat_ext
 **功    能: 创建XML结点
 **输入参数:
 **     xml: XML树
 **     type: 结点类型(xml_node_type_e)
 **     name: 结点名
 **     vlaue: 结点值
 **输出参数: NONE
 **返    回: 结点地址
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.06.11 #
 ******************************************************************************/
xml_node_t *xml_node_creat_ext(xml_tree_t *xml,
        xml_node_type_e type, const char *name, const char *value)
{
    int size;
    xml_node_t *node;

    /* 1. 创建结点 */
    node = (xml_node_t*)xml->alloc(xml->pool, sizeof(xml_node_t));
    if (NULL == node) {
        return NULL;
    }

    xml_node_init(node, type);

    /* 2. 设置结点名 */
    size = strlen(name) + 1;
    node->name.str = (char *)xml->alloc(xml->pool, size);
    if (NULL == node->name.str) {
        xml_node_free_one(xml, node);
        return NULL;
    }

    node->name.len = snprintf(node->name.str, size, "%s", name);

    /* 3. 设置结点值 */
    if (xml_set_value(xml, node, value)) {
        xml_node_free_one(xml, node);
        return NULL;
    }

    return node;
}

/******************************************************************************
 **函数名称: xml_init
 **功    能: 初始化XML树
 **输入参数:
 **     opt: 选项信息
 **输出参数:
 **返    回: XML树
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
xml_tree_t *xml_init(xml_opt_t *opt)
{
    xml_tree_t *xml;
    xml_node_t *root;

    /* 1. 创建XML对象 */
    xml = (xml_tree_t *)opt->alloc(opt->pool, sizeof(xml_tree_t));
    if (NULL == xml) {
        return NULL;
    }

    xml->log = opt->log;
    xml->pool = opt->pool;
    xml->alloc = opt->alloc;
    xml->dealloc = opt->dealloc;

    /* 2. 创建根结点 */
    root = (xml_node_t *)opt->alloc(opt->pool, sizeof(xml_node_t));
    if (NULL == root) {
        opt->dealloc(opt->pool, xml);
        return NULL;
    }

    xml_node_init(root, XML_NODE_ROOT);

    xml->root = root;

    return xml;
}

/******************************************************************************
 **函数名称: xml_fload
 **功    能: 将XML文件载入文件缓存
 **输入参数:
 **     fname: 文件路径名
 **输出参数:
 **返    回: 文件缓存
 **实现描述:
 **注意事项: 内存分配的最小单位为1KB, 防止内存碎片.
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
char *xml_fload(const char *fname, xml_opt_t *opt)
{
    FILE *fp;
    char *buff;
    int left, off, n;
    struct stat st;

    /* 1. 判断文件状态是否正常 */
    if (0 != stat(fname, &st)) {
        return NULL;
    }

    /* 2. 分配文件缓存空间 */
    buff = (char *)opt->alloc(opt->pool, st.st_size + 1);
    if (NULL == buff) {
        return NULL;
    }

    /* 3. 将文件载入缓存 */
    fp = fopen(fname, "r");
    if (NULL == fp) {
        opt->dealloc(opt->pool, buff);
        return NULL;
    }

    off = 0;
    left = st.st_size;
    while (!feof(fp) && (left > 0)) {
        n = fread(buff + off, 1, left, fp);
        if (ferror(fp)) {
            fclose(fp);
            opt->dealloc(opt->pool, buff);
            return NULL;
        }

        left -= n;
        off += n;
    }

    buff[st.st_size] = '\0';

    fclose(fp);
    return buff;
}

/******************************************************************************
 **函数名称: xml_parse
 **功    能: 解析XML文件缓存
 **输入参数:
 **     xml: XML树
 **     stack: XML栈
 **     str: XML字串
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述:
 **     1. 解析版本信息
 **     2. 解析XML BODY
 **注意事项:
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
int xml_parse(xml_tree_t *xml, Stack_t *stack, const char *str, size_t len)
{
    xml_parse_t parse;

    parse.str = str;
    parse.ptr = str;
    parse.len = len; /* 未知 */

    while (!XmlIsStrEndChar(*(parse.ptr))
            && ((size_t)(parse.ptr - parse.str) < parse.len))
    {
        while (XmlIsIgnoreChar(*(parse.ptr))) parse.ptr++;    /* 跳过无意义的字符 */

        switch(*(parse.ptr)) {
            case XML_BEGIN_FLAG:
            {
                switch(*(parse.ptr+1))
                {
                    case XML_VERS_FLAG:  /* "<?" 版本开始 */
                    {
                        /* 版本信息不用加载到XML树中 */
                        if (xml_parse_version(xml, &parse)) {
                            log_error(xml->log, "XML format is wrong![%-.32s] [%ld]",
                                parse.ptr, parse.ptr-parse.str);
                            return XML_ERR_FORMAT;
                        }
                        break;
                    }
                    case XML_NOTE_FLAG:   /* "<!--" 注释信息 */
                    {
                        /* 注释信息不用加载到XML树中 */
                        if (xml_parse_note(xml, &parse)) {
                            log_error(xml->log, "XML format is wrong![%-.32s]", parse.ptr);
                            return XML_ERR_FORMAT;
                        }
                        break;
                    }
                    case XML_END_FLAG:   /* "</" 结点结束 */
                    {
                        if (xml_parse_end(xml, stack, &parse)) {
                            log_error(xml->log, "XML format is wrong![%-.32s] [%ld]",
                                parse.ptr, parse.ptr-parse.str);
                            return XML_ERR_FORMAT;
                        }
                        break;
                    }
                    default:    /* "<XYZ" 结点开始 */
                    {
                        if (xml_parse_mark(xml, stack, &parse)) {
                            log_error(xml->log, "Parse XML failed! [%-.32s] [%ld]",
                                parse.ptr, parse.ptr-parse.str);
                            return XML_ERR_FORMAT;
                        }
                        break;
                    }
                }
                break;
            }
            case STR_END_FLAG:  /* 字串结束'\0' */
            {
                if (stack_isempty(stack)) {
                    log_trace(xml->log, "Parse xml success!");
                    return XML_OK;
                }
                log_error(xml->log, "Invalid format! [%-.32s] [%ld]", parse.ptr, parse.ptr-parse.str);
                return XML_ERR_FORMAT;
            }
            default:            /* 非法字符 */
            {
                log_error(xml->log, "Invalid format! [%-.32s] [%ld]", parse.ptr, parse.ptr-parse.str);
                return XML_ERR_FORMAT;
            }
        }
    }

    if (!stack_isempty(stack)) {
        log_error(xml->log, "Invalid format! [%-.32s]", parse.ptr);
        return XML_ERR_FORMAT;
    }

    return XML_OK;
}

/******************************************************************************
 **函数名称: xml_parse_version
 **功    能: 解析XML文件缓存版本信息
 **输入参数:
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述:
 **注意事项: XML大小写敏感
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
static int xml_parse_version(xml_tree_t *xml, xml_parse_t *parse)
{
    char border = '"';
    const char *ptr;

    /* 匹配版本开头"<?xml " */
    if (strncmp(parse->ptr, XML_VERS_BEGIN, XML_VERS_BEGIN_LEN)) {
        log_error(xml->log, "XML format is wrong![%-.32s]", parse->ptr);
        return XML_ERR_FORMAT;
    }

    parse->ptr += XML_VERS_BEGIN_LEN; /* 跳过版本开头"<?xml " */

    /* 检查格式是否正确 */
    /* 跳过无意义字符 */
    while (XmlIsIgnoreChar(*parse->ptr)) { ++parse->ptr; }

    while (!XmlIsDoubtChar(*parse->ptr) && !XmlIsStrEndChar(*parse->ptr)) {
        ptr = parse->ptr;

        /* 属性名是否正确 */
        while (XmlIsMarkChar(*ptr)) { ++ptr; }
        if (ptr == parse->ptr) {
            log_error(xml->log, "XML format is wrong![%-.32s]", parse->ptr);
            return XML_ERR_FORMAT;
        }

        if (!XmlIsEqualChar(*ptr)) {
            log_error(xml->log, "XML format is wrong![%-.32s]", parse->ptr);
            return XML_ERR_FORMAT;
        }
        ptr++;
        parse->ptr = ptr;

        /* 属性值是否正确 */
        while (XmlIsIgnoreChar(*ptr)) { ++ptr; } /* 跳过=之后的无意义字符 */

        /* 判断是双引号(")还是单引号(')为版本属性值的边界 */
        if (XmlIsQuotChar(*ptr) || XmlIsSQuotChar(*ptr)) {
            border = *ptr;
        }
        else {                              /* 不为双/单引号，则格式错误 */
            log_error(xml->log, "XML format is wrong![%-.32s]", parse->ptr);
            return XML_ERR_FORMAT;
        }
        ptr++;
        parse->ptr = ptr;

        while ((*ptr != border) && !XmlIsStrEndChar(*ptr)) { ++ptr; }

        if (*ptr != border) {
            log_error(xml->log, "XML format is wrong![%-.32s]", parse->ptr);
            return XML_ERR_FORMAT;
        }
        ptr++;  /* 跳过双/单引号 */

		/* 跳过无意义字符 */
        while (XmlIsIgnoreChar(*ptr)) { ++ptr; }
        parse->ptr = ptr;
    }

    /* 版本信息以"?>"结束 */
    if (!XmlIsDoubtChar(*parse->ptr)) {
        log_error(xml->log, "XML format is wrong![%-.32s]", parse->ptr);
        return XML_ERR_FORMAT;
    }
    parse->ptr++;  /* 跳过? */

    if (!XmlIsRPBrackChar(*parse->ptr)) {
        log_error(xml->log, "XML format is wrong![%-.32s]", parse->ptr);
        return XML_ERR_FORMAT;
    }

    parse->ptr++;
    return XML_OK;
}

/******************************************************************************
 **函数名称: xml_parse_note
 **功    能: 解析XML文件缓存注释信息
 **输入参数:
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述:
 **注意事项: XML大小写敏感
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
static int xml_parse_note(xml_tree_t *xml, xml_parse_t *parse)
{
    const char *ptr;

	/* 匹配注释开头"<!--" */
    if (strncmp(parse->ptr, XML_NOTE_BEGIN, XML_NOTE_BEGIN_LEN)) {
        log_error(xml->log, "XML format is wrong![%-.32s]", parse->ptr);
        return XML_ERR_FORMAT;
    }

    parse->ptr += XML_NOTE_BEGIN_LEN; /* 跳过注释开头"<!--" */

    /* 因在注释信息的结点中不允许出现"-->"，所以可使用如下匹配查找结束 */
    ptr = strstr(parse->ptr, XML_NOTE_END1);
    if ((NULL == ptr) || (XML_NOTE_END2 != *(ptr + XML_NOTE_END1_LEN))) {
        log_error(xml->log, "XML format is wrong![%-.32s]", parse->ptr);
        return XML_ERR_FORMAT;
    }

    parse->ptr = ptr;
    parse->ptr += XML_NOTE_END_LEN;
    return XML_OK;
}

/******************************************************************************
 **函数名称: xml_mark_end
 **功    能: 标签以/>为标志结束
 **输入参数:
 **     stack: 栈
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.02.23 #
 ******************************************************************************/
#define xml_mark_end(stack, parse) (parse->ptr+=XML_MARK_END2_LEN, stack_pop(stack), 0)

/******************************************************************************
 **函数名称: xml_parse_mark
 **功    能: 处理标签结点
 **输入参数:
 **     xml: XML树
 **     stack: XML栈
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述:
 **     1. 提取标签名
 **     2. 提取标签各属性
 **     3. 提取标签值
 **注意事项:
 **     注意: 以上3个步骤是固定的，否则将会出现混乱
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
static int xml_parse_mark(xml_tree_t *xml, Stack_t *stack, xml_parse_t *parse)
{
    int ret;
    parse->ptr += XML_MARK_BEGIN_LEN;    /* 跳过"<" */

    /* 1. 提取标签名，并入栈 */
    if (xml_mark_get_name(xml, stack, parse)) {
        log_error(xml->log, "Get mark name failed!");
        return XML_ERR;
    }

    /* 2. 提取标签属性 */
    if (xml_mark_has_attr(parse)) {
        if (xml_mark_get_attr(xml, stack, parse)) {
            log_error(xml->log, "Get mark attr failed!");
            return XML_ERR;
        }
    }

    /* 3. 标签是否结束:
            如果是<ABC DEF="EFG"/>格式时，此时标签结束；
            如果是<ABC DEF="EFG">HIGK</ABC>格式时，此时标签不结束 */
    if (xml_mark_is_end(parse)) {
        return xml_mark_end(stack, parse);
    }

    /* 4. 提取标签值 */
    ret = xml_mark_has_value(parse);
    switch(ret) {
        case XML_HAS_VALUE:
        {
            return xml_mark_get_value(xml, stack, parse);
        }
        case XML_NO_VALUE:
        {
            return XML_OK;
        }
        default:
        {
            log_error(xml->log, "XML format is wrong![%-.32s]", parse->ptr);
            return ret;
        }
    }

    return XML_OK;
}

/******************************************************************************
 **函数名称: xml_parse_end
 **功    能: 处理结束结点(处理</XXX>格式的结束)
 **输入参数:
 **     stack: XML栈
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述:
 **注意事项: XML大小写敏感
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
static int xml_parse_end(xml_tree_t *xml, Stack_t *stack, xml_parse_t *parse)
{
    size_t len;
    xml_node_t *top;
    const char *ptr;

    parse->ptr += XML_MARK_END2_LEN; /* 跳过</ */
    ptr = parse->ptr;

    /* 1. 确定结束结点名长度 */
    while (XmlIsMarkChar(*ptr)) { ++ptr; }

    if (!XmlIsRPBrackChar(*ptr)) {
        log_error(xml->log, "XML format is wrong![%-.32s]", parse->ptr);
        return XML_ERR_FORMAT;
    }

    len = ptr - parse->ptr;

    /* 2. 获取栈中顶结点信息 */
    top = (xml_node_t*)stack_pop(stack);
    if (NULL == top) {
        log_error(xml->log, "Get stack top member failed!");
        return XML_ERR_STACK;
    }

    /* 3. 结点名是否一致 */
    if (len != top->name.len
        || (0 != strncmp(top->name.str, parse->ptr, len)))
    {
        log_error(xml->log, "Mark name is not match![%s][%-.32s]", top->name.str, parse->ptr);
        return XML_ERR_MARK_MISMATCH;
    }

    ptr++;
    parse->ptr = ptr;

    return XML_OK;
}

/******************************************************************************
 **函数名称: xml_mark_get_name
 **功    能: 提取标签名，并入栈
 **输入参数:
 **     xml: XML树
 **     stack: XML栈
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述:
 **注意事项:
 **     1. 因新建结点已加入XML树中，因此在此不必去释放新结点的内存空间
 **     2. 此时tail用来记录孩子结点链表尾
 **作    者: # Qifeng.zou # 2013.02.23 #
 ******************************************************************************/
static int xml_mark_get_name(xml_tree_t *xml, Stack_t *stack, xml_parse_t *parse)
{
    int len;
    xml_node_t *node, *top;
    const char *ptr = parse->ptr;

    /* 1. 新建结点，并初始化 */
    node = xml_node_creat(xml, XML_NODE_UNKNOWN);
    if (NULL == node) {
        log_error(xml->log, "Create xml node failed!");
        return XML_ERR_CREAT_NODE;
    }

    /* 2. 将结点加入XML树中 */
    if (stack_isempty(stack)) {
        if (NULL == xml->root->tail) {
            xml->root->child = node;
        }
        else {
            xml->root->tail->next = node;
        }
        xml->root->tail = node;
        node->parent = xml->root;
        xml_set_type(node, XML_NODE_CHILD);
        xml_set_child_flag(xml->root);
    }
    else {
        xml_set_type(node, XML_NODE_CHILD);
        top = stack_gettop(stack);
        node->parent = top;
        if (NULL == top->tail) {
            top->child = node;
        }
        else {
            top->tail->next = node;
        }
        top->tail = node;
        xml_set_child_flag(top);
    }

    /* 3. 确定结点名长度 */
    while (XmlIsMarkChar(*ptr)) { ++ptr; }

    /* 4.判断标签名边界是否合法 */
    if (!XmlIsMarkBorder(*ptr)) {
        log_error(xml->log, "XML format is wrong!\n[%-32.32s]", parse->ptr);
        return XML_ERR_FORMAT;
    }

    len = ptr - parse->ptr;

    /* 5. 提取出结点名 */
    node->name.str = (char *)xml->alloc(xml->pool, len + 1);
    if (NULL == node->name.str) {
        log_error(xml->log, "Calloc failed!");
        return XML_ERR_CALLOC;
    }

    strncpy(node->name.str, parse->ptr, len);
    node->name.len = len;
    node->name.str[len] = '\0';

    /* 6. 将结点入栈 */
    if (stack_push(stack, (void*)node)) {
        log_error(xml->log, "Stack push failed!");
        return XML_ERR_STACK;
    }

    parse->ptr = ptr;

    return XML_OK;
}

/******************************************************************************
 **函数名称: xml_mark_has_attr
 **功    能: 判断标签是否有属性结点
 **输入参数:
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.02.23 #
 ******************************************************************************/
static int xml_mark_has_attr(xml_parse_t *parse)
{
    const char *ptr = parse->ptr;

    while (XmlIsIgnoreChar(*ptr)) { ++ptr; }    /* 跳过无意义的字符 */

    parse->ptr = ptr;

    if (XmlIsMarkChar(*ptr)) {
        return true;
    }

    return false;
}

/******************************************************************************
 **函数名称: xml_mark_get_attr
 **功    能: 解析有属性的标签
 **输入参数:
 **
 **     stack: XML栈
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述:
 **注意事项:
 **     1. 属性值可使用双引号或单引号确定属性值范围
 **     2. 转义符号的转换对应关系如下:
 **       &lt;    <    小于
 **       &gt;    >    大于
 **       &amp;   &    和号
 **       &apos;  '    单引号
 **       &quot;  "    引号
 **作    者: # Qifeng.zou # 2013.02.18 #
 **修    改: # Qifeng.zou # 2014.01.06 #
 ******************************************************************************/
static int xml_mark_get_attr(xml_tree_t *xml, Stack_t *stack, xml_parse_t *parse)
{
    char border = '"';
    xml_node_t *node, *top;
    int len, errflg = 0;
    const char *ptr = parse->ptr;
#if defined(__XML_ESC_PARSE__)
    int ret, size;
    xml_esc_split_t split;
    const xml_esc_t *esc = NULL;

    memset(&split, 0, sizeof(split));
#endif /*__XML_ESC_PARSE__*/

    /* 1. 获取正在处理的标签 */
    top = (xml_node_t*)stack_gettop(stack);
    if (NULL == top) {
        log_error(xml->log, "Get stack top failed!");
        return XML_ERR_STACK;
    }

    /* 3. 将属性结点依次加入标签子结点链表 */
    do {
        /* 3.1 新建结点，并初始化 */
        node = xml_node_creat(xml, XML_NODE_ATTR);
        if (NULL == node) {
            log_error(xml->log, "Create xml node failed!");
            return XML_ERR_CREAT_NODE;
        }

        /* 3.2 获取属性名 */
        while (XmlIsIgnoreChar(*ptr)) { ++ptr; }/* 跳过属性名之前无意义的空格 */

        parse->ptr = ptr;
        while (XmlIsMarkChar(*ptr)) { ++ptr; }  /* 查找属性名的边界 */

        len = ptr - parse->ptr;
        node->name.str = (char *)xml->alloc(xml->pool, (len+1)*sizeof(char));
        if (NULL == node->name.str) {
            errflg = 1;
            log_error(xml->log, "Calloc failed!");
            break;
        }

        memcpy(node->name.str, parse->ptr, len);
        node->name.len = len;
        node->name.str[len] = '\0';

        /* 3.3 获取属性值 */
        while (XmlIsIgnoreChar(*ptr)) { ++ptr; }        /* 跳过=之前的无意义字符 */

        if (!XmlIsEqualChar(*ptr)) {                    /* 不为等号，则格式错误 */
            errflg = 1;
            log_error(xml->log, "Attribute format is incorrect![%-.32s]", parse->ptr);
            break;
        }
        ptr++;                                  /* 跳过"=" */
        while (XmlIsIgnoreChar(*ptr)) { ++ptr; }/* 跳过=之后的无意义字符 */

        /* 判断是单引号(')还是双引号(")为属性的边界 */
        if (XmlIsQuotChar(*ptr) || XmlIsSQuotChar(*ptr)) {
            border = *ptr;
        }
        else {                /* 不为 双/单 引号，则格式错误 */
            errflg = 1;
            log_error(xml->log, "XML format is wrong![%-.32s]", parse->ptr);
            break;
        }

        ptr++;
        parse->ptr = ptr;
        while ((*ptr != border) && !XmlIsStrEndChar(*ptr))/* 计算 双/单 引号之间的数据长度 */
        {
        #if defined(__XML_ESC_PARSE__)
            if (XmlIsAndChar(*ptr)) {
                /* 判断并获取转义字串类型及相关信息 */
                esc = xml_esc_get(ptr);

                /* 对包含有转义字串的字串进行切割 */
                ret = xml_esc_split(xml, esc, parse->ptr, ptr-parse->ptr+1, &split);
                if (XML_OK != ret) {
                    errflg = 1;
                    log_error(xml->log, "Parse forwad string failed!");
                    break;
                }

                ptr += esc->len;
                parse->ptr = ptr;
            }
            else
        #endif /*__XML_ESC_PARSE__*/
            {
                ptr++;
            }
        }

        if (*ptr != border) {
            errflg = 1;
            log_error(xml->log, "Mismatch border [%c]![%-.32s]", border, parse->ptr);
            break;
        }

        len = ptr - parse->ptr;
        ptr++;  /* 跳过" */

    #if defined(__XML_ESC_PARSE__)
        if (NULL != split.head) {
            size = xml_esc_size(&split);
            size += len+1;

            node->value = (char *)xml->alloc(xml->pool, size);
            if (NULL == node->value) {
                errflg = 1;
                log_error(xml->log, "Alloc memory failed!");
                break;
            }

            xml_esc_merge(&split, node->value);

            strncat(node->value, parse->ptr, len);

            xml_esc_free(&split);
        }
        else
    #endif /*__XML_ESC_PARSE__*/
        {
            node->value.str = (char *)xml->alloc(xml->pool, len+1);
            if (NULL == node->value.str) {
                errflg = 1;
                log_error(xml->log, "Calloc failed!");
                break;
            }

            memcpy(node->value.str, parse->ptr, len);
            node->value.len = len;
            node->value.str[len] = '\0';
        }

        /* 3.4 将结点加入属性链表 */
        if (NULL == top->tail) { /* 还没有孩子结点 */
            top->child = node;
        }
        else {
            top->tail->next = node;
        }
        node->parent = top;
        top->tail = node;

        /* 3.5 指针向后移动 */
        while (XmlIsIgnoreChar(*ptr)) { ++ptr; }

    }while (XmlIsMarkChar(*ptr));

#if defined(__XML_ESC_PARSE__)
    xml_esc_free(&split);
#endif /*__XML_ESC_PARSE__*/

    if (1 == errflg) {       /* 防止内存泄漏 */
        xml_node_free(xml, node);
        node = NULL;
        return XML_ERR_GET_ATTR;
    }

    parse->ptr = ptr;
    xml_set_attr_flag(top);

    return XML_OK;
}

/******************************************************************************
 **函数名称: xml_mark_is_end
 **功    能: 标签是否结束 "/>"
 **输入参数:
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述:
 **注意事项: XML大小写敏感
 **作    者: # Qifeng.zou # 2013.02.23 #
 ******************************************************************************/
static int xml_mark_is_end(xml_parse_t *parse)
{
    const char *ptr = parse->ptr;

    while (XmlIsIgnoreChar(*ptr)) { ++ptr; }

    /* 1. 是否有结点值 */
    if (strncmp(ptr, XML_MARK_END1, XML_MARK_END1_LEN)) {
        return false;
    }
    return true;
}

/******************************************************************************
 **函数名称: xml_mark_has_value
 **功    能: 是否有结点值
 **输入参数:
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: true:有 false:无 -1: 错误格式
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.02.23 #
 ******************************************************************************/
static int xml_mark_has_value(xml_parse_t *parse)
{
    const char *ptr = parse->ptr;

    while (XmlIsIgnoreChar(*ptr)) { ++ptr; }

    if (XmlIsRPBrackChar(*ptr)) {
        ptr++;

        /* 跳过起始的空格和换行符 */
        while (XmlIsIgnoreChar(*ptr)) { ++ptr; }

        parse->ptr = ptr;
        if (XmlIsLPBrackChar(*ptr)) { /* 出现子结点 */
            return XML_NO_VALUE;
        }
        return XML_HAS_VALUE;
    }

    return XML_ERR_FORMAT;
}

/******************************************************************************
 **函数名称: xml_mark_get_value
 **功    能: 获取结点值
 **输入参数:
 **     stack: XML栈
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.02.23 #
 ******************************************************************************/
static int xml_mark_get_value(xml_tree_t *xml, Stack_t *stack, xml_parse_t *parse)
{
    char border = '<'; /* 取值边界 */
    int len, size = 0;
    const char *p1, *p2;
    xml_node_t *curr;
#if defined(__XML_ESC_PARSE__)
    const xml_esc_t *esc;
    xml_esc_split_t split;

    memset(&split, 0, sizeof(split));
#endif /*__XML_ESC_PARSE__*/

    curr = (xml_node_t*)stack_gettop(stack);
    if (NULL == curr) {
        return XML_ERR_STACK;
    }

    p1 = parse->ptr;

    while (XmlIsIgnoreChar(*p1)) { ++p1; }

    if (XmlIsQuotChar(*p1) || XmlIsSQuotChar(*p1)) {
        border = *p1; /* 记录取值边界 */
        ++p1;
    }

    parse->ptr = p1;

    /* 提取结点值: 允许结点值中出现空格和换行符 */
    while (!XmlIsStrEndChar(*p1) && (border != *p1))
    {
    #if defined(__XML_ESC_PARSE__)
        if (XmlIsAndChar(*p1)) {
            esc = xml_esc_get(p1);

            if (xml_esc_split(esc, parse->ptr, p1-parse->ptr+1, &split)) {
                xml_esc_free(&split);
                log_error(xml->log, "Parse forwad string failed!");
                return XML_ERR;
            }

            p1 += esc->len;
            parse->ptr = p1;
        }
        else
    #endif /*__XML_ESC_PARSE__*/
        {
            p1++;
        }
    }

    if (XmlIsLPBrackChar(*p1)) {      /* 为尖括号时 */
        p2 = p1;
        p1--;
        while (XmlIsIgnoreChar(*p1)) { --p1; }
    }
    else if (XmlIsStrEndChar(*p1)) { /* 为结束符时 */
    #if defined(__XML_ESC_PARSE__)
        xml_esc_free(&split);
    #endif /*__XML_ESC_PARSE__*/
        log_error(xml->log, "XML format is wrong! MarkName:[%s]", curr->name.str);
        return XML_ERR_FORMAT;
    }
    else {                          /* 为单引号或双引号时 */
        p2 = p1 + 1;
        p1--;
        while (XmlIsIgnoreChar(*p2)) { ++p2; }

        if (!XmlIsLPBrackChar(*p2)) {
            log_error(xml->log, "XML format is wrong! [%s]", p2);
            return XML_ERR_FORMAT;
        }
    }

    len = p1 - parse->ptr + 1;
#if defined(__XML_ESC_PARSE__)
    size = xml_esc_size(&split);
#endif /*__XML_ESC_PARSE__*/
    size += len+1;

    curr->value.str = (char *)xml->alloc(xml->pool, size*sizeof(char));
    if (NULL == curr->value.str) {
    #if defined(__XML_ESC_PARSE__)
        xml_esc_free(&split);
    #endif /*__XML_ESC_PARSE__*/
        log_error(xml->log, "Calloc failed!");
        return XML_ERR_CALLOC;
    }

#if defined(__XML_ESC_PARSE__)
    if (NULL != split.head) {
        curr->value.str[0] = '\0';

        xml_esc_merge(&split, curr->value);

        strncat(curr->value.str, parse->ptr, len);
        curr->value.len = len;

        xml_esc_free(&split);
    }
    else
#endif /*__XML_ESC_PARSE__*/
    {
        strncpy(curr->value.str, parse->ptr, len);
        curr->value.len = len;
        curr->value.str[len] = '\0';
    }

    parse->ptr = p2;
    xml_set_value_flag(curr);

#if defined(__XML_EITHER_CHILD_OR_VALUE__)
    /* 判断：有数值的情况下，是否还有孩子结点 */
    if ((XML_BEGIN_FLAG == *p2) && (XML_END_FLAG == *(p2+1))) {
        return XML_OK;
    }

    log_error(xml->log, "XML format is wrong: Node have child and value at same time!");
    return XML_ERR_FORMAT;
#endif /*__XML_EITHER_CHILD_OR_VALUE__*/

    return XML_OK;
}

/******************************************************************************
 **函数名称: xml_node_free_one
 **功    能: 释放单个结点
 **输入参数:
 **     xml: XML树
 **     node: 需要被释放的结点
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.02.27 #
 ******************************************************************************/
int xml_node_free_one(xml_tree_t *xml, xml_node_t *node)
{
    if (NULL != node->name.str) {
        xml->dealloc(xml->pool, node->name.str);
    }

    if (NULL != node->value.str) {
        xml->dealloc(xml->pool, node->value.str);
    }

    xml->dealloc(xml->pool, node);
    return XML_OK;
}

/******************************************************************************
 **函数名称: xml_free_next
 **功    能: 获取下一个需要被处理的结点
 **输入参数:
 **     stack: 栈
 **     curr: 当前正在处理的结点
 **输出参数:
 **返    回: 下一个需要处理的结点
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.02.27 #
 ******************************************************************************/
xml_node_t *xml_free_next(xml_tree_t *xml, Stack_t *stack, xml_node_t *curr)
{
    xml_node_t *child, *top;

    /* 1. 释放孩子结点 */
    if (NULL != curr->temp) {     /* 首先: 处理孩子结点: 选出下一个孩子结点 */
        child = curr->temp;
        curr->temp = child->next;
        curr = child;
        return curr;
    }
    else {                          /* 再次: 处理其兄弟结点: 选出下一个兄弟结点 */
        /* 1. 弹出已经处理完成的结点, 并释放 */
        top = stack_pop(stack);
        if (NULL == top) {
            log_error(xml->log, "Stack pop failed!");
            return NULL;
        }

        if (stack_isempty(stack)) {
            xml_node_free_one(xml, top);
            return NULL;
        }

        /* 2. 处理其下一个兄弟结点 */
        curr = top->next;
        xml_node_free_one(xml, top);
        while (NULL == curr) {   /* 所有兄弟结点已经处理完成，说明父亲结点也处理完成 */
            /* 3. 父亲结点出栈 */
            top = stack_pop(stack);
            if (NULL == top) {
                log_error(xml->log, "Stack pop failed!");
                return NULL;
            }

            if (stack_isempty(stack)) {
                xml_node_free_one(xml, top);
                return NULL;
            }

            /* 5. 选择父亲的兄弟结点 */
            curr = top->next;
            xml_node_free_one(xml, top);
        }
    }

    return curr;
}

/******************************************************************************
 **函数名称: xml_delete_child
 **功    能: 从孩子结点链表中删除指定的孩子结点
 **输入参数:
 **     node: 需要删除孩子结点的结点
 **     child: 孩子结点
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述:
 **注意事项:
 **     从树中删除的结点，只是从树中被剥离出来，其相关内存并没有被释放.
 **     释放时需调用函数xml_node_free()
 **作    者: # Qifeng.zou # 2013.03.02 #
 ******************************************************************************/
int xml_delete_child(xml_tree_t *xml, xml_node_t *node, xml_node_t *child)
{
    xml_node_t *p1, *p2;

    if (node != child->parent) {
        log_error(xml->log, "Parent node is not right!");
        return XML_ERR_PTR;
    }

    if (node->child == child) {  /* 1. 要删的是子结点链表的开始结点 */
        node->child = child->next;  /* 剔除链表 */
        if (NULL == node->child) {
            node->tail = NULL;
            if (xml_is_attr(child)) {
                xml_unset_attr_flag(node);
            }
        }
        else if (xml_is_attr(child) && !xml_is_attr(node->child)) {
            xml_unset_attr_flag(node);
        }
        return XML_OK;
    }

    p1 = node->child;
    p2 = p1->next;
    while (NULL != p2) {
        if (child == p2) {
            p1->next = child->next; /* 剔除链表 */
            if (node->tail == child) {
                node->tail = p1;
            }

            if (NULL == child->next) {
                if (xml_is_child(child) && !xml_is_child(p1)) {
                    xml_unset_child_flag(node);
                }
            }
            return XML_OK;
        }
        p1 = p2;
        p2 = p2->next;
    }
	return XML_OK;
}

/* 打印结点名长度(注: XML有层次格式) */
#define xml_node_name_len(node, depth, len) \
{ \
    while (depth > 1) { \
        /*fprintf(fp, "\t");*/ \
        len++; \
        depth--; \
    } \
    /*fprintf(fp, "<%s", node->name);*/ \
    len += (node->name.len + 1); \
}

/* 打印属性结点长度(注: XML有层次格式) */
#define xml_node_attr_len(node, len) \
{ \
    while (NULL != node->temp) { \
        if (xml_is_attr(node->temp)) { \
            /*fprintf(fp, " %s=\"%s\"", node->temp->name, node->temp->value);*/ \
            len += (node->temp->name.len + node->temp->value.len + 4); \
            node->temp = node->temp->next; \
            continue; \
        } \
        break; \
    } \
}

/* 打印结点值长度(注: XML有层次格式) */
#define xml_node_value_len(node, len) \
{ \
    if (xml_has_value(node)) { \
        if (xml_has_child(node)) { /* 此时temp指向node的孩子结点 或 NULL */ \
            /* fprintf(fp, ">%s\n", node->value); */ \
            len += (node->value.len + 2); \
        } \
        else { \
            /* fprintf(fp, ">%s</%s>\n", node->value, node->name); */ \
            len += (node->value.len + node->name.len + 5); \
        } \
    } \
    else { \
        if (NULL != node->temp) { /* 此时temp指向node的孩子结点 或 NULL */ \
            /* fprintf(fp, ">\n"); */ \
            len += 2; \
        } \
        else { \
            /* fprintf(fp, "/>\n"); */ \
            len += 3; \
        } \
    } \
}

/******************************************************************************
 **函数名称: xml_node_next_len
 **功    能: 获取下一个要处理的结点，并计算当前结束结点的长度(注: XML有层次结构)
 **输入参数:
 **     root: XML树根结点
 **     stack: 栈
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.06.10 #
 ******************************************************************************/
static xml_node_t *xml_node_next_len(
    xml_tree_t *xml, Stack_t *stack, xml_node_t *node, int *len)
{
    xml_node_t *top, *child;
    int depth, level, len2;

    /* 首先: 处理孩子结点: 选出下一个孩子结点 */
    if (NULL != node->temp) {
        child = node->temp;
        node->temp = child->next;
        node = child;
        return node;
    }

    /* 再次: 处理其兄弟结点: 选出下一个兄弟结点 */

    len2 = 0;

    /* 1. 弹出已经处理完成的结点 */
    top = stack_gettop(stack);
    if (xml_has_child(top)) {
        depth = stack_depth(stack);
        level = depth - 1;
        while (level > 1) {
            /* fprintf(fp, "\t"); */
            len2++;
            level--;
        }
        /* fprintf(fp, "</%s>\n", top->name); */
        len2 += (top->name.len + 4);
    }

    if (NULL == stack_pop(stack)) {
        *len += len2;
        log_error(xml->log, "Stack pop failed!");
        return NULL;
    }

    if (stack_isempty(stack)) {
        *len += len2;
        log_error(xml->log, "Compelte fprint!");
        return NULL;
    }

    /* 2. 处理其下一个兄弟结点 */
    node = top->next;
    while (NULL == node) {   /* 所有兄弟结点已经处理完成，说明父亲结点也处理完成 */
        /* 3. 父亲结点出栈 */
        top = stack_pop(stack);
        if (NULL == top) {
            *len += len2;
            log_error(xml->log, "Stack pop failed!");
            return NULL;
        }

        /* 4. 打印父亲结点结束标志 */
        if (xml_has_child(top)) {
            depth = stack_depth(stack);
            level = depth + 1;
            while (level > 1) {
                /* fprintf(fp, "\t"); */
                len2++;
                level--;
            }
            /* fprintf(fp, "</%s>\n", top->name); */
            len2 += (top->name.len + 4);
        }

        if (stack_isempty(stack)) {
            *len += len2;
            return NULL;    /* 处理完成 */
        }

        /* 5. 选择父亲的兄弟结点 */
        node = top->next;
    }

    *len += len2;
    return node;
}

/******************************************************************************
 **函数名称: _xml_node_len
 **功    能: 计算结点打印成XML格式字串时的长度(注: XML有层次结构)
 **输入参数:
 **     root: XML树根结点
 **     stack: 栈
 **输出参数:
 **返    回: 结点及其属性、孩子结点的总长度
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2013.06.10 #
 ******************************************************************************/
int _xml_node_len(xml_tree_t *xml, xml_node_t *root, Stack_t *stack)
{
    int depth, len;
    xml_node_t *node = root;

    depth = stack_depth(stack);
    if (0 != depth) {
        log_error(xml->log, "Stack depth must empty. depth:[%d]", depth);
        return XML_ERR_STACK;
    }

    len = 0;

    do {
        /* 1. 将要处理的结点压栈 */
        node->temp = node->child;
        if (stack_push(stack, node)) {
            log_error(xml->log, "Stack push failed!");
            return XML_ERR_STACK;
        }

        /* 2. 打印结点名 */
        depth = stack_depth(stack);

        xml_node_name_len(node, depth, len);

        /* 3. 打印属性结点 */
        if (xml_has_attr(node)) {
            xml_node_attr_len(node, len);
        }

        /* 4. 打印结点值 */
        xml_node_value_len(node, len);

        /* 5. 选择下一个处理的结点: 从父亲结点、兄弟结点、孩子结点中 */
        node = xml_node_next_len(xml, stack, node, &len);
    }while (NULL != node);

    if (!stack_isempty(stack)) {
        return XML_ERR_STACK;
    }
    return len;
}

#if defined(__XML_ESC_PARSE__)
/******************************************************************************
 **函数名称: xml_esc_get
 **功    能: 获取转义字串的信息
 **输入参数:
 **     str: 以&开头的字串
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述:
 **注意事项:
 **     转义字串的对应关系如下:
 **       &lt;    <    小于
 **       &gt;    >    大于
 **       &amp;   &    和号
 **       &apos;  '    单引号
 **       &quot;  "    引号
 **作    者: # Qifeng.zou # 2014.01.06 #
 ******************************************************************************/
static const xml_esc_t *xml_esc_get(const char *str)
{
    if (XmlIsLtStr(str)) {       /* &lt; */
        return &g_xml_esc_str[XML_ESC_LT];
    }
    else if (XmlIsGtStr(str)) {  /* &gt; */
        return &g_xml_esc_str[XML_ESC_GT];
    }
    else if (XmlIsAmpStr(str)) { /* &amp; */
        return &g_xml_esc_str[XML_ESC_AMP];
    }
    else if (XmlIsAposStr(str)) { /* &apos; */
        return &g_xml_esc_str[XML_ESC_APOS];
    }
    else if (XmlIsQuotStr(str)) { /* &quot; */
        return &g_xml_esc_str[XML_ESC_QUOT];
    }

    return &g_xml_esc_str[XML_ESC_UNKNOWN];    /* 未知类型 */
}

/******************************************************************************
 **函数名称: xml_esc_size
 **功    能: 获取转义切割之前字段长度之和
 **输入参数:
 **     s: 被切割后的字串链表
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.01.06 #
 ******************************************************************************/
static int xml_esc_size(const xml_esc_split_t *sp)
{
    int size = 0;
    xml_esc_node_t *node = sp->head;

    while (NULL != node) {
        size = node->len;
        node = node->next;
    }

    return size;
}

/******************************************************************************
 **函数名称: xml_esc_merge
 **功    能: 合并被切割转义的字串
 **输入参数:
 **     s: 被切割后的字串链表
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.01.06 #
 ******************************************************************************/
static int xml_esc_merge(const xml_esc_split_t *sp, char *dst)
{
    char *ptr = dst;
    xml_esc_node_t *fnode = sp->head;

    while (NULL != fnode) {
        sprintf(ptr, "%s", fnode->str);
        ptr += fnode->len;
        fnode = fnode->next;
    }

    return XML_OK;
}

/******************************************************************************
 **函数名称: xml_esc_free
 **功    能: 是否转义切割对象
 **输入参数:
 **     split: 切割对象
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.01.06 #
 ******************************************************************************/
static int xml_esc_free(xml_tree_t *xml, xml_esc_split_t *split)
{
    xml_esc_node_t *node, *next;

    node = split->head;
    while (NULL != node) {
        next = node->next;
        xml->dealloc(xml->pool, node->str);
        xml->dealloc(xml->pool, node);
        node = next;
    }

    split->head = NULL;
    split->tail = NULL;

    return XML_OK;
}

/******************************************************************************
 **函数名称: xml_esc_split
 **功    能: 切割并转义从转义字串及之前的字串
 **输入参数:
 **     esc: 对应的转义信息
 **     str: 字串
 **     len: str+len处的字串需要进行转义, 即:str[len]='&'
 **输出参数:
 **     split: 分割后的结果
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **     转义字串的对应关系如下:
 **       &lt;    <    小于
 **       &gt;    >    大于
 **       &amp;   &    和号
 **       &apos;  '    单引号
 **       &quot;  "    引号
 **作    者: # Qifeng.zou # 2014.01.06 #
 ******************************************************************************/
static int xml_esc_split(xml_tree_t *xml, const xml_esc_t *esc,
    const char *str, int len, xml_esc_split_t *split)
{
    xml_esc_node_t *node;

    node = (xml_esc_node_t *)xml->alloc(xml->pool, sizeof(xml_esc_node_t));
    if (NULL == node) {
        log_error(xml->log, "Calloc memory failed!");
        return XML_ERR_CALLOC;
    }

    node->next = NULL;

    node->str = (char *)xml->alloc(xml->pool, len+1);
    if (NULL == node->str) {
        xml->dealloc(xml->pool, node);
        return XML_ERR_CALLOC;
    }

    strncpy(node->str, str, len-1);
    node->str[len-1] = esc->ch;
    node->len = len;
    node->str[len] = '\0';

    if (NULL == split->head) {
        split->head = node;
    }
    else {
        split->tail->next = node;
    }
    split->tail = node;

    return XML_OK;
}
#endif /*__XML_ESC_PARSE__*/
