/*
 * DeepSound 2.1+ "DSC2" container key-verification block.
 *
 * key      = SHA256( UTF-16LE(password) )
 * keycheck = SHA1( AES-256-CBC( key = key, iv = key[:16], PKCS7( key ) ) )
 *
 * CBC over an exactly-32-byte plaintext = three ECB blocks:
 *   c0 = E_k(0^16)
 *   c1 = E_k(key[16:32] ^ c0)
 *   c2 = E_k(0x10^16 ^ c1)
 *   keycheck = SHA1(c0 || c1 || c2)
 *
 * Hash file format:
 *   $deepsound2$c3891fa82c0a842db941d5d4656b35f69ecd45f6
 */

#if FMT_EXTERNS_H
extern struct fmt_main fmt_deepsound2;
#elif FMT_REGISTERS_H
john_register_one(&fmt_deepsound2);
#else

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "arch.h"
#include "params.h"
#include "common.h"
#include "formats.h"
#include "sha2.h"
#include <openssl/sha.h>
#include "aes.h"

#define FORMAT_LABEL            "deepsound2"
#define FORMAT_NAME             "DeepSound 2.1+ DSC2 (unicode password)"
#define ALGORITHM_NAME          "AES-256-CBC/SHA-256/SHA-1 32/" ARCH_BITS_STR

#define BENCHMARK_COMMENT       ""
#define BENCHMARK_LENGTH        0

#define PLAINTEXT_LENGTH        64
#define BINARY_SIZE             20
#define BINARY_ALIGN            4
#define SALT_SIZE               0
#define SALT_ALIGN              1

#define MIN_KEYS_PER_CRYPT      1
#define MAX_KEYS_PER_CRYPT      64

#ifndef OMP_SCALE
#define OMP_SCALE               8
#endif

#define TAG                     "$deepsound2$"
#define TAG_LEN                 (sizeof(TAG) - 1)

static struct fmt_tests tests[] = {
	{"$deepsound2$5af80536d41e826be11da7317f250ffaaa0eaf8b", "test"},
	{"$deepsound2$35a42fb91ff63d3bd4b4f318666f252c5d398b4f", "gain"},
	{"$deepsound2$ccb7867e119a42b48fa97b19fb15634fd16e226e", ""},
	{"$deepsound2$5b786f06b42bb9fc452f1c1cf8fb34e9f2a1bc15", "h\xc3\xa1t b\xc3\xa9"},
	{NULL}
};

static char (*saved_key)[PLAINTEXT_LENGTH + 1];
static int (*saved_len);
static uint32_t (*crypt_out)[BINARY_SIZE / 4];

#define COMMON_GET_HASH_VAR crypt_out
#include "common-get-hash.h"

static void init(struct fmt_main *self)
{
	omp_autotune(self, OMP_SCALE);

	saved_key = mem_calloc(self->params.max_keys_per_crypt,
	                       sizeof(*saved_key));
	saved_len = mem_calloc(self->params.max_keys_per_crypt,
	                       sizeof(*saved_len));
	crypt_out = mem_calloc(self->params.max_keys_per_crypt,
	                       sizeof(*crypt_out));
}

static void done(void)
{
	MEM_FREE(crypt_out);
	MEM_FREE(saved_len);
	MEM_FREE(saved_key);
}

static int valid(char *ciphertext, struct fmt_main *self)
{
	int i;

	if (strncmp(ciphertext, TAG, TAG_LEN))
		return 0;
	ciphertext += TAG_LEN;
	if (strlen(ciphertext) != 2 * BINARY_SIZE)
		return 0;
	for (i = 0; i < 2 * BINARY_SIZE; i++)
		if (atoi16[ARCH_INDEX(ciphertext[i])] == 0x7F)
			return 0;
	return 1;
}

static char *split(char *ciphertext, int index, struct fmt_main *self)
{
	static char out[TAG_LEN + 2 * BINARY_SIZE + 1];
	int i;

	strcpy(out, TAG);
	for (i = 0; i < 2 * BINARY_SIZE; i++)
		out[TAG_LEN + i] =
		    itoa16[atoi16[ARCH_INDEX(ciphertext[TAG_LEN + i])]];
	out[TAG_LEN + 2 * BINARY_SIZE] = 0;
	return out;
}

static void *get_binary(char *ciphertext)
{
	static uint32_t out[BINARY_SIZE / 4];
	unsigned char *p = (unsigned char *)out;
	char *c = ciphertext + TAG_LEN;
	int i;

	for (i = 0; i < BINARY_SIZE; i++) {
		p[i] = (atoi16[ARCH_INDEX(c[0])] << 4) | atoi16[ARCH_INDEX(c[1])];
		c += 2;
	}
	return out;
}

static void set_key(char *key, int index)
{
	int n = strlen(key);

	if (n > PLAINTEXT_LENGTH)
		n = PLAINTEXT_LENGTH;
	memcpy(saved_key[index], key, n);
	saved_key[index][n] = 0;
	saved_len[index] = n;
}

static char *get_key(int index)
{
	return saved_key[index];
}

