#include <stdio.h>
#include <stdint.h>

static inline uint32_t ror32(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

static const uint32_t K[64] = {
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
		uint32_t t1 = h + S1 + ch + K[i] + w[i & 15];
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

int main() {
	uint32_t in[16] = {0};
	in[0] = 0x80000000;
	uint32_t out[8];
	sha256_1block(in, out);
	printf("SHA-256(empty): %08x%08x%08x%08x...\n", out[0], out[1], out[2], out[3]);
	return 0;
}
