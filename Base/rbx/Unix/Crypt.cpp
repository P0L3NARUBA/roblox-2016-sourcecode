#include "rbx/Crypt.h"
#include "rbxFormat.h"
#include <boost/scoped_array.hpp>

#include <iostream>
#include <string>
#include <sstream>

#if defined(__APPLE__)
#   include <TargetConditionals.h>
#   if defined(__i386__) && !TARGET_IPHONE_SIMULATOR
#       define RBX_BUILD_CRYPT
#   endif
#elif defined(__ANDROID__)
#   define RBX_BUILD_CRYPT
#else
#   error Unsupported platform.
#endif

#ifdef RBX_BUILD_CRYPT
#   include <openssl/sha.h>
#   include <openssl/bio.h>
#   include <openssl/x509.h>
#   include <openssl/evp.h>
#   include <openssl/pem.h>
#   include <openssl/err.h>
#endif // RBX_BUILD_CRYPT

namespace
{
#ifdef RBX_BUILD_CRYPT
// The stuff between "-----BEGIN PUBLIC KEY-----" is base64 encoded string, the roblox key is also base64 encoded & should work.

// Public/Private Key Pair generated by Ben D from the public/private blobs
char publicKey[] = {
    "-----BEGIN PUBLIC KEY-----\n"
    "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCtfLLFT36v5r9bNP7STBteDU5a\n"
    "0hk1+5rK8KEhXTPQysst6oZGBV8zW6pMB8QNDkX8gxLLI40gM9Vj6rIwnT6Jp+3u\n"
    "AvDu4aamxxVE4mfLHsRz+hW0OlULYdGwWlq6KXQd8zsSbJkBsFXbiy8xIbZPKPA5\n"
    "2YkChMUF0+X0sUxtowIDAQAB\n"
    "-----END PUBLIC KEY-----\n"
};
#endif

//Helper Fn for Base64 Decode, pass out the unsigned char** with the caller responsibility to free out
int unbase64(char *input, unsigned char **out)
{
#ifdef RBX_BUILD_CRYPT
    BIO *b64, *bmem;

    // base64 encoding is longer than the original 'string'
    int length = strlen(input);
    unsigned char *buffer = (unsigned char *)malloc(length);

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    bmem = BIO_new_mem_buf(input, length);
    bmem = BIO_push(b64, bmem);

    length = BIO_read(bmem, buffer, length);
    buffer[length] = 0;

    BIO_free_all(bmem);

    *out = buffer; // caller frees
    return length;
#else
    return 0;
#endif
}

// verify that the plaintext message is matching signature (base64 encoded)
int verifyMessage(char* message, char* sig_b64)
{
#ifdef RBX_BUILD_CRYPT
    SHA_CTX sha_ctx = { 0 };
    unsigned char digest[SHA_DIGEST_LENGTH];
    int rc = 1;

    //SHA1((unsigned char*)message, strlen(message), digest);
    rc = SHA1_Init(&sha_ctx);
    if (1 != rc)
        throw RBX::runtime_error("Error during SHA_Init");

    rc = SHA1_Update(&sha_ctx, (unsigned char*)message, strlen(message));
    if (1 != rc)
        throw RBX::runtime_error("Error during SHA_Update");

    rc = SHA1_Final(digest, &sha_ctx);
    if (1 != rc)
        throw RBX::runtime_error("Error during SHA_Final");

    unsigned char* signature;
    int length = unbase64(sig_b64, &signature);

    // WARNING: no error checking for brevity
    BIO* bio = BIO_new_mem_buf(publicKey, sizeof(publicKey));

    RSA* rsa_key = 0;
    rsa_key = PEM_read_bio_RSA_PUBKEY(bio, NULL, NULL, NULL);

    int ret = RSA_verify(NID_sha1, digest, SHA_DIGEST_LENGTH, signature, length, rsa_key);

    ERR_print_errors_fp(stdout);

    RSA_free(rsa_key);
    BIO_free(bio);
    free(signature);

    return ret;
#else
    return 1;
#endif
}
} // namespace

namespace RBX
{
	Crypt::Crypt()
	{
	}
	
	Crypt::~Crypt()
	{
	}
		
	//Crypt implementation with RSA security using SHA1 algorithm for signature verification.
	void Crypt::verifySignatureBase64(std::string messageStr, std::string signatureStr)
	{
		if (!verifyMessage((char*)messageStr.c_str(), (char*)signatureStr.c_str()))
			throw std::runtime_error("");  // message hidden on purpose - prevent reverse engineering
	}
			
} // namespace RBX
