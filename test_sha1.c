#include <stdio.h>
#include <stdint.h>
#include "te0_table.h"

static inline uint32_t ror32(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
static inline uint32_t rol32(uint32_t x, uint32_t n) { return (x << n) | (x >> (32 - n)); }

// We can implement 1-block SHA-1 with circular buffer of 16 words
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

int main() {
	// Test on empty message SHA-1: 1 block with 0x80 at in[0], in[1..14]=0, in[15]=0
	uint32_t in[16] = {0};
	in[0] = 0x80000000;
	uint32_t out[5];
	sha1_1block(in, out);
	printf("SHA-1(empty): %08x %08x %08x %08x %08x\n", out[0], out[1], out[2], out[3], out[4]);
	return 0;
}
