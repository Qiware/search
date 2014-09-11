/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: html_comm.c
 ** 版本号: 1.0
 ** 描  述: HTML格式的解析
 **        解析HTML格式的文件、字串: 在此主要使用的是压栈的思路，其效率比使用递
 **        归算法的处理思路效率更高.对比数据如下:
 **        1. 创建HTML: 高5%~10%
 **        2. 查找效率: 高30%左右
 **        3. 释放效率: 高20%左右
 ** 作  者: # Qifeng.zou # 2013.02.18 #
 ** 修  改:
 **     1. # 增加转义字符的处理 # Qifeng.zou # 2014.01.06 #
 ******************************************************************************/
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "html_tree.h"
#include "html_comm.h"
#include "common.h"
#include "log.h"

static int html_parse_doctype(html_tree_t *html, html_parse_t *parse);
static int html_parse_note(html_tree_t *html, html_parse_t *parse);
static int html_parse_mark(html_tree_t *html, Stack_t *stack, html_parse_t *parse);
static int html_parse_end(html_tree_t *html, Stack_t *stack, html_parse_t *parse);
static int html_parse_special(html_tree_t *html, Stack_t *stack, html_parse_t *parse);

static int html_mark_get_name(html_tree_t *html, Stack_t *stack, html_parse_t *parse);
static int html_mark_has_attr(html_parse_t *parse);
static int html_mark_get_attr(html_tree_t *html, Stack_t *stack, html_parse_t *parse);
static int html_mark_is_end(html_parse_t *parse);
static int html_mark_has_value(html_parse_t *parse);
static int html_mark_get_value(html_tree_t *html, Stack_t *stack, html_parse_t *parse);
static int html_script_get_value(html_tree_t *html, Stack_t *stack, html_parse_t *parse);

static int html_mark_mismatch_hdl(html_tree_t *html, html_node_t *node);

/******************************************************************************
 **函数名称: html_node_creat
 **功    能: 创建HTML节点
 **输入参数: 
 **     type: 节点类型(html_node_type_e)
 **输出参数: NONE
 **返    回: 节点地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.18 #
 ******************************************************************************/
html_node_t *html_node_creat(html_node_type_e type)
{
    html_node_t *node = NULL;

    node = (html_node_t*)calloc(1, sizeof(html_node_t));
    if (NULL == node)
    {
        return NULL;
    }

    node->name = NULL;
    node->value = NULL;
    node->type = type;

    node->next = NULL;
    node->firstchild = NULL;
    node->parent = NULL;
    html_reset_flag(node);
    node->temp = NULL;
    node->tail = NULL;

    return node;
}

/******************************************************************************
 **函数名称: html_node_creat_ext
 **功    能: 创建HTML节点
 **输入参数: 
 **     type: 节点类型(html_node_type_e)
 **     name: 节点名
 **     vlaue: 节点值
 **输出参数: NONE
 **返    回: 节点地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.06.11 #
 ******************************************************************************/
html_node_t *html_node_creat_ext(
        html_node_type_e type,
        const char *name, const char *value)
{
    int size=0, ret=0;
    html_node_t *node = NULL;

    /* 1. 创建节点 */
    node = (html_node_t*)calloc(1, sizeof(html_node_t));
    if (NULL == node)
    {
        return NULL;
    }

    node->type = type;

    /* 2. 设置节点名 */
    size = strlen(name) + 1;
    node->name = (char *)calloc(1, size);
    if (NULL == node->name)
    {
        html_node_sfree(node);
        return NULL;
    }
    snprintf(node->name, size, "%s", name);

    /* 3. 设置节点值 */
    ret = html_set_value(node, value);
    if (0 != ret)
    {
        html_node_sfree(node);
        return NULL;
    }
    
    node->next = NULL;
    node->firstchild = NULL;
    node->parent = NULL;
    node->temp = NULL;
    node->tail = NULL;

    return node;
}

/******************************************************************************
 **函数名称: html_init
 **功    能: 初始化HTML树
 **输入参数:
 **     html: HTML树
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
int html_init(html_tree_t **html)
{
    *html = (html_tree_t*)calloc(1, sizeof(html_tree_t));
    if (NULL == *html)
    {
        return HTML_ERR_CALLOC;
    }

    (*html)->root = (html_node_t*)calloc(1, sizeof(html_node_t));
    if (NULL == (*html)->root)
    {
        free(*html), *html = NULL;
        return HTML_ERR_CALLOC;
    }

    return HTML_OK;
}

/******************************************************************************
 **函数名称: html_fload
 **功    能: 将HTML文件载入文件缓存
 **输入参数:
 **     fname: 文件路径名
 **     length: 获取文件长度
 **输出参数:
 **返    回: 文件缓存
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
char *html_fload(const char *fname)
{
    int ret = 0, left = 0, num = 0, offset = 0;
    FILE *fp = NULL;
    char *buff = NULL;
    struct stat fstate;

    memset(&fstate, 0, sizeof(fstate));

    /* 判断文件状态是否正常 */
    ret = stat(fname, &fstate);
    if (HTML_OK != ret)
    {
        return NULL;
    }

    /* 分配文件缓存空间 */
    buff = (char *)calloc(1, (fstate.st_size+1)*sizeof(char));
    if (NULL == buff)
    {
        return NULL;
    }

    /* 将文件载入缓存 */
    fp = fopen(fname, "r");
    if (NULL == fp)
    {
        free(buff), buff=NULL;
        return NULL;
    }

    offset = 0;
    left = fstate.st_size;
    while (!feof(fp) && (left > 0))
    {
        num = fread(buff + offset, 1, left, fp);
        if (ferror(fp))
        {
            fclose(fp), fp = NULL;
            free(buff), buff = NULL;
            return NULL;
        }
        
        left -= num;
        offset += num;
    }

    fclose(fp), fp = NULL;
    return buff;
}

