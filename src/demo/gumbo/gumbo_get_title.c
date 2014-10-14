#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "gumbo_ex.h"
#include <gumbo.h>

static const char* find_title(const GumboNode* root)
{
    int i;
    GumboNode *head, *child, *title_text;
    const GumboVector *root_children, *head_children;

    assert(GUMBO_NODE_ELEMENT == root->type);
    assert(root->v.element.children.length >= 2);

    root_children = &root->v.element.children;
    head = NULL;
    for (i = 0; i < root_children->length; ++i)
    {
        child = root_children->data[i];
        if (GUMBO_NODE_ELEMENT == child->type
            && GUMBO_TAG_HEAD == child->v.element.tag)
        {
            head = child;
            break;
        }
    }
    assert(head != NULL);

    head_children = &head->v.element.children;
    for (i = 0; i < head_children->length; ++i)
    {
        child = head_children->data[i];
        if (GUMBO_NODE_ELEMENT == child->type
            && GUMBO_TAG_TITLE == child->v.element.tag)
        {
            if (1 != child->v.element.children.length)
            {
                return "<empty title>";
            }

            title_text = child->v.element.children.data[0];
            assert(title_text->type == GUMBO_NODE_TEXT);
            return title_text->v.text.text;
        }
    }
    return "<no title found>";
}

int main(int argc, const char** argv)
{
    int ret;
    gumbo_cntx_t ctx;
    gumbo_html_t *html;
    const char *filename = argv[1], *title;

    if (2 != argc)
    {
        printf("Usage: get_title <html filename>.\n");
        exit(EXIT_FAILURE);
    }

    log2_init("trace", "./gettile.log");

    ret = gumbo_init(&ctx);
    if (0 != ret)
    {
        fprintf(stderr, "Init gumbo failed!");
        return -1;
    }

    html = gumbo_html_parse(&ctx, filename);
    if (NULL == html)
    {
        gumbo_destroy(&ctx);
        fprintf(stderr, "Parse html failed! [%s]", filename);
        return -1;
    }

    title = find_title(html->output->root);
    printf("%s\n", title);

    gumbo_html_destroy(&ctx, html);
    gumbo_destroy(&ctx);
    return 0;
}
