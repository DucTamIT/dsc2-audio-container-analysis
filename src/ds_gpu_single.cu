/*
 * DeepSound DSC2 GPU search tool -- CUDA version (for machines without OpenCL).
 *
 *   nvcc -O3 -o ds_gpu.exe ds_gpu.cu
 *
 * Same CLI as the OpenCL build:
 *   ds_gpu --list
 *   ds_gpu --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --wordlist rockyou.txt
 *   ds_gpu --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --mask abcdefghijklmnopqrstuvwxyz0123456789 8
 *   ds_gpu --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --prefix 'CTF{' --suffix '}' --mask abcdefghijklmnopqrstuvwxyz0123456789_ 7
 *
 * Exit code 0 = found, 2 = space exhausted.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cuda_runtime.h>

typedef unsigned char u8;
typedef unsigned int  u32;
typedef unsigned long long u64;

#define MAXPW      64
#define RECLEN     (2 * MAXPW)
#define MAXPOS     32
#define MAXAFFIX   (2 * MAXPW)
#define DS_MAX_UNITS 64

#define CUDA_OK(x) do { cudaError_t e_ = (x); if (e_ != cudaSuccess) { \
	fprintf(stderr, "CUDA error %s at %s:%d\n", cudaGetErrorString(e_), __FILE__, __LINE__); exit(1); } } while (0)

/* ---------------- device-side KDF (same source as the OpenCL build) -------- */

/* ---- ds_kdf.h inlined ---- */
/*
 * DeepSound DSC2 key-check KDF, written so the same source compiles both as
 * plain C (for CPU unit tests) and as OpenCL C (for the GPU kernel).
 *
 *   key      = SHA256( UTF-16LE(password) )
 *   c0       = AES-256_k( 00*16 )
 *   c1       = AES-256_k( key[16:32] ^ c0 )
 *   c2       = AES-256_k( 10*16 ^ c1 )            (PKCS7 block)
 *   keycheck = SHA1( c0 || c1 || c2 )
 */

#ifdef __OPENCL_VERSION__
typedef uchar ds_u8;
typedef uint  ds_u32;
typedef ulong ds_u64;
#define DS_CONST __constant
#define DS_FN static inline
#elif defined(__CUDACC__)
typedef unsigned char ds_u8;
typedef unsigned int  ds_u32;
typedef unsigned long long ds_u64;
#define DS_CONST __constant__
#define DS_FN static __device__ __forceinline__
#else
#include <stdint.h>
typedef uint8_t  ds_u8;
typedef uint32_t ds_u32;
typedef uint64_t ds_u64;
#define DS_CONST static const
#define DS_FN static inline
#endif

#define DS_MAX_UTF16_BYTES 128
#define DS_MAX_UNITS       64   /* DS_MAX_UTF16_BYTES / 2 */

/* ---------------- SHA-256 ---------------- */

