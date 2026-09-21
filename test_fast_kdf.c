#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "te0_table.h"

static inline uint32_t ror32(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
static inline uint32_t rol32(uint32_t x, uint32_t n) { return (x << n) | (x >> (32 - n)); }

static const uint8_t sbox[256] = {
	0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
	0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
	0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
	0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
	0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
	0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
	0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
	0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
	0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
	0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
	0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
	0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
	0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
	0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
	0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
	0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static const uint32_t K256[64] = {
	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

void sha256_1block(const uint32_t in[16], uint32_t out[8]) {
	uint32_t w[16];
	for (int i = 0; i < 16; i++) w[i] = in[i];
	uint32_t a = 0x6a09e667, b = 0xbb67ae85, c = 0x3c6ef372, d = 0xa54ff53a;
	uint32_t e = 0x510e527f, f = 0x9b05688c, g = 0x1f83d9ab, h = 0x5be0cd19;
	for (int i = 0; i < 64; i++) {
		if (i >= 16) {
			uint32_t s0 = ror32(w[(i-15) & 15], 7) ^ ror32(w[(i-15) & 15], 18) ^ (w[(i-15) & 15] >> 3);
			uint32_t s1 = ror32(w[(i-2) & 15], 17) ^ ror32(w[(i-2) & 15], 19) ^ (w[(i-2) & 15] >> 10);
			w[i & 15] = w[(i-16) & 15] + s0 + w[(i-7) & 15] + s1;
		}
		uint32_t S1 = ror32(e, 6) ^ ror32(e, 11) ^ ror32(e, 25);
		uint32_t ch = (e & f) ^ ((~e) & g);
		uint32_t t1 = h + S1 + ch + K256[i] + w[i & 15];
		uint32_t S0 = ror32(a, 2) ^ ror32(a, 13) ^ ror32(a, 22);
		uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
		uint32_t t2 = S0 + maj;
		h = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
	}
	out[0] = 0x6a09e667 + a;
	out[1] = 0xbb67ae85 + b;
	out[2] = 0x3c6ef372 + c;
	out[3] = 0xa54ff53a + d;
	out[4] = 0x510e527f + e;
	out[5] = 0x9b05688c + f;
	out[6] = 0x1f83d9ab + g;
	out[7] = 0x5be0cd19 + h;
}

void aes256_setkey(const uint32_t key[8], uint32_t rk[60]) {
	int i;
	for (i = 0; i < 8; i++) rk[i] = key[i];
	for (i = 8; i < 60; i++) {
		uint32_t t = rk[i-1];
		if (i % 8 == 0) {
			t = (t << 8) | (t >> 24);
			t = ((uint32_t)sbox[(t >> 24) & 0xff] << 24) |
			    ((uint32_t)sbox[(t >> 16) & 0xff] << 16) |
			    ((uint32_t)sbox[(t >> 8) & 0xff] << 8) |
			    (uint32_t)sbox[t & 0xff];
			switch (i / 8) {
			case 1: t ^= 0x01000000; break;
			case 2: t ^= 0x02000000; break;
			case 3: t ^= 0x04000000; break;
			case 4: t ^= 0x08000000; break;
			case 5: t ^= 0x10000000; break;
			case 6: t ^= 0x20000000; break;
			case 7: t ^= 0x40000000; break;
			}
		} else if (i % 8 == 4) {
			t = ((uint32_t)sbox[(t >> 24) & 0xff] << 24) |
			    ((uint32_t)sbox[(t >> 16) & 0xff] << 16) |
			    ((uint32_t)sbox[(t >> 8) & 0xff] << 8) |
			    (uint32_t)sbox[t & 0xff];
		}
		rk[i] = rk[i-8] ^ t;
	}
}

void aes256_encrypt_fast(const uint32_t rk[60], uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3,
                         uint32_t *o0, uint32_t *o1, uint32_t *o2, uint32_t *o3) {
	s0 ^= rk[0]; s1 ^= rk[1]; s2 ^= rk[2]; s3 ^= rk[3];
	for (int r = 1; r < 14; r++) {
		uint32_t a0 = s0, a1 = s1, a2 = s2, a3 = s3;
		s0 = Te0[a0 >> 24] ^ ror32(Te0[(a1 >> 16) & 0xff], 8) ^ ror32(Te0[(a2 >> 8) & 0xff], 16) ^ ror32(Te0[a3 & 0xff], 24) ^ rk[r*4 + 0];
		s1 = Te0[a1 >> 24] ^ ror32(Te0[(a2 >> 16) & 0xff], 8) ^ ror32(Te0[(a3 >> 8) & 0xff], 16) ^ ror32(Te0[a0 & 0xff], 24) ^ rk[r*4 + 1];
		s2 = Te0[a2 >> 24] ^ ror32(Te0[(a3 >> 16) & 0xff], 8) ^ ror32(Te0[(a0 >> 8) & 0xff], 16) ^ ror32(Te0[a1 & 0xff], 24) ^ rk[r*4 + 2];
		s3 = Te0[a3 >> 24] ^ ror32(Te0[(a0 >> 16) & 0xff], 8) ^ ror32(Te0[(a1 >> 8) & 0xff], 16) ^ ror32(Te0[a2 & 0xff], 24) ^ rk[r*4 + 3];
	}
	uint32_t a0 = s0, a1 = s1, a2 = s2, a3 = s3;
	*o0 = (((uint32_t)sbox[a0 >> 24] << 24) | ((uint32_t)sbox[(a1 >> 16) & 0xff] << 16) | ((uint32_t)sbox[(a2 >> 8) & 0xff] << 8) | (uint32_t)sbox[a3 & 0xff]) ^ rk[56];
	*o1 = (((uint32_t)sbox[a1 >> 24] << 24) | ((uint32_t)sbox[(a2 >> 16) & 0xff] << 16) | ((uint32_t)sbox[(a3 >> 8) & 0xff] << 8) | (uint32_t)sbox[a0 & 0xff]) ^ rk[57];
	*o2 = (((uint32_t)sbox[a2 >> 24] << 24) | ((uint32_t)sbox[(a3 >> 16) & 0xff] << 16) | ((uint32_t)sbox[(a0 >> 8) & 0xff] << 8) | (uint32_t)sbox[a1 & 0xff]) ^ rk[58];
	*o3 = (((uint32_t)sbox[a3 >> 24] << 24) | ((uint32_t)sbox[(a0 >> 16) & 0xff] << 16) | ((uint32_t)sbox[(a1 >> 8) & 0xff] << 8) | (uint32_t)sbox[a2 & 0xff]) ^ rk[59];
}

void sha1_1block(const uint32_t in[16], uint32_t out[5]) {
	uint32_t w[16];
	for (int i = 0; i < 16; i++) w[i] = in[i];
	uint32_t a = 0x67452301, b = 0xefcdab89, c = 0x98badcfe, d = 0x10325476, e = 0xc3d2e1f0;
	for (int i = 0; i < 80; i++) {
		if (i >= 16) {
			w[i & 15] = rol32(w[(i - 3) & 15] ^ w[(i - 8) & 15] ^ w[(i - 14) & 15] ^ w[(i - 16) & 15], 1);
		}
		uint32_t f, k;
		if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5a827999; }
		else if (i < 40) { f = b ^ c ^ d; k = 0x6ed9eba1; }
		else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdc; }
		else { f = b ^ c ^ d; k = 0xca62c1d6; }
		uint32_t t = rol32(a, 5) + f + e + k + w[i & 15];
		e = d; d = c; c = rol32(b, 30); b = a; a = t;
	}
	out[0] = 0x67452301 + a;
	out[1] = 0xefcdab89 + b;
	out[2] = 0x98badcfe + c;
	out[3] = 0x10325476 + d;
	out[4] = 0xc3d2e1f0 + e;
}

void deepsound_kdf_fast(const uint32_t u16_words[16], uint32_t out[5]) {
	uint32_t key[8];
	sha256_1block(u16_words, key);

	uint32_t rk[60];
	aes256_setkey(key, rk);

	uint32_t c0_0, c0_1, c0_2, c0_3;
	uint32_t c1_0, c1_1, c1_2, c1_3;
	uint32_t c2_0, c2_1, c2_2, c2_3;

	// Block 0: input is 0
	aes256_encrypt_fast(rk, 0, 0, 0, 0, &c0_0, &c0_1, &c0_2, &c0_3);

	// Block 1: input is key[4..7] ^ c0
	aes256_encrypt_fast(rk, key[4] ^ c0_0, key[5] ^ c0_1, key[6] ^ c0_2, key[7] ^ c0_3,
	                    &c1_0, &c1_1, &c1_2, &c1_3);

	// Block 2: input is 0x10101010 ^ c1
	aes256_encrypt_fast(rk, 0x10101010 ^ c1_0, 0x10101010 ^ c1_1, 0x10101010 ^ c1_2, 0x10101010 ^ c1_3,
	                    &c2_0, &c2_1, &c2_2, &c2_3);

	// SHA-1 input
	uint32_t sha1_in[16];
	sha1_in[0] = c0_0; sha1_in[1] = c0_1; sha1_in[2] = c0_2; sha1_in[3] = c0_3;
	sha1_in[4] = c1_0; sha1_in[5] = c1_1; sha1_in[6] = c1_2; sha1_in[7] = c1_3;
	sha1_in[8] = c2_0; sha1_in[9] = c2_1; sha1_in[10] = c2_2; sha1_in[11] = c2_3;
	sha1_in[12] = 0x80000000;
	sha1_in[13] = 0;
	sha1_in[14] = 0;
	sha1_in[15] = 0x00000180; // 48 * 8 = 384 = 0x180

	sha1_1block(sha1_in, out);
}

int main() {
	// Test "test": UTF-16LE is 0x74, 0x00, 0x65, 0x00, 0x73, 0x00, 0x74, 0x00 (8 bytes)
	// In SHA-256 big-endian words:
	// w[0] = (0x74 << 24) | (0x00 << 16) | (0x65 << 8) | 0x00 = 0x74006500
	// w[1] = (0x73 << 24) | (0x00 << 16) | (0x74 << 8) | 0x00 = 0x73007400
	// w[2] = 0x80000000 (padding)
	// w[3..14] = 0
	// w[15] = 8 * 8 = 64 = 0x40
	uint32_t in[16] = {0};
	in[0] = 0x74006500;
	in[1] = 0x73007400;
	in[2] = 0x80000000;
	in[15] = 64;

	uint32_t out[5];
	deepsound_kdf_fast(in, out);
	printf("test -> %08x%08x%08x%08x%08x\n", out[0], out[1], out[2], out[3], out[4]);
	printf("exp  -> 5af80536d41e826be11da7317f250ffaaa0eaf8b\n");
	return 0;
}
