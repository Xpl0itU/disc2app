#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/err.h>

int decrypt_aes(unsigned char *ciphertext, int ciphertext_len, unsigned char *key,
            unsigned char *iv, unsigned char *plaintext);