DS_CONST ds_u32 ds_sha256_k[64] = {
	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

DS_FN ds_u32 ds_ror32(ds_u32 x, ds_u32 n) { return (x >> n) | (x << (32 - n)); }
DS_FN ds_u32 ds_rol32(ds_u32 x, ds_u32 n) { return (x << n) | (x >> (32 - n)); }

DS_FN void ds_sha256_block(ds_u32 st[8], const ds_u8 *p)
{
	ds_u32 w[64], a, b, c, d, e, f, g, h, t1, t2;
	int i;

	for (i = 0; i < 16; i++)
		w[i] = ((ds_u32)p[i*4] << 24) | ((ds_u32)p[i*4+1] << 16) |
		       ((ds_u32)p[i*4+2] << 8) | (ds_u32)p[i*4+3];
	for (i = 16; i < 64; i++) {
		ds_u32 s0 = ds_ror32(w[i-15], 7) ^ ds_ror32(w[i-15], 18) ^ (w[i-15] >> 3);
		ds_u32 s1 = ds_ror32(w[i-2], 17) ^ ds_ror32(w[i-2], 19) ^ (w[i-2] >> 10);
		w[i] = w[i-16] + s0 + w[i-7] + s1;
	}
	a = st[0]; b = st[1]; c = st[2]; d = st[3];
	e = st[4]; f = st[5]; g = st[6]; h = st[7];

	for (i = 0; i < 64; i++) {
		t1 = h + (ds_ror32(e,6) ^ ds_ror32(e,11) ^ ds_ror32(e,25)) +
		     ((e & f) ^ (~e & g)) + ds_sha256_k[i] + w[i];
		t2 = (ds_ror32(a,2) ^ ds_ror32(a,13) ^ ds_ror32(a,22)) +
		     ((a & b) ^ (a & c) ^ (b & c));
		h = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
	}
	st[0] += a; st[1] += b; st[2] += c; st[3] += d;
	st[4] += e; st[5] += f; st[6] += g; st[7] += h;
}

DS_FN void ds_sha256(const ds_u8 *msg, ds_u32 len, ds_u8 out[32])
{
	ds_u32 st[8] = { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
	                 0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 };
	ds_u8 buf[DS_MAX_UTF16_BYTES + 72];
	ds_u32 total, i;
	ds_u64 bits;

	for (i = 0; i < len; i++)
		buf[i] = msg[i];
	buf[len] = 0x80;
	total = ((len + 8) / 64 + 1) * 64;
	for (i = len + 1; i < total; i++)
		buf[i] = 0;
	bits = (ds_u64)len * 8;
	for (i = 0; i < 8; i++)
		buf[total - 8 + i] = (ds_u8)(bits >> (56 - 8 * i));

	for (i = 0; i < total; i += 64)
		ds_sha256_block(st, buf + i);

	for (i = 0; i < 8; i++) {
		out[i*4]   = (ds_u8)(st[i] >> 24);
		out[i*4+1] = (ds_u8)(st[i] >> 16);
		out[i*4+2] = (ds_u8)(st[i] >> 8);
		out[i*4+3] = (ds_u8)st[i];
	}
}

/* ---------------- AES-256 (encrypt only) ---------------- */

DS_CONST ds_u8 ds_aes_sbox[256] = {
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

DS_FN ds_u8 ds_aes_xtime(ds_u8 x) { return (ds_u8)((x << 1) ^ ((x >> 7) * 0x1b)); }

DS_FN void ds_aes256_setkey(const ds_u8 key[32], ds_u32 rk[60])
{
	int i;
	for (i = 0; i < 8; i++)
		rk[i] = ((ds_u32)key[4*i] << 24) | ((ds_u32)key[4*i+1] << 16) |
		        ((ds_u32)key[4*i+2] << 8) | (ds_u32)key[4*i+3];
	for (i = 8; i < 60; i++) {
		ds_u32 t = rk[i-1];
		if (i % 8 == 0) {
			t = (t << 8) | (t >> 24);
			t = ((ds_u32)ds_aes_sbox[(t >> 24) & 0xff] << 24) |
			    ((ds_u32)ds_aes_sbox[(t >> 16) & 0xff] << 16) |
			    ((ds_u32)ds_aes_sbox[(t >> 8) & 0xff] << 8) |
			    (ds_u32)ds_aes_sbox[t & 0xff];
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
			t = ((ds_u32)ds_aes_sbox[(t >> 24) & 0xff] << 24) |
			    ((ds_u32)ds_aes_sbox[(t >> 16) & 0xff] << 16) |
			    ((ds_u32)ds_aes_sbox[(t >> 8) & 0xff] << 8) |
			    (ds_u32)ds_aes_sbox[t & 0xff];
		}
		rk[i] = rk[i-8] ^ t;
	}
}

DS_FN void ds_aes256_encrypt(const ds_u32 rk[60], const ds_u8 in[16], ds_u8 out[16])
{
	ds_u8 s[16], t[16];
	int i, r;

	for (i = 0; i < 16; i++)
		s[i] = in[i];

	for (r = 0; r <= 14; r++) {
		if (r > 0) {
			/* SubBytes */
			for (i = 0; i < 16; i++)
				s[i] = ds_aes_sbox[s[i]];
			/* ShiftRows (column-major state: s[c*4+r]) */
			for (i = 0; i < 4; i++) {
				t[i*4+0] = s[((i+0)%4)*4 + 0];
				t[i*4+1] = s[((i+1)%4)*4 + 1];
				t[i*4+2] = s[((i+2)%4)*4 + 2];
				t[i*4+3] = s[((i+3)%4)*4 + 3];
			}
			for (i = 0; i < 16; i++)
				s[i] = t[i];
		}
		if (r > 0 && r < 14) {
			/* MixColumns */
			for (i = 0; i < 4; i++) {
				ds_u8 a0 = s[i*4+0], a1 = s[i*4+1], a2 = s[i*4+2], a3 = s[i*4+3];
				ds_u8 b0 = (ds_u8)(ds_aes_xtime(a0) ^ ds_aes_xtime(a1) ^ a1 ^ a2 ^ a3);
				ds_u8 b1 = (ds_u8)(a0 ^ ds_aes_xtime(a1) ^ ds_aes_xtime(a2) ^ a2 ^ a3);
				ds_u8 b2 = (ds_u8)(a0 ^ a1 ^ ds_aes_xtime(a2) ^ ds_aes_xtime(a3) ^ a3);
				ds_u8 b3 = (ds_u8)(ds_aes_xtime(a0) ^ a0 ^ a1 ^ a2 ^ ds_aes_xtime(a3));
				s[i*4+0] = b0; s[i*4+1] = b1; s[i*4+2] = b2; s[i*4+3] = b3;
			}
		}
		/* AddRoundKey */
		for (i = 0; i < 4; i++) {
			ds_u32 k = rk[r*4 + i];
			s[i*4+0] ^= (ds_u8)(k >> 24);
			s[i*4+1] ^= (ds_u8)(k >> 16);
			s[i*4+2] ^= (ds_u8)(k >> 8);
			s[i*4+3] ^= (ds_u8)k;
		}
	}
	for (i = 0; i < 16; i++)
		out[i] = s[i];
}

/* ---------------- SHA-1 ---------------- */

DS_FN void ds_sha1_block(ds_u32 st[5], const ds_u8 *p)
{
	ds_u32 w[80], a, b, c, d, e, t;
	int i;

	for (i = 0; i < 16; i++)
		w[i] = ((ds_u32)p[i*4] << 24) | ((ds_u32)p[i*4+1] << 16) |
		       ((ds_u32)p[i*4+2] << 8) | (ds_u32)p[i*4+3];
	for (i = 16; i < 80; i++)
		w[i] = ds_rol32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

	a = st[0]; b = st[1]; c = st[2]; d = st[3]; e = st[4];
	for (i = 0; i < 80; i++) {
		ds_u32 f, k;
		if (i < 20)      { f = (b & c) | (~b & d);          k = 0x5a827999; }
		else if (i < 40) { f = b ^ c ^ d;                   k = 0x6ed9eba1; }
		else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdc; }
		else             { f = b ^ c ^ d;                   k = 0xca62c1d6; }
		t = ds_rol32(a, 5) + f + e + k + w[i];
		e = d; d = c; c = ds_rol32(b, 30); b = a; a = t;
	}
	st[0] += a; st[1] += b; st[2] += c; st[3] += d; st[4] += e;
}

DS_FN void ds_sha1(const ds_u8 *msg, ds_u32 len, ds_u8 out[20])
{
	ds_u32 st[5] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 };
	ds_u8 buf[192];
	ds_u32 total, i;
	ds_u64 bits;

	for (i = 0; i < len; i++)
		buf[i] = msg[i];
	buf[len] = 0x80;
	total = ((len + 8) / 64 + 1) * 64;
	for (i = len + 1; i < total; i++)
		buf[i] = 0;
	bits = (ds_u64)len * 8;
	for (i = 0; i < 8; i++)
		buf[total - 8 + i] = (ds_u8)(bits >> (56 - 8 * i));

	for (i = 0; i < total; i += 64)
		ds_sha1_block(st, buf + i);

	for (i = 0; i < 5; i++) {
		out[i*4]   = (ds_u8)(st[i] >> 24);
		out[i*4+1] = (ds_u8)(st[i] >> 16);
		out[i*4+2] = (ds_u8)(st[i] >> 8);
		out[i*4+3] = (ds_u8)st[i];
	}
}