/******************************************************************************
 **函数名称: html_parse
 **功    能: 解析HTML文件缓存
 **输入参数:
 **     html: HTML树
 **     stack: HTML栈
 **     str: HTML字串
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **     1. 解析版本信息
 **     2. 解析HTML BODY
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
int html_parse(html_tree_t *html, Stack_t *stack, const char *str)
{
    int ret = 0;
    html_parse_t parse;

    parse.str = str;
    parse.ptr = str;
    parse.length = -1; /* 未知 */

    while (!HtmlIsStrEndChar(*(parse.ptr)))
    {
        while (HtmlIsIgnoreChar(*(parse.ptr))) parse.ptr++;    /* 跳过无意义的字符 */

        switch(*(parse.ptr))
        {
            case HTML_BEGIN_FLAG:
            {
                switch(*(parse.ptr+1))
                {
                    case HTML_NOTE_FLAG:   /* "<!--" 注释信息 */
                    {
                        if ('-' == *(parse.ptr+2))
                        {
                            /* 注释信息不用加载到HTML树中 */
                            ret = html_parse_note(html, &parse);
                        }
                        else
                        {
                            ret = html_parse_doctype(html, &parse);
                        }
                        if (HTML_OK != ret)
                        {
                            log2_error("HTML format is wrong![%-.1024s]", parse.ptr);
                            return HTML_ERR_FORMAT;
                        }
                        break;
                    }
                    case HTML_END_FLAG:   /* "</" 节点结束 */
                    {
                        ret = html_parse_end(html, stack, &parse);
                        if (HTML_OK != ret)
                        {
                            log2_error("HTML format is wrong![%-.1024s] [%ld]",
                                parse.ptr, parse.ptr-parse.str);
                            return HTML_ERR_FORMAT;
                        }
                        break;
                    }
                    default:    /* "<XYZ" 节点开始 */
                    {
                        ret = html_parse_mark(html, stack, &parse);
                        if (HTML_OK != ret)
                        {
                            log2_error("Parse HTML failed! [%-.1024s] [%ld]",
                                parse.ptr, parse.ptr-parse.str);
                            return HTML_ERR_FORMAT;
                        }
                        break;
                    }
                }
                break;
            }
            case STR_END_FLAG:  /* 字串结束'\0' */
            {
                if (stack_isempty(stack))
                {
                    log2_debug("Parse html success!");
                    return HTML_OK;
                }
                log2_error("Invalid format! [%-.1024s] [%ld]",
                    parse.ptr, parse.ptr-parse.str);
                return HTML_ERR_FORMAT;
            }
            default:            /* 非法字符 */
            {
                /* 判断是否存在特殊情况, 并处理 */
                ret = html_parse_special(html, stack, &parse);
                if (HTML_OK != ret)
                {
                    log2_error("Invalid format! [%-.1024s] [%ld]",
                        parse.ptr, parse.ptr-parse.str);
                    return HTML_ERR_FORMAT;
                }
                break;
            }
        }
    }

    if (!stack_isempty(stack))
    {
        log2_error("Invalid format! [%-.1024s]", parse.ptr);
        return HTML_ERR_FORMAT;
    }
    
    return HTML_OK;
}

/******************************************************************************
 **函数名称: html_parse_doctype
 **功    能: 解析HTML格式文档类型信息
 **输入参数:
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **参考格式:
 **     1. 凤凰网 
 **         <!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
 **             "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
 **     2. 腾讯主页
 **         <!DOCTYPE html>
 **作    者: # Qifeng.zou # 2014.09.06 #
 ******************************************************************************/
static int html_parse_doctype(html_tree_t *html, html_parse_t *parse)
{
    int ret = 0;
    char border = '"';
    const char *ptr = NULL;

    /* 匹配版本开头"<?DOCTYPE " */
    ret = strncmp(parse->ptr, HTML_DOC_TYPE_BEGIN, HTML_DOC_TYPE_BEGIN_LEN);
    if (0 != ret)
    {
        log2_error("HTML format is wrong![%-.1024s]", parse->ptr);
        return HTML_ERR_FORMAT;
    }

    parse->ptr += HTML_DOC_TYPE_BEGIN_LEN; /* 跳过版本开头"<?DOCTYPE " */

    /* 检查格式是否正确 */
    /* 跳过无意义字符 */
    while (HtmlIsIgnoreChar(*parse->ptr))
    {
        parse->ptr++;
    }

    while (!HtmlIsStrEndChar(*parse->ptr)      /* 结束符'\0' */
        && !HtmlIsRPBrackChar(*parse->ptr))    /* 右尖括号'>' */
    {
        ptr = parse->ptr;
        
        if (HtmlIsDQuotChar(*ptr)
            || HtmlIsSQuotChar(*ptr)) /* 引号 */
        {
            border = *ptr;
            ptr++;
            while (*ptr != border
                && !HtmlIsStrEndChar(*parse->ptr))
            {
                ptr++;
            }

            if (*ptr != border)
            {
                log2_error("HTML format is wrong![%-.1024s]", parse->ptr);
                return HTML_ERR_FORMAT;
            }
            ptr++;  /* 跳过双/单引号 */

		    /* 跳过无意义字符 */
            while (HtmlIsIgnoreChar(*ptr))
            {
                ptr++;
            }
            parse->ptr = ptr;
            continue;
        }

        while (HtmlIsMarkChar(*ptr))
        {
            ptr++;
        }

        /* 跳过无意义字符 */
        while (HtmlIsIgnoreChar(*ptr))
        {
            ptr++;
        }

        parse->ptr = ptr;
        continue;
    }

    /* 文档类型信息以">"结束 */
    if (!HtmlIsRPBrackChar(*parse->ptr))
    {
        log2_error("HTML format is wrong![%-.1024s]", parse->ptr);
        return HTML_ERR_FORMAT;
    }
    
    parse->ptr++;
    return HTML_OK;
}