/* UTF-8 -> UTF-16LE.  Input is our own buffer so we can overwrite safely. */
static int utf8_to_utf16le(const unsigned char *in, int inlen, unsigned char *out)
{
	int i = 0, o = 0;

	while (i < inlen) {
		unsigned int cp;
		unsigned char c = in[i];

		if (c < 0x80) {
			cp = c;
			i++;
		} else if ((c & 0xE0) == 0xC0 && i + 1 < inlen) {
			cp = ((c & 0x1F) << 6) | (in[i + 1] & 0x3F);
			i += 2;
		} else if ((c & 0xF0) == 0xE0 && i + 2 < inlen) {
			cp = ((c & 0x0F) << 12) | ((in[i + 1] & 0x3F) << 6) |
			     (in[i + 2] & 0x3F);
			i += 3;
		} else if ((c & 0xF8) == 0xF0 && i + 3 < inlen) {
			cp = ((c & 0x07) << 18) | ((in[i + 1] & 0x3F) << 12) |
			     ((in[i + 2] & 0x3F) << 6) | (in[i + 3] & 0x3F);
			i += 4;
		} else {
			cp = c;
			i++;
		}

		if (cp >= 0x10000) {
			cp -= 0x10000;
			out[o++] = 0x00 + ((cp >> 10) & 0xFF);
			out[o++] = 0xD8 + ((cp >> 18) & 0x03);
			out[o++] = 0x00 + (cp & 0xFF);
			out[o++] = 0xDC + ((cp >> 8) & 0x03);
		} else {
			out[o++] = cp & 0xFF;
			out[o++] = (cp >> 8) & 0xFF;
		}
	}
	return o;
}

static int crypt_all(int *pcount, struct db_salt *salt)
{
	const int count = *pcount;
	int index;

#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (index = 0; index < count; index++) {
		unsigned char u16[2 * PLAINTEXT_LENGTH + 8];
		unsigned char key32[32], blk[16], c0[16], c1[16], c2[16];
		int n;
		AES_KEY akey;
		SHA_CTX sctx;
		SHA256_CTX s256;

		n = utf8_to_utf16le((const unsigned char *)saved_key[index],
		                    saved_len[index], u16);
		SHA256_Init(&s256);
		SHA256_Update(&s256, u16, (unsigned int)n);
		SHA256_Final(key32, &s256);

		AES_set_encrypt_key(key32, 256, &akey);

		memset(blk, 0, 16);
		AES_encrypt(blk, c0, &akey);

		{
			int i;
			for (i = 0; i < 16; i++)
				blk[i] = key32[16 + i] ^ c0[i];
		}
		AES_encrypt(blk, c1, &akey);

		memset(blk, 0x10, 16);
		{
			int i;
			for (i = 0; i < 16; i++)
				blk[i] ^= c1[i];
		}
		AES_encrypt(blk, c2, &akey);

		SHA1_Init(&sctx);
		SHA1_Update(&sctx, c0, 16);
		SHA1_Update(&sctx, c1, 16);
		SHA1_Update(&sctx, c2, 16);
		SHA1_Final((unsigned char *)crypt_out[index], &sctx);
	}

	return count;
}

static int cmp_all(void *binary, int count)
{
	int index;

	for (index = 0; index < count; index++)
		if (!memcmp(binary, crypt_out[index], BINARY_SIZE))
			return 1;
	return 0;
}

static int cmp_one(void *binary, int index)
{
	return !memcmp(binary, crypt_out[index], BINARY_SIZE);
}

static int cmp_exact(char *source, int index)
{
	return 1;
}

struct fmt_main fmt_deepsound2 = {
	{
		FORMAT_LABEL,
		FORMAT_NAME,
		ALGORITHM_NAME,
		BENCHMARK_COMMENT,
		BENCHMARK_LENGTH,
		0,
		PLAINTEXT_LENGTH,
		BINARY_SIZE,
		BINARY_ALIGN,
		SALT_SIZE,
		SALT_ALIGN,
		MIN_KEYS_PER_CRYPT,
		MAX_KEYS_PER_CRYPT,
		FMT_CASE | FMT_8_BIT | FMT_OMP | FMT_SPLIT_UNIFIES_CASE,
		{ NULL },
		{ TAG },
		tests
	}, {
		init,
		done,
		fmt_default_reset,
		fmt_default_prepare,
		valid,
		split,
		get_binary,
		fmt_default_salt,
		{ NULL },
		fmt_default_source,
		{
			fmt_default_binary_hash_0,
			fmt_default_binary_hash_1,
			fmt_default_binary_hash_2,
			fmt_default_binary_hash_3,
			fmt_default_binary_hash_4,
			fmt_default_binary_hash_5,
			fmt_default_binary_hash_6
		},
		fmt_default_salt_hash,
		NULL,
		fmt_default_set_salt,
		set_key,
		get_key,
		fmt_default_clear_keys,
		crypt_all,
		{
#define COMMON_GET_HASH_LINK
#include "common-get-hash.h"
		},
		cmp_all,
		cmp_one,
		cmp_exact
	}
};

#endif /* plugin stanza */