/* ---------------- the DeepSound DSC2 KDF ---------------- */

/* u16 = UTF-16LE password bytes, len = byte count (0 .. 128) */
DS_FN void ds_deepsound_kdf(const ds_u8 *u16, ds_u32 len, ds_u8 out[20])
{
	ds_u8 key[32], blk[16], c0[16], c1[16], c2[16], cat[48];
	ds_u32 rk[60];
	int i;

	ds_sha256(u16, len, key);
	ds_aes256_setkey(key, rk);

	for (i = 0; i < 16; i++)
		blk[i] = 0;
	ds_aes256_encrypt(rk, blk, c0);

	for (i = 0; i < 16; i++)
		blk[i] = key[16 + i] ^ c0[i];
	ds_aes256_encrypt(rk, blk, c1);

	for (i = 0; i < 16; i++)
		blk[i] = 0x10 ^ c1[i];
	ds_aes256_encrypt(rk, blk, c2);

	for (i = 0; i < 16; i++) {
		cat[i] = c0[i]; cat[16 + i] = c1[i]; cat[32 + i] = c2[i];
	}
	ds_sha1(cat, 48, out);
}

/* ---- end ds_kdf.h ---- */

__global__ void ds_check_word_k(const u8 *pwbuf, const u32 *pwlen,
                                const u8 *target, u8 *hits, u32 n)
{
	u32 gid = blockIdx.x * blockDim.x + threadIdx.x;
	u8 u16[DS_MAX_UTF16_BYTES];
	u8 out[20];
	u32 len, i;
	u8 m = 1;

	if (gid >= n) return;
	len = pwlen[gid];
	for (i = 0; i < len; i++) u16[i] = pwbuf[(size_t)gid * DS_MAX_UTF16_BYTES + i];
	ds_deepsound_kdf(u16, len, out);
	for (i = 0; i < 20; i++) if (out[i] != target[i]) { m = 0; break; }
	hits[gid] = m;
}