/******************************************************************************
 **函数名称: html_parse_note
 **功    能: 解析HTML文件缓存注释信息
 **输入参数:
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
static int html_parse_note(html_tree_t *html, html_parse_t *parse)
{
    int ret = 0;
    const char *ptr = NULL;

	/* 匹配注释开头"<!--" */
    ret = strncmp(parse->ptr, HTML_NOTE_BEGIN, HTML_NOTE_BEGIN_LEN);
    if (0 != ret)
    {
        log2_error("HTML format is wrong![%-.1024s]", parse->ptr);
        return HTML_ERR_FORMAT;
    }

    parse->ptr += HTML_NOTE_BEGIN_LEN; /* 跳过注释开头"<!--" */
    
    /* 因在注释信息的节点中不允许出现"-->"，所以可使用如下匹配查找结束 */
    ptr = strstr(parse->ptr, HTML_NOTE_END1);
    if ((NULL == ptr) || (HTML_NOTE_END2 != *(ptr + HTML_NOTE_END1_LEN)))
    {
        log2_error("HTML format is wrong![%-.1024s]", parse->ptr);
        return HTML_ERR_FORMAT;
    }

    parse->ptr = ptr;
    parse->ptr += HTML_NOTE_END_LEN;
    return HTML_OK;
}

/******************************************************************************
 **函数名称: html_mark_end
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
#define html_mark_end(stack, parse) (parse->ptr+=HTML_MARK_END2_LEN, stack_pop(stack))

/******************************************************************************
 **函数名称: html_parse_mark
 **功    能: 处理标签节点
 **输入参数:
 **     html: HTML树
 **     stack: HTML栈
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
static int html_parse_mark(html_tree_t *html, Stack_t *stack, html_parse_t *parse)
{
    int ret = 0;
    html_node_t *top;

    parse->ptr += HTML_MARK_BEGIN_LEN;    /* 跳过"<" */

    /* 1. 提取标签名，并入栈 */
    ret = html_mark_get_name(html, stack, parse);
    if (HTML_OK != ret)
    {
        log2_error("Get mark name failed!");
        return ret;
    }
    
    /* 2. 提取标签属性 */
    if (html_mark_has_attr(parse))
    {
        ret = html_mark_get_attr(html, stack, parse);
        if (HTML_OK != ret)
        {
            log2_error("Get mark attr failed!");
            return ret;
        }
    }

    /* 3. 标签是否结束:
            如果是<ABC DEF="EFG"/>格式时，此时标签结束；
            如果是<ABC DEF="EFG">HIGK</ABC>格式时，此时标签不结束 */
    if (html_mark_is_end(parse))
    {
        return html_mark_end(stack, parse);
    }

    /* 4. 提取标签值 */
    ret = html_mark_has_value(parse);
    switch(ret)
    {
        case true:
        {
            top = stack_gettop(stack);
            if (!strcasecmp(HTML_MARK_SCRIPT, top->name))
            {
                return html_script_get_value(html, stack, parse);
            }

            return html_mark_get_value(html, stack, parse);
        }
        case false:
        {
            return HTML_OK;
        }
        default:
        {
            log2_error("HTML format is wrong![%-.1024s]", parse->ptr);
            return ret;
        }
    }
 
    return HTML_OK;
}
   
