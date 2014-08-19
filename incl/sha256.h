/** ######################################
    Sha256 Calculator
-- Relative Module: --
-- Author: Cylock
##########################################
*/

#ifndef __SHA256_H__
#define __SHA256_H__

typedef struct _sha256_t
{ 
	char  Value[ 32 ]; 
	int  DwordBufBytes;
	int  ByteNumLo;
	int  ByteNumHi;
	int  reg[ 8 ]; /** h0 to h 7 -- old value store*/
	int  DwordBuf[ 16 ]; /** data store */
	int  Padding[ 64 ];
}sha256_t;

long sha256_init(sha256_t* sha256);
long sha256_uninit(sha256_t* sha256);
long sha256_reset(sha256_t* sha256);
long sha256_calculate(sha256_t* sha256, char *data, long data_len);

#endif