__global__ void ds_check_pos_k(const u8 *csbuf, const u32 *csoff, const u32 *cslen,
                               u32 npos, const u8 *prefix, u32 plen,
                               const u8 *suffix, u32 slen,
                               const u8 *target, u8 *hits, u64 base, u32 n)
{
	u32 gid = blockIdx.x * blockDim.x + threadIdx.x;
	u8 u16[DS_MAX_UNITS * 2];
	u8 out[20];
	u64 v;
	u32 o = 0, i, c, p;
	u8 m = 1;

	if (gid >= n) return;
	v = base + (u64)gid;

	for (i = 0; i < plen; i++) u16[o++] = prefix[i];
	for (i = 0; i < npos; i++) {
		u32 r = cslen[i];
		c = (u32)(v % (u64)r);
		v /= (u64)r;
		p = npos - 1 - i;
		u16[o + 2 * p] = csbuf[csoff[p] + c];
		u16[o + 2 * p + 1] = 0;
	}
	o += npos * 2;
	for (i = 0; i < slen; i++) u16[o++] = suffix[i];

	ds_deepsound_kdf(u16, o, out);
	for (i = 0; i < 20; i++) if (out[i] != target[i]) { m = 0; break; }
	hits[gid] = m;
}

/* ---------------- host ---------------- */