/******************************************************************************
 **函数名称: html_parse_end
 **功    能: 处理结束节点(处理</XXX>格式的结束)
 **输入参数:
 **     stack: HTML栈
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
static int html_parse_end(html_tree_t *html, Stack_t *stack, html_parse_t *parse)
{
    int ret;
    size_t len;
    html_node_t *top;
    const char *ptr;

    parse->ptr += HTML_MARK_END2_LEN; /* 跳过</ */
    ptr = parse->ptr;
    
    /* 1. 确定结束节点名长度 */
    while (HtmlIsMarkChar(*ptr)) ptr++;
    
    if (!HtmlIsRPBrackChar(*ptr))
    {
        log2_error("HTML format is wrong![%-.1024s]", parse->ptr);
        return HTML_ERR_FORMAT;
    }

    len = ptr - parse->ptr;

    /* 2. 获取栈中顶节点信息 */
    top = (html_node_t*)stack_gettop(stack);
    if (NULL == top)
    {
        log2_error("Get stack top failed!");
        return HTML_ERR_STACK;
    }

    /* 3. 节点名是否一致 */
    if (len != strlen(top->name)
        || (0 != strncmp(top->name, parse->ptr, len)))
    {
    #if defined(__HTML_AUTO_RESTORE__)
        do
        {
            log2_error("Mismatch![%s][%-.32s] [%d/%d]",
                top->name, parse->ptr, strlen(top->name), len);

            /* Step 1: 将TOP结点的孩子结点改为兄弟结点 */
            ret = html_mark_mismatch_hdl(html, top);
            if (HTML_OK != ret)
            {
                log2_error("Html restore failed!");
                return HTML_ERR;
            }

            log2_debug("Html restore success!");

            /* Step 2: 将TOP结点的孩子结点改为兄弟结点 */
            stack_pop(stack);

            top = (html_node_t*)stack_gettop(stack);
            if (NULL == top)
            {
                log2_error("Get stack top failed!");
                return HTML_ERR_STACK;
            }
        } while (len != strlen(top->name) || 0 != strncasecmp(top->name, parse->ptr, len));

        log2_debug("Match success![%s][%-.32s] [%d/%d]",
                top->name, parse->ptr, strlen(top->name), len);
    #else /*!__HTML_AUTO_RESTORE__*/
        log2_error("Mark name is not match![%s][%-.1024s]", top->name, parse->ptr);
        return HTML_ERR_MARK_MISMATCH;
    #endif /*!__HTML_AUTO_RESTORE__*/
    }

    /* 4. 弹出栈顶节点 */
    ret = stack_pop(stack);
    if (HTML_OK != ret)
    {
        log2_error("Pop failed!");
        return HTML_ERR_STACK;
    }

    ptr++;
    parse->ptr = ptr;
        
    return HTML_OK;
}

/******************************************************************************
 **函数名称: html_parse_special
 **功    能: 解析HTML中的特殊情况
 **输入参数:
 **     html: HTML树
 **     stack: HTML栈
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **     如果此时结点已经有值，说明此时出现的是非法数据；如果没有值，则说明出现
 **     的数据为节点值。 如：
 **     <a href="www.baidu.com"> 
 **         <span id="dingyue" class="txtright"></span>
 **         今日更新
 **     </a>
 **     出现如下情况时，将"今日更新"解析为标签a的节点值.
 **作    者: # Qifeng.zou # 2014.09.07 #
 ******************************************************************************/
static int html_parse_special(html_tree_t *html, Stack_t *stack, html_parse_t *parse)
{
    html_node_t *top;

    top = stack_gettop(stack);
    if (NULL == top)
    {
        return HTML_ERR;
    }

    if (html_has_value(top))
    {
        return HTML_ERR;
    }

    return html_mark_get_value(html, stack, parse);
}

/******************************************************************************
 **函数名称: html_mark_get_name
 **功    能: 提取标签名，并入栈
 **输入参数:
 **     html: HTML树
 **     stack: HTML栈
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **     注意: 1. 因新建节点已加入HTML树中，因此在此不必去释放新节点的内存空间
 **           2. 此时tail用来记录孩子节点链表尾
 **作    者: # Qifeng.zou # 2013.02.23 #
 ******************************************************************************/
static int html_mark_get_name(html_tree_t *html, Stack_t *stack, html_parse_t *parse)
{
    int ret=0, len=0;
    const char *ptr = parse->ptr;
    html_node_t *node = NULL, *top = NULL;

    /* 1. 新建节点，并初始化 */
    node = html_node_creat(HTML_NODE_UNKNOWN);
    if (NULL == node)
    {
        log2_error("Create html node failed!");
        return HTML_ERR_CREAT_NODE;
    }

    /* 2. 将节点加入HTML树中 */
    if (stack_isempty(stack))
    {
        if (NULL == html->root->tail)
        {
            html->root->firstchild = node;
        }
        else
        {
            html->root->tail->next = node;
        }
        html->root->tail = node;
        node->parent = html->root;
        html_set_type(node, HTML_NODE_CHILD);
        html_set_child_flag(html->root);
    }
    else
    {
        html_set_type(node, HTML_NODE_CHILD);
        top = stack_gettop(stack);
        node->parent = top;
        if (NULL == top->tail)
        {
            top->firstchild = node;
        }
        else
        {
            top->tail->next = node;
        }
        top->tail = node;
        html_set_child_flag(top);
    }

    /* 3. 确定节点名长度 */
    while (HtmlIsMarkChar(*ptr)) ptr++;

    /* 4.判断标签名边界是否合法 */
    if (!HtmlIsMarkBorder(*ptr))
    {
        log2_error("HTML format is wrong!\n[%-32.32s]", parse->ptr);
        return HTML_ERR_FORMAT;
    }

    len = ptr - parse->ptr;

    /* 5. 提取出节点名 */
    node->name = calloc(1, (len+1)*sizeof(char));
    if (NULL == node->name)
    {
        log2_error("Calloc failed!");
        return HTML_ERR_CALLOC;
    }
    strncpy(node->name, parse->ptr, len);

    /* 6. 将节点入栈 */
    ret = stack_push(stack, (void*)node);
    if (HTML_OK != ret)
    {
        log2_error("Stack push failed!");
        return HTML_ERR_STACK;
    }

    parse->ptr = ptr;

    return HTML_OK;
}

/******************************************************************************
 **函数名称: html_mark_has_attr
 **功    能: 判断标签是否有属性节点
 **输入参数:
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.23 #
 ******************************************************************************/
