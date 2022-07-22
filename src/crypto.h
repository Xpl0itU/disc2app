#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#ifdef __cplusplus
extern "C" {
#endif
int decrypt_aes(unsigned char *ciphertext, int ciphertext_len, unsigned char *key,
                unsigned char *iv, unsigned char *plaintext);
#ifdef __cplusplus
}
#endif
