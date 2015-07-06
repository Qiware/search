#include "md5.h"

int main(void)
{
    md5_ctx_t md5;
    char data[64], digest[MD5_DIGEST_LEN];

    md5_init(&md5);

    md5_update(&md5, (unsigned char *)data, sizeof(data));

    md5_final((unsigned char *)digest, &md5);

    return 0;
}