static int html_mark_has_attr(html_parse_t *parse)
{
    const char *ptr = parse->ptr;

    while (HtmlIsIgnoreChar(*ptr)) ptr++;    /* 跳过无意义的字符 */

    parse->ptr = ptr;
    
    if (HtmlIsMarkChar(*ptr))
    {
        return true;
    }

    return false;
}

/******************************************************************************
 **函数名称: html_mark_get_attr
 **功    能: 解析有属性的标签
 **输入参数:
 **     
 **     stack: HTML栈
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
static int html_mark_get_attr(html_tree_t *html, Stack_t *stack, html_parse_t *parse)
{
    char border = '"';
    html_node_t *node = NULL, *top = NULL;
    int len = 0, errflg = 0;
    const char *ptr = parse->ptr;

    /* 1. 获取正在处理的标签 */
    top = (html_node_t*)stack_gettop(stack);
    if (NULL == top)
    {
        log2_error("Get stack top failed!");
        return HTML_ERR_STACK;
    }

    /* 3. 将属性节点依次加入标签子节点链表 */
    do
    {
        /* 3.1 新建节点，并初始化 */
        node = html_node_creat(HTML_NODE_ATTR);
        if (NULL == node)
        {
            log2_error("Create html node failed!");
            return HTML_ERR_CREAT_NODE;
        }

        /* 3.2 获取属性名 */
        while (HtmlIsIgnoreChar(*ptr)) ptr++;/* 跳过属性名之前无意义的空格 */

        parse->ptr = ptr;
        while (HtmlIsMarkChar(*ptr)) ptr++;  /* 查找属性名的边界 */

        len = ptr - parse->ptr;
        node->name = (char*)calloc(1, (len+1)*sizeof(char));
        if (NULL == node->name)
        {
            errflg = 1;
            log2_error("Calloc failed!");
            break;
        }

        memcpy(node->name, parse->ptr, len);
        
        /* 3.3 获取属性值 */
        while (HtmlIsIgnoreChar(*ptr)) ptr++;         /* 跳过=之前的无意义字符 */

        if (!HtmlIsEqualChar(*ptr))                      /* 不为等号，则格式错误 */
        {
            errflg = 1;
            log2_error("Attribute format is incorrect![%-.1024s]", parse->ptr);
            break;
        }
        ptr++;                                  /* 跳过"=" */
        while (HtmlIsIgnoreChar(*ptr)) ptr++;     /* 跳过=之后的无意义字符 */

        /* 1) 判断是单引号(')还是双引号(")为属性的边界 */
        if (HtmlIsDQuotChar(*ptr) || HtmlIsSQuotChar(*ptr))
        {
            border = *ptr;

            ptr++;
            parse->ptr = ptr;

            /* 计算 双/单 引号之间的数据长度 */
            while ((*ptr != border) && !HtmlIsStrEndChar(*ptr))
            {
                ptr++;
            }

            if (HtmlIsStrEndChar(*ptr))
            {
                errflg = 1;
                log2_error("Mismatch border [%c]![%-.1024s]", border, parse->ptr);
                break;
            }

            len = ptr - parse->ptr;
            ptr++;  /* 跳过" */
        }
        /* 2) 不为 双/单 引号时，则以空格和'>'结束属性值 */
        else
        {
        #if 1
            /* 查找属性值的边界:空格或> */
            while (!isspace(*ptr)
                && !HtmlIsStrEndChar(*ptr)
                && !HtmlIsRPBrackChar(*ptr))
            {
                ptr++;
            }

            if (HtmlIsStrEndChar(*ptr))
            {
                errflg = 1;
                log2_error("Mismatch border [%c]![%-.1024s]", border, parse->ptr);
                break;
            }

            len = ptr - parse->ptr;
        #else
            errflg = 1;
            log2_error("Html format is wrong! [%-.1024s]", parse->ptr);
            break;
        #endif
        }

        node->value = (char*)calloc(1, (len+1)*sizeof(char));
        if (NULL == node->value)
        {
            errflg = 1;
            log2_error("Calloc failed!");
            break;
        }

        memcpy(node->value, parse->ptr, len);

        /* 3.4 将节点加入属性链表 */
        if (NULL == top->tail) /* 还没有孩子节点 */
        {
            top->firstchild = node;
        }
        else
        {
            top->tail->next = node;
        }
        node->parent = top;
        top->tail = node;
        
        /* 3.5 指针向后移动 */
        while (HtmlIsIgnoreChar(*ptr)) ptr++;

    }while (HtmlIsMarkChar(*ptr));

    if (1 == errflg)         /* 防止内存泄漏 */
    {
        html_node_free(html, node);
        node = NULL;
        return HTML_ERR_GET_ATTR;
    }

    parse->ptr = ptr;
    html_set_attr_flag(top);

    return HTML_OK;
}

/******************************************************************************
 **函数名称: html_mark_is_end
 **功    能: 标签是否结束 "/>"
 **输入参数:
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.23 #
 ******************************************************************************/
static int html_mark_is_end(html_parse_t *parse)
{
    int ret = 0;
    const char *ptr = parse->ptr;
    
    while (HtmlIsIgnoreChar(*ptr)) ptr++;

    /* 1. 是否有节点值 */
    ret = strncmp(ptr, HTML_MARK_END1, HTML_MARK_END1_LEN);
    if (0 != ret)
    {
        return false;
    }
    return true;
}

/******************************************************************************
 **函数名称: html_mark_has_value
 **功    能: 是否有节点值
 **输入参数:
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: true:有 false:无 -1: 错误格式
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.23 #
 ******************************************************************************/
