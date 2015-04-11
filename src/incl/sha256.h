#ifndef __SHA256_H__
#define __SHA256_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#define SHA256_DIGEST_LEN   (32)

typedef struct _sha256_t
{ 
    char Value[SHA256_DIGEST_LEN]; 
    int DwordBufBytes;
    int ByteNumLo;
    int ByteNumHi;
    int reg[8]; /* h0 to h 7 -- old value store */
    int DwordBuf[16]; /** data store */
    int Padding[64];
} sha256_t;

uint64_t sha256_init(sha256_t* sha256);
uint64_t sha256_uninit(sha256_t* sha256);
uint64_t sha256_reset(sha256_t* sha256);
uint64_t sha256_calculate(sha256_t* sha256, char *data, uint64_t data_len);

void sha256_digest(char *str, unsigned int len, unsigned char digest[32]);
#endif