static void die(const char *s) { fprintf(stderr, "error: %s\n", s); exit(1); }

static void hex2bin(const char *hex, u8 *out, int n)
{
	int i;
	for (i = 0; i < n; i++) { unsigned v; if (sscanf(hex + 2*i, "%2x", &v) != 1) die("bad --hash"); out[i] = (u8)v; }
}

static int utf8_to_utf16le(const unsigned char *in, int inlen, unsigned char *out, int maxb)
{
	int i = 0, o = 0;
	while (i < inlen && o + 2 <= maxb) {
		unsigned cp; unsigned char c = in[i];
		if (c < 0x80) { cp = c; i++; }
		else if ((c & 0xE0) == 0xC0 && i + 1 < inlen) { cp = ((c & 0x1F) << 6) | (in[i+1] & 0x3F); i += 2; }
		else if ((c & 0xF0) == 0xE0 && i + 2 < inlen) { cp = ((c & 0x0F) << 12) | ((in[i+1] & 0x3F) << 6) | (in[i+2] & 0x3F); i += 3; }
		else if ((c & 0xF8) == 0xF0 && i + 3 < inlen) { cp = ((c & 0x07) << 18) | ((in[i+1] & 0x3F) << 12) | ((in[i+2] & 0x3F) << 6) | (in[i+3] & 0x3F); i += 4; }
		else { cp = c; i++; }
		if (cp >= 0x10000) {
			unsigned v = cp - 0x10000;
			if (o + 4 > maxb) break;
			out[o++] = 0xD8 + (v >> 18); out[o++] = (v >> 10) & 0xFF;
			out[o++] = 0xDC + ((v >> 8) & 3); out[o++] = v & 0xFF;
		} else { out[o++] = cp & 0xFF; out[o++] = (cp >> 8) & 0xFF; }
	}
	return o;
}

static double now_sec(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec*1e-9; }