static int html_mark_has_value(html_parse_t *parse)
{
    const char *ptr = parse->ptr;

    while (HtmlIsIgnoreChar(*ptr)) ptr++;
    
    if (HtmlIsRPBrackChar(*ptr))
    {
        ptr++;

        /* 跳过起始的空格和换行符 */
        while (HtmlIsIgnoreChar(*ptr)) ptr++;

        parse->ptr = ptr;
        if (HtmlIsLPBrackChar(*ptr)) /* 出现子节点 */
        {
            return false;
        }
        return true;
    }
    
    return HTML_ERR_FORMAT;
}

/******************************************************************************
 **函数名称: html_mark_get_value
 **功    能: 获取节点值
 **输入参数: 
 **     html: HTML树
 **     stack: HTML栈
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.23 #
 ******************************************************************************/
static int html_mark_get_value(html_tree_t *html, Stack_t *stack, html_parse_t *parse)
{
    int len = 0, size = 0;
    const char *p1=NULL, *p2=NULL;
    html_node_t *current = NULL;

    current = (html_node_t*)stack_gettop(stack);
    if (NULL == current)
    {
        return HTML_ERR_STACK;
    }

    p1 = parse->ptr;

    while (HtmlIsIgnoreChar(*p1)) p1++;

    parse->ptr = p1;
    
    /* 提取节点值: 允许节点值中出现空格和换行符 */    
    while (!HtmlIsStrEndChar(*p1))
    {
        if (HtmlIsLPBrackChar(*p1)
        #if defined(__HTML_DEL_BR__)
            && 0 != strncasecmp(p1, "<br />", 6)
            && 0 != strncasecmp(p1, "<br/>", 5)
        #endif /*__HTML_DEL_BR__*/
        )
        {
            break;
        }
        p1++;
    }

    if (HtmlIsStrEndChar(*p1))
    {
        log2_error("HTML format is wrong! MarkName:[%s]", current->name);
        return HTML_ERR_FORMAT;
    }
    
    p2 = p1;
    p1--;
    while (HtmlIsIgnoreChar(*p1)) p1--;

    p1++;

    len = p1 - parse->ptr;
    size += len+1;

    current->value = (char*)calloc(1, size*sizeof(char));
    if (NULL == current->value)
    {
        log2_error("Calloc failed!");
        return HTML_ERR_CALLOC;
    }

    strncpy(current->value, parse->ptr, len);

    parse->ptr = p2;
    html_set_value_flag(current);

#if defined(__HTML_OCOV__)
    /* 判断：有数值的情况下，是否还有孩子节点 */
    if ((HTML_BEGIN_FLAG == *p2) && (HTML_END_FLAG == *(p2+1)))
    {
        return HTML_OK;
    }

    log2_error("HTML format is wrong: Node have child and value at same time!");
    return HTML_ERR_FORMAT;
#endif /*__HTML_OCOV__*/

    return HTML_OK;
}

/******************************************************************************
 **函数名称: html_script_get_value
 **功    能: 获取脚本结点值
 **输入参数: 
 **     html: HTML树
 **     stack: HTML栈
 **     parse: 解析文件缓存信息
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.23 #
 ******************************************************************************/
static int html_script_get_value(html_tree_t *html, Stack_t *stack, html_parse_t *parse)
{
    int len = 0, size = 0;
    const char *p1=NULL, *p2=NULL;
    html_node_t *current = NULL;

    current = (html_node_t*)stack_gettop(stack);
    if (NULL == current)
    {
        return HTML_ERR_STACK;
    }

    p1 = parse->ptr;

    while (HtmlIsIgnoreChar(*p1)) p1++;

    parse->ptr = p1;
    
    /* 提取节点值: 允许节点值中出现空格和换行符 */    
    while (!HtmlIsStrEndChar(*p1))
    {
        if (HtmlIsLPBrackChar(*p1)
            && !strncasecmp(p1, HTML_MARK_SCRIPT_END, strlen(HTML_MARK_SCRIPT_END)))
        {
            break;
        }
        p1++;
    }

    if (HtmlIsStrEndChar(*p1))
    {
        log2_error("HTML format is wrong! MarkName:[%s]", current->name);
        return HTML_ERR_FORMAT;
    }
    
    p2 = p1;
    p1--;
    while (HtmlIsIgnoreChar(*p1)) p1--;

    p1++;

    len = p1 - parse->ptr;
    size += len+1;

    current->value = (char*)calloc(1, size*sizeof(char));
    if (NULL == current->value)
    {
        log2_error("Calloc failed!");
        return HTML_ERR_CALLOC;
    }

    strncpy(current->value, parse->ptr, len);

    parse->ptr = p2;
    html_set_value_flag(current);

    return HTML_OK;
}

/******************************************************************************
 **函数名称: html_node_sfree
 **功    能: 释放单个节点
 **输入参数:
 **     node: 需要被释放的节点
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.27 #
 ******************************************************************************/
int html_node_sfree(html_node_t *node)
{
    if (NULL != node->name)
    {
        free(node->name);
        node->name = NULL;
    }

    if (NULL != node->value)
    {
        free(node->value);
        node->value = NULL;
    }

    free(node);
    return HTML_OK;
}

/******************************************************************************
 **函数名称: html_free_next
 **功    能: 获取下一个需要被处理的节点
 **输入参数:
 **     stack: 栈
 **     current: 当前正在处理的节点
 **输出参数:
 **返    回: 下一个需要处理的节点
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.27 #
 ******************************************************************************/
