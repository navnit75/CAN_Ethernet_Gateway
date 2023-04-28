#include "encrypt.h"
#include "common.h"
#include "utils_cmac.h"

unsigned char* ecb_encrypt(unsigned char* in, unsigned char* key, unsigned char* (*some_encrypt)(unsigned char* in, unsigned char* out, unsigned char* key), unsigned int* n, unsigned int len)
{
   // unsigned int length;
    bool flag = false;
   // length = strlen((char*)in) + 1;
    *n = len / 16;
    if (len % 16 != 0) {
        *n = *n + 1;
        flag = true;
    } else if (*n == 0) {
        *n = 1;
        flag = true;
    }

    unsigned char M[*n][16];
    memset(M[0], 0x00, (*n) * 16);
    memcpy(M[0], in, len);
    unsigned char* C;
    C = (unsigned char*)malloc((*n) * 16);

    for (auto i = 0; i < *n; i++) {
        some_encrypt(M[i], C + 16 * i, key);
    }

    return C;
}

unsigned char* ecb_decrypt(unsigned char* in, unsigned char* key, unsigned char* (*some_decrypt)(unsigned char* in, unsigned char* out, unsigned char* key), unsigned int* n)
{
    unsigned char* out;
    out = (unsigned char*)malloc((*n) * 16);

    for (auto i = 0; i < *n; i++) {
        some_decrypt(in + i * 16, out + i * 16, key);
    }

    return out;
}