int main(int argc, char **argv)
{
	const char *hash = NULL, *wlpath = NULL, *prefix = "", *suffix = "";
	const char *pos[MAXPOS]; int npos = 0;
	int list_only = 0, dev_idx = 0;
	size_t batch = (size_t)1 << 22;
	u8 target[20];
	int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--hash") && i+1 < argc) hash = argv[++i];
		else if (!strcmp(argv[i], "--wordlist") && i+1 < argc) wlpath = argv[++i];
		else if (!strcmp(argv[i], "--pos") && i+1 < argc) { if (npos >= MAXPOS) die("too many --pos"); pos[npos++] = argv[++i]; }
		else if (!strcmp(argv[i], "--mask") && i+2 < argc) {
			const char *cs = argv[++i]; int L = atoi(argv[++i]);
			if (L <= 0 || L > MAXPOS) die("bad --mask length");
			for (int k = 0; k < L; k++) pos[npos++] = cs;
		}
		else if (!strcmp(argv[i], "--printable") && i+1 < argc) {
			static char pr[96]; int k;
			for (k = 0; k < 95; k++) pr[k] = (char)(32 + k);
			pr[95] = 0;
			int L = atoi(argv[++i]);
			if (L <= 0 || L > MAXPOS) die("bad --printable length");
			for (int j = 0; j < L; j++) { if (npos >= MAXPOS) die("too many positions"); pos[npos++] = pr; }
		}
		else if (!strcmp(argv[i], "--prefix") && i+1 < argc) prefix = argv[++i];
		else if (!strcmp(argv[i], "--suffix") && i+1 < argc) suffix = argv[++i];
		else if (!strcmp(argv[i], "--batch") && i+1 < argc) batch = (size_t)strtoull(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "--device") && i+1 < argc) dev_idx = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--list")) list_only = 1;
		else { fprintf(stderr, "unknown/incomplete option: %s\n", argv[i]); return 1; }
	}
	if (list_only) {
		int n = 0; CUDA_OK(cudaGetDeviceCount(&n));
		for (i = 0; i < n; i++) {
			cudaDeviceProp p; CUDA_OK(cudaGetDeviceProperties(&p, i));
			printf("  [%d] %s (%d MB, sm_%d%d)\n", i, p.name,
			       (int)(p.totalGlobalMem >> 20), p.major, p.minor);
		}
		return 0;
	}
	if (!hash || (!wlpath && !npos)) {
		fprintf(stderr, "usage: %s --hash <40hex> (--wordlist FILE | --mask CS LEN | --pos CS ...) [--prefix S] [--suffix S] [--batch N] [--device N] [--list]\n", argv[0]);
		return 1;
	}
	hex2bin(hash, target, 20);
	{ cudaDeviceProp p; CUDA_OK(cudaGetDeviceProperties(&p, dev_idx));
	  fprintf(stderr, "device: %s\n", p.name); }
	CUDA_OK(cudaSetDevice(dev_idx));

	u8 *d_target, *d_hits; u8 *hitbuf = malloc(batch);
	CUDA_OK(cudaMalloc(&d_target, 20));
	CUDA_OK(cudaMalloc(&d_hits, batch));
	CUDA_OK(cudaMemcpy(d_target, target, 20, cudaMemcpyHostToDevice));

	const int TPB = 256;
	size_t blocks = (batch + TPB - 1) / TPB;
	double t0 = now_sec();
	unsigned long long total = 0;
	int found = 0;

	if (wlpath) {
		FILE *wf = strcmp(wlpath, "-") ? fopen(wlpath, "rb") : stdin;
		if (!wf) { perror("wordlist"); return 1; }
		u8 *pwbuf = malloc(batch * RECLEN);
		u32 *lens = malloc(batch * sizeof(u32));
		u8 *d_pw, *d_len;
		CUDA_OK(cudaMalloc(&d_pw, batch * RECLEN));
		CUDA_OK(cudaMalloc(&d_len, batch * sizeof(u32)));
		char line[8192];
		while (!found) {
			size_t cnt = 0;
			while (cnt < batch && fgets(line, sizeof(line), wf)) {
				size_t L = strlen(line);
				while (L && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = 0;
				if (!L) continue;
				memset(pwbuf + cnt * RECLEN, 0, RECLEN);
				lens[cnt] = (u32)utf8_to_utf16le((unsigned char *)line, (int)L, pwbuf + cnt * RECLEN, RECLEN);
				cnt++;
			}
			if (!cnt) break;
			CUDA_OK(cudaMemcpy(d_pw, pwbuf, cnt * RECLEN, cudaMemcpyHostToDevice));
			CUDA_OK(cudaMemcpy(d_len, lens, cnt * sizeof(u32), cudaMemcpyHostToDevice));
			size_t bl = (cnt + TPB - 1) / TPB;
			ds_check_word_k<<<(unsigned)bl, TPB>>>(d_pw, d_len, d_target, d_hits, (u32)cnt);
			CUDA_OK(cudaGetLastError());
			CUDA_OK(cudaDeviceSynchronize());
			CUDA_OK(cudaMemcpy(hitbuf, d_hits, cnt, cudaMemcpyDeviceToHost));
			total += cnt;
			for (size_t k = 0; k < cnt; k++)
				if (hitbuf[k]) { printf("\n*** MATCH at list position %llu ***\n", total - cnt + k); found = 1; }
			if ((total & 0xFFFFFF) == 0) {
				double d = now_sec() - t0;
				fprintf(stderr, "\r%llu  %.2f M/s   ", total, total / d / 1e6);
			}
		}
		if (wf != stdin) fclose(wf);
	} else {
		u8 csbuf[4096]; u32 csoff[MAXPOS], cslen[MAXPOS];
		unsigned int off = 0; unsigned long long space = 1;
		for (i = 0; i < npos; i++) {
			size_t L = strlen(pos[i]);
			if (!L || L > 200) die("bad charset");
			if (off + L > sizeof(csbuf)) die("charsets too large");
			csoff[i] = off; cslen[i] = (u32)L;
			memcpy(csbuf + off, pos[i], L); off += L;
			space *= L;
		}
		u8 p16[MAXAFFIX], s16[MAXAFFIX];
		u32 plen = (u32)utf8_to_utf16le((unsigned char *)prefix, (int)strlen(prefix), p16, MAXAFFIX);
		u32 slen = (u32)utf8_to_utf16le((unsigned char *)suffix, (int)strlen(suffix), s16, MAXAFFIX);
		if (plen/2 + npos + slen/2 > MAXPW) die("password too long");

		fprintf(stderr, "mask: %d positions, %llu candidates\n", npos, space);
		u8 *d_cs, *d_off, *d_len, *d_pre, *d_suf;
		CUDA_OK(cudaMalloc(&d_cs, off));
		CUDA_OK(cudaMalloc(&d_off, npos * sizeof(u32)));
		CUDA_OK(cudaMalloc(&d_len, npos * sizeof(u32)));
		CUDA_OK(cudaMalloc(&d_pre, plen ? plen : 1));
		CUDA_OK(cudaMalloc(&d_suf, slen ? slen : 1));
		CUDA_OK(cudaMemcpy(d_cs, csbuf, off, cudaMemcpyHostToDevice));
		CUDA_OK(cudaMemcpy(d_off, csoff, npos * sizeof(u32), cudaMemcpyHostToDevice));
		CUDA_OK(cudaMemcpy(d_len, cslen, npos * sizeof(u32), cudaMemcpyHostToDevice));
		if (plen) CUDA_OK(cudaMemcpy(d_pre, p16, plen, cudaMemcpyHostToDevice));
		if (slen) CUDA_OK(cudaMemcpy(d_suf, s16, slen, cudaMemcpyHostToDevice));

		unsigned long long base = 0;
		while (base < space && !found) {
			u32 n32 = (u32)((space - base < batch) ? (space - base) : batch);
			size_t bl = (n32 + TPB - 1) / TPB;
			ds_check_pos_k<<<(unsigned)bl, TPB>>>(d_cs, d_off, d_len, (u32)npos,
			                                     d_pre, plen, d_suf, slen,
			                                     d_target, d_hits, (u64)base, n32);
			CUDA_OK(cudaGetLastError());
			CUDA_OK(cudaDeviceSynchronize());
			CUDA_OK(cudaMemcpy(hitbuf, d_hits, n32, cudaMemcpyDeviceToHost));
			total += n32;
			for (u32 k = 0; k < n32; k++)
				if (hitbuf[k]) {
					unsigned long long idx = base + k, v = idx;
					char inner[MAXPOS + 1];
					for (int j = npos - 1; j >= 0; j--) { inner[j] = pos[j][v % strlen(pos[j])]; v /= strlen(pos[j]); }
					inner[npos] = 0;
					printf("\n*** MATCH: password = '%s%s%s'  (inner index %llu) ***\n", prefix, inner, suffix, idx);
					found = 1;
				}
			base += n32;
			{ double d = now_sec() - t0;
			  fprintf(stderr, "\r%llu/%llu  %.2f%%  %.2f M/s   ", total, space,
			          100.0 * (double)total / (double)space, total / d / 1e6); }
		}
	}
	{ double d = now_sec() - t0;
	  fprintf(stderr, "\n%llu candidates in %.1f s = %.2f M/s\n", total, d, total / d / 1e6); }
	if (!found) printf("NOT FOUND in the searched space\n");
	return found ? 0 : 2;
}