html_node_t *html_free_next(html_tree_t *html, Stack_t *stack, html_node_t *current)
{
    int ret = 0;
    html_node_t *child = NULL, *top = NULL;
    
    /* 1. 释放孩子节点 */
    if (NULL != current->temp)       /* 首先: 处理孩子节点: 选出下一个孩子节点 */
    {
        child = current->temp;
        current->temp = child->next;
        current = child;
        return current;
    }
    else                            /* 再次: 处理其兄弟节点: 选出下一个兄弟节点 */
    {
        /* 1. 弹出已经处理完成的节点, 并释放 */
        top = stack_gettop(stack);
        
        ret = stack_pop(stack);
        if (HTML_OK != ret)
        {
            log2_error("Stack pop failed!");
            return NULL;
        }
        
        if (stack_isempty(stack))
        {
            html_node_sfree(top), top = NULL;
            return NULL;
        }
        
        /* 2. 处理其下一个兄弟节点 */
        current = top->next;
        html_node_sfree(top), top = NULL;
        while (NULL == current)     /* 所有兄弟节点已经处理完成，说明父亲节点也处理完成 */
        {
            /* 3. 父亲节点出栈 */
            top = stack_gettop(stack);
            ret = stack_pop(stack);
            if (HTML_OK != ret)
            {
                log2_error("Stack pop failed!");
                return NULL;
            }
            
            if (stack_isempty(stack))
            {
                html_node_sfree(top), top = NULL;
                return NULL;
            }
    
            /* 5. 选择父亲的兄弟节点 */
            current = top->next;
            html_node_sfree(top), top = NULL;
        }
    }

    return current;
}

/******************************************************************************
 **函数名称: html_delete_child
 **功    能: 从孩子节点链表中删除指定的孩子节点
 **输入参数:
 **     node: 需要删除孩子节点的节点
 **     child: 孩子节点
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **     从树中删除的节点，只是从树中被剥离出来，其相关内存并没有被释放.
 **     释放时需调用函数html_node_free()
 **作    者: # Qifeng.zou # 2013.03.02 #
 ******************************************************************************/
int html_delete_child(html_tree_t *html, html_node_t *node, html_node_t *child)
{
    html_node_t *p1 = NULL, *p2 = NULL;

    if (node != child->parent)
    {
        log2_error("Parent node is not right!");
        return -1;
    }
    
    if (node->firstchild == child)    /* 1. 要删的是子节点链表的开始节点 */
    {
        node->firstchild = child->next;  /* 剔除链表 */
        if (NULL == node->firstchild)
        {
            node->tail = NULL;
            if (html_is_attr(child))
            {
                html_unset_attr_flag(node);
            }
        }
        else if (html_is_attr(child) && !html_is_attr(node->firstchild))
        {
            html_unset_attr_flag(node);
        }
        return HTML_OK;
    }

    p1 = node->firstchild;
    p2 = p1->next;
    while (NULL != p2)
    {
        if (child == p2)
        {
            p1->next = child->next; /* 剔除链表 */
            if (node->tail == child)
            {
                node->tail = p1;
            }

            if (NULL == child->next)
            {
                if (html_is_child(child) && !html_is_child(p1))
                {
                    html_unset_child_flag(node);
                }
            }
            return HTML_OK;
        }
        p1 = p2;
        p2 = p2->next;
    }
	return HTML_OK;
}

/* 打印节点名长度(注: HTML有层次格式) */
#define html_node_name_length(node, depth, length) \
{ \
    while (depth > 1) \
    { \
        /*fprintf(fp, "\t");*/ \
        length++; \
        depth--; \
    } \
    /*fprintf(fp, "<%s", node->name);*/ \
    length += (1 + strlen(node->name)); \
}

/* 打印属性节点长度(注: HTML有层次格式) */
#define html_node_attr_length(node, length) \
{ \
    while (NULL != node->temp) \
    { \
        if (html_is_attr(node->temp)) \
        { \
            /*fprintf(fp, " %s=\"%s\"", node->temp->name, node->temp->value);*/ \
            length += strlen(node->temp->name) + strlen(node->temp->value) + 4; \
            node->temp = node->temp->next; \
            continue; \
        } \
        break; \
    } \
}

/* 打印节点值长度(注: HTML有层次格式) */
#define html_node_value_length(node, length) \
{ \
    if (html_has_value(node)) \
    { \
        if (html_has_child(node))  /* 此时temp指向node的孩子节点 或 NULL */ \
        { \
            /* fprintf(fp, ">%s\n", node->value); */ \
            length += strlen(node->value) + 2; \
        } \
        else \
        { \
            /* fprintf(fp, ">%s</%s>\n", node->value, node->name); */ \
            length += strlen(node->value) + strlen(node->name) + 5; \
        } \
    } \
    else \
    { \
        if (NULL != node->temp)   /* 此时temp指向node的孩子节点 或 NULL */ \
        { \
            /* fprintf(fp, ">\n"); */ \
            length += 2; \
        } \
        else \
        { \
            /* fprintf(fp, "/>\n"); */ \
            length += 3; \
        } \
    } \
}

/******************************************************************************
 **函数名称: html_node_next_length
 **功    能: 获取下一个要处理的节点，并计算当前结束节点的长度(注: HTML有层次结构)
 **输入参数:
 **     root: HTML树根节点
 **     stack: 栈
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.06.10 #
 ******************************************************************************/
static html_node_t *html_node_next_length(
    html_tree_t *html, Stack_t *stack, html_node_t *node, int *length)
{
    int ret = 0, depth = 0, level = 0, length2 = 0;
    html_node_t *top = NULL, *child = NULL;

    if (NULL != node->temp)      /* 首先: 处理孩子节点: 选出下一个孩子节点 */
    {
        child = node->temp;
        node->temp = child->next;
        node = child;
        return node;
    }
    else                        /* 再次: 处理其兄弟节点: 选出下一个兄弟节点 */
    {
        /* 1. 弹出已经处理完成的节点 */
        top = stack_gettop(stack);
        if (html_has_child(top))
        {
            depth = stack_depth(stack);
            level = depth - 1;
            while (level > 1)
            {
                /* fprintf(fp, "\t"); */
                length2++;
                level--;
            }
            /* fprintf(fp, "</%s>\n", top->name); */
            length2 += strlen(top->name) + 4;
        }
        
        ret = stack_pop(stack);
        if (HTML_OK != ret)
        {
            *length += length2;
            log2_error("Stack pop failed!");
            return NULL;
        }
        
        if (stack_isempty(stack))
        {
            *length += length2;
            log2_error("Compelte fprint!");
            return NULL;
        }
        
        /* 2. 处理其下一个兄弟节点 */
        node = top->next;
        while (NULL == node)     /* 所有兄弟节点已经处理完成，说明父亲节点也处理完成 */
        {
            /* 3. 父亲节点出栈 */
            top = stack_gettop(stack);
            ret = stack_pop(stack);
            if (HTML_OK != ret)
            {
                *length += length2;
                log2_error("Stack pop failed!");
                return NULL;
            }
        
            /* 4. 打印父亲节点结束标志 */
            if (html_has_child(top))
            {
                depth = stack_depth(stack);
                level = depth + 1;
                while (level > 1)
                {
                    /* fprintf(fp, "\t"); */
                    length2++;
                    level--;
                }
                /* fprintf(fp, "</%s>\n", top->name); */
                length2 += strlen(top->name) + 4;
            }
            
            if (stack_isempty(stack))
            {
                *length += length2;
                return NULL;    /* 处理完成 */
            }

            /* 5. 选择父亲的兄弟节点 */
            node = top->next;
        }
    }    

    *length += length2;
    return node;
}

/******************************************************************************
 **函数名称: _html_node_length
 **功    能: 计算节点打印成HTML格式字串时的长度(注: HTML有层次结构)
 **输入参数:
 **     root: HTML树根节点
 **     stack: 栈
 **输出参数:
 **返    回: 节点及其属性、孩子节点的总长度
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.06.10 #
 ******************************************************************************/
int _html_node_length(html_tree_t *html, html_node_t *root, Stack_t *stack)
{
    int ret = 0, depth = 0, length=0;
    html_node_t *node = root;

    depth = stack_depth(stack);
    if (0 != depth)
    {
        log2_error("Stack depth must empty. depth:[%d]", depth);
        return HTML_ERR_STACK;
    }

    do
    {
        /* 1. 将要处理的节点压栈 */
        node->temp = node->firstchild;
        ret = stack_push(stack, node);
        if (HTML_OK != ret)
        {
            log2_error("Stack push failed!");
            return HTML_ERR_STACK;
        }
        
        /* 2. 打印节点名 */
        depth = stack_depth(stack);
        
        html_node_name_length(node, depth, length);
        
        /* 3. 打印属性节点 */
        if (html_has_attr(node))
        {
            html_node_attr_length(node, length);
        }
        
        /* 4. 打印节点值 */
        html_node_value_length(node, length);
        
        /* 5. 选择下一个处理的节点: 从父亲节点、兄弟节点、孩子节点中 */
        node = html_node_next_length(html, stack, node, &length);
        
    }while (NULL != node);

    if (!stack_isempty(stack))
    {
        return HTML_ERR_STACK;
    }
    return length;
}

/******************************************************************************
 **函数名称: html_mark_mismatch_hdl
 **功    能: 修复匹配失败的结点格式
 **输入参数:
 **     html: HTML树
 **     node: 异常结点
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.07 #
 ******************************************************************************/
static int html_mark_mismatch_hdl(html_tree_t *html, html_node_t *node)
{
    html_node_t *attr, *next,
        *child = node->firstchild,
        *parent = node->parent;

    attr = child;
    while (NULL != child)
    {
        /* 1. 查找第一个孩子结点(非属性结点) */
        if (html_is_attr(child))
        {
            attr = child;
            child = child->next;
            continue;
        }

        /* 2. 修改孩子结点的父节点 */
        if (child == node->firstchild)
        {
            node->firstchild = NULL;
        }
        else
        {
            attr->next = NULL;
        }

        next = child;
        while (NULL != next)
        {
            next->parent = parent;
            next = next->next;
        }

        html_unset_child_flag(node);

        /* 3. 将孩子结点插入父节点的孩子列表 */
        if (NULL == node->next)
        {
            node->next = child;
        }
        else
        {
            next = node->next;
            while (NULL != next->next)
            {
                next = next->next;
            }

            next->next = child;
        }

        html_set_child_flag(node->parent);
        return HTML_OK;
    }

    html_unset_child_flag(node);
    return HTML_OK;
}
