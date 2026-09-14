/*
 * DeepSound DSC2 GPU search tool (OpenCL).
 *
 *   clang -O2 -framework OpenCL -o ds_gpu ds_gpu.c      # macOS
 *   gcc   -O2 -o ds_gpu ds_gpu.c -lOpenCL               # Linux / MinGW
 *
 * Modes:
 *   --wordlist FILE                  try every line (UTF-8, diacritics OK)
 *   --mask CHARSET LEN               LEN positions, all using CHARSET
 *   --pos CHARSET (repeatable)       one charset per position, left to right
 *   --prefix STR --suffix STR        fixed UTF-8 prefix/suffix around the mask
 *
 * Examples:
 *   ./ds_gpu --hash c3891f...45f6 --wordlist rockyou.txt
 *   ./ds_gpu --hash c3891f...45f6 --mask abcdefghijklmnopqrstuvwxyz0123456789 8
 *   ./ds_gpu --hash c3891f...45f6 --prefix 'miniCTF{' --suffix '}' \
 *            --pos abcdefghijklmnopqrstuvwxyz0123456789_ ... (once per inner char)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Embedded OpenCL kernel (ds_kdf.h + ds_gpu.cl). */
static const char ds_kernel_src[] =
"/*\n"
" * DeepSound DSC2 key-check KDF, written so the same source compiles both as\n"
" * plain C (for CPU unit tests) and as OpenCL C (for the GPU kernel).\n"
" *\n"
" *   key      = SHA256( UTF-16LE(password) )\n"
" *   c0       = AES-256_k( 00*16 )\n"
" *   c1       = AES-256_k( key[16:32] ^ c0 )\n"
" *   c2       = AES-256_k( 10*16 ^ c1 )            (PKCS7 block)\n"
" *   keycheck = SHA1( c0 || c1 || c2 )\n"
" */\n"
"\n"
"#ifdef __OPENCL_VERSION__\n"
"typedef uchar ds_u8;\n"
"typedef uint  ds_u32;\n"
"typedef ulong ds_u64;\n"
"#define DS_CONST __constant\n"
"#define DS_FN static inline\n"
"#elif defined(__CUDACC__)\n"
"typedef unsigned char ds_u8;\n"
"typedef unsigned int  ds_u32;\n"
"typedef unsigned long long ds_u64;\n"
"#define DS_CONST __constant__\n"
"#define DS_FN static __device__ __forceinline__\n"
"#else\n"
"#include <stdint.h>\n"
"typedef uint8_t  ds_u8;\n"
"typedef uint32_t ds_u32;\n"
"typedef uint64_t ds_u64;\n"
"#define DS_CONST static const\n"
"#define DS_FN static inline\n"
"#endif\n"
"\n"
"#define DS_MAX_UTF16_BYTES 128\n"
"#define DS_MAX_UNITS       64   /* DS_MAX_UTF16_BYTES / 2 */\n"
"\n"
"/* ---------------- SHA-256 ---------------- */\n"
"\n"
"DS_CONST ds_u32 ds_sha256_k[64] = {\n"
"	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,\n"
"	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,\n"
"	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,\n"
"	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,\n"
"	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,\n"
"	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,\n"
"	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,\n"
"	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2\n"
"};\n"
"\n"
"DS_FN ds_u32 ds_ror32(ds_u32 x, ds_u32 n) { return (x >> n) | (x << (32 - n)); }\n"
"DS_FN ds_u32 ds_rol32(ds_u32 x, ds_u32 n) { return (x << n) | (x >> (32 - n)); }\n"
"\n"
"DS_FN void ds_sha256_block(ds_u32 st[8], const ds_u8 *p)\n"
"{\n"
"	ds_u32 w[64], a, b, c, d, e, f, g, h, t1, t2;\n"
"	int i;\n"
"\n"
"	for (i = 0; i < 16; i++)\n"
"		w[i] = ((ds_u32)p[i*4] << 24) | ((ds_u32)p[i*4+1] << 16) |\n"
"		       ((ds_u32)p[i*4+2] << 8) | (ds_u32)p[i*4+3];\n"
"	for (i = 16; i < 64; i++) {\n"
"		ds_u32 s0 = ds_ror32(w[i-15], 7) ^ ds_ror32(w[i-15], 18) ^ (w[i-15] >> 3);\n"
"		ds_u32 s1 = ds_ror32(w[i-2], 17) ^ ds_ror32(w[i-2], 19) ^ (w[i-2] >> 10);\n"
"		w[i] = w[i-16] + s0 + w[i-7] + s1;\n"
"	}\n"
"	a = st[0]; b = st[1]; c = st[2]; d = st[3];\n"
"	e = st[4]; f = st[5]; g = st[6]; h = st[7];\n"
"\n"
"	for (i = 0; i < 64; i++) {\n"
"		t1 = h + (ds_ror32(e,6) ^ ds_ror32(e,11) ^ ds_ror32(e,25)) +\n"
"		     ((e & f) ^ (~e & g)) + ds_sha256_k[i] + w[i];\n"
"		t2 = (ds_ror32(a,2) ^ ds_ror32(a,13) ^ ds_ror32(a,22)) +\n"
"		     ((a & b) ^ (a & c) ^ (b & c));\n"
"		h = g; g = f; f = e; e = d + t1;\n"
"		d = c; c = b; b = a; a = t1 + t2;\n"
"	}\n"
"	st[0] += a; st[1] += b; st[2] += c; st[3] += d;\n"
"	st[4] += e; st[5] += f; st[6] += g; st[7] += h;\n"
"}\n"
"\n"
"DS_FN void ds_sha256(const ds_u8 *msg, ds_u32 len, ds_u8 out[32])\n"
"{\n"
"	ds_u32 st[8] = { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,\n"
"	                 0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 };\n"
"	ds_u8 buf[DS_MAX_UTF16_BYTES + 72];\n"
"	ds_u32 total, i;\n"
"	ds_u64 bits;\n"
"\n"
"	for (i = 0; i < len; i++)\n"
"		buf[i] = msg[i];\n"
"	buf[len] = 0x80;\n"
"	total = ((len + 8) / 64 + 1) * 64;\n"
"	for (i = len + 1; i < total; i++)\n"
"		buf[i] = 0;\n"
"	bits = (ds_u64)len * 8;\n"
"	for (i = 0; i < 8; i++)\n"
"		buf[total - 8 + i] = (ds_u8)(bits >> (56 - 8 * i));\n"
"\n"
"	for (i = 0; i < total; i += 64)\n"
"		ds_sha256_block(st, buf + i);\n"
"\n"
"	for (i = 0; i < 8; i++) {\n"
"		out[i*4]   = (ds_u8)(st[i] >> 24);\n"
"		out[i*4+1] = (ds_u8)(st[i] >> 16);\n"
"		out[i*4+2] = (ds_u8)(st[i] >> 8);\n"
"		out[i*4+3] = (ds_u8)st[i];\n"
"	}\n"
"}\n"
"\n"
"/* ---------------- AES-256 (encrypt only) ---------------- */\n"
"\n"
"DS_CONST ds_u8 ds_aes_sbox[256] = {\n"
"	0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,\n"
"	0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,\n"
"	0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,\n"
"	0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,\n"
"	0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,\n"
"	0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,\n"
"	0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,\n"
"	0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,\n"
"	0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,\n"
"	0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,\n"
"	0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,\n"
"	0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,\n"
"	0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,\n"
"	0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,\n"
"	0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,\n"
"	0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16\n"
"};\n"
"\n"
"DS_FN ds_u8 ds_aes_xtime(ds_u8 x) { return (ds_u8)((x << 1) ^ ((x >> 7) * 0x1b)); }\n"
"\n"
"DS_FN void ds_aes256_setkey(const ds_u8 key[32], ds_u32 rk[60])\n"
"{\n"
"	int i;\n"
"	for (i = 0; i < 8; i++)\n"
"		rk[i] = ((ds_u32)key[4*i] << 24) | ((ds_u32)key[4*i+1] << 16) |\n"
"		        ((ds_u32)key[4*i+2] << 8) | (ds_u32)key[4*i+3];\n"
"	for (i = 8; i < 60; i++) {\n"
"		ds_u32 t = rk[i-1];\n"
"		if (i % 8 == 0) {\n"
"			t = (t << 8) | (t >> 24);\n"
"			t = ((ds_u32)ds_aes_sbox[(t >> 24) & 0xff] << 24) |\n"
"			    ((ds_u32)ds_aes_sbox[(t >> 16) & 0xff] << 16) |\n"
"			    ((ds_u32)ds_aes_sbox[(t >> 8) & 0xff] << 8) |\n"
"			    (ds_u32)ds_aes_sbox[t & 0xff];\n"
"			switch (i / 8) {\n"
"			case 1: t ^= 0x01000000; break;\n"
"			case 2: t ^= 0x02000000; break;\n"
"			case 3: t ^= 0x04000000; break;\n"
"			case 4: t ^= 0x08000000; break;\n"
"			case 5: t ^= 0x10000000; break;\n"
"			case 6: t ^= 0x20000000; break;\n"
"			case 7: t ^= 0x40000000; break;\n"
"			}\n"
"		} else if (i % 8 == 4) {\n"
"			t = ((ds_u32)ds_aes_sbox[(t >> 24) & 0xff] << 24) |\n"
"			    ((ds_u32)ds_aes_sbox[(t >> 16) & 0xff] << 16) |\n"
"			    ((ds_u32)ds_aes_sbox[(t >> 8) & 0xff] << 8) |\n"
"			    (ds_u32)ds_aes_sbox[t & 0xff];\n"
"		}\n"
"		rk[i] = rk[i-8] ^ t;\n"
"	}\n"
"}\n"
"\n"
"DS_FN void ds_aes256_encrypt(const ds_u32 rk[60], const ds_u8 in[16], ds_u8 out[16])\n"
"{\n"
"	ds_u8 s[16], t[16];\n"
"	int i, r;\n"
"\n"
"	for (i = 0; i < 16; i++)\n"
"		s[i] = in[i];\n"
"\n"
"	for (r = 0; r <= 14; r++) {\n"
"		if (r > 0) {\n"
"			/* SubBytes */\n"
"			for (i = 0; i < 16; i++)\n"
"				s[i] = ds_aes_sbox[s[i]];\n"
"			/* ShiftRows (column-major state: s[c*4+r]) */\n"
"			for (i = 0; i < 4; i++) {\n"
"				t[i*4+0] = s[((i+0)%4)*4 + 0];\n"
"				t[i*4+1] = s[((i+1)%4)*4 + 1];\n"
"				t[i*4+2] = s[((i+2)%4)*4 + 2];\n"
"				t[i*4+3] = s[((i+3)%4)*4 + 3];\n"
"			}\n"
"			for (i = 0; i < 16; i++)\n"
"				s[i] = t[i];\n"
"		}\n"
"		if (r > 0 && r < 14) {\n"
"			/* MixColumns */\n"
"			for (i = 0; i < 4; i++) {\n"
"				ds_u8 a0 = s[i*4+0], a1 = s[i*4+1], a2 = s[i*4+2], a3 = s[i*4+3];\n"
"				ds_u8 b0 = (ds_u8)(ds_aes_xtime(a0) ^ ds_aes_xtime(a1) ^ a1 ^ a2 ^ a3);\n"
"				ds_u8 b1 = (ds_u8)(a0 ^ ds_aes_xtime(a1) ^ ds_aes_xtime(a2) ^ a2 ^ a3);\n"
"				ds_u8 b2 = (ds_u8)(a0 ^ a1 ^ ds_aes_xtime(a2) ^ ds_aes_xtime(a3) ^ a3);\n"
"				ds_u8 b3 = (ds_u8)(ds_aes_xtime(a0) ^ a0 ^ a1 ^ a2 ^ ds_aes_xtime(a3));\n"
"				s[i*4+0] = b0; s[i*4+1] = b1; s[i*4+2] = b2; s[i*4+3] = b3;\n"
"			}\n"
"		}\n"
"		/* AddRoundKey */\n"
"		for (i = 0; i < 4; i++) {\n"
"			ds_u32 k = rk[r*4 + i];\n"
"			s[i*4+0] ^= (ds_u8)(k >> 24);\n"
"			s[i*4+1] ^= (ds_u8)(k >> 16);\n"
"			s[i*4+2] ^= (ds_u8)(k >> 8);\n"
"			s[i*4+3] ^= (ds_u8)k;\n"
"		}\n"
"	}\n"
"	for (i = 0; i < 16; i++)\n"
"		out[i] = s[i];\n"
"}\n"
"\n"
"/* ---------------- SHA-1 ---------------- */\n"
"\n"
"DS_FN void ds_sha1_block(ds_u32 st[5], const ds_u8 *p)\n"
"{\n"
"	ds_u32 w[80], a, b, c, d, e, t;\n"
"	int i;\n"
"\n"
"	for (i = 0; i < 16; i++)\n"
"		w[i] = ((ds_u32)p[i*4] << 24) | ((ds_u32)p[i*4+1] << 16) |\n"
"		       ((ds_u32)p[i*4+2] << 8) | (ds_u32)p[i*4+3];\n"
"	for (i = 16; i < 80; i++)\n"
"		w[i] = ds_rol32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);\n"
"\n"
"	a = st[0]; b = st[1]; c = st[2]; d = st[3]; e = st[4];\n"
"	for (i = 0; i < 80; i++) {\n"
"		ds_u32 f, k;\n"
"		if (i < 20)      { f = (b & c) | (~b & d);          k = 0x5a827999; }\n"
"		else if (i < 40) { f = b ^ c ^ d;                   k = 0x6ed9eba1; }\n"
"		else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdc; }\n"
"		else             { f = b ^ c ^ d;                   k = 0xca62c1d6; }\n"
"		t = ds_rol32(a, 5) + f + e + k + w[i];\n"
"		e = d; d = c; c = ds_rol32(b, 30); b = a; a = t;\n"
"	}\n"
"	st[0] += a; st[1] += b; st[2] += c; st[3] += d; st[4] += e;\n"
"}\n"
"\n"
"DS_FN void ds_sha1(const ds_u8 *msg, ds_u32 len, ds_u8 out[20])\n"
"{\n"
"	ds_u32 st[5] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 };\n"
"	ds_u8 buf[192];\n"
"	ds_u32 total, i;\n"
"	ds_u64 bits;\n"
"\n"
"	for (i = 0; i < len; i++)\n"
"		buf[i] = msg[i];\n"
"	buf[len] = 0x80;\n"
"	total = ((len + 8) / 64 + 1) * 64;\n"
"	for (i = len + 1; i < total; i++)\n"
"		buf[i] = 0;\n"
"	bits = (ds_u64)len * 8;\n"
"	for (i = 0; i < 8; i++)\n"
"		buf[total - 8 + i] = (ds_u8)(bits >> (56 - 8 * i));\n"
"\n"
"	for (i = 0; i < total; i += 64)\n"
"		ds_sha1_block(st, buf + i);\n"
"\n"
"	for (i = 0; i < 5; i++) {\n"
"		out[i*4]   = (ds_u8)(st[i] >> 24);\n"
"		out[i*4+1] = (ds_u8)(st[i] >> 16);\n"
"		out[i*4+2] = (ds_u8)(st[i] >> 8);\n"
"		out[i*4+3] = (ds_u8)st[i];\n"
"	}\n"
"}\n"
"\n"
"/* ---------------- the DeepSound DSC2 KDF ---------------- */\n"
"\n"
"/* u16 = UTF-16LE password bytes, len = byte count (0 .. 128) */\n"
"DS_FN void ds_deepsound_kdf(const ds_u8 *u16, ds_u32 len, ds_u8 out[20])\n"
"{\n"
"	ds_u8 key[32], blk[16], c0[16], c1[16], c2[16], cat[48];\n"
"	ds_u32 rk[60];\n"
"	int i;\n"
"\n"
"	ds_sha256(u16, len, key);\n"
"	ds_aes256_setkey(key, rk);\n"
"\n"
"	for (i = 0; i < 16; i++)\n"
"		blk[i] = 0;\n"
"	ds_aes256_encrypt(rk, blk, c0);\n"
"\n"
"	for (i = 0; i < 16; i++)\n"
"		blk[i] = key[16 + i] ^ c0[i];\n"
"	ds_aes256_encrypt(rk, blk, c1);\n"
"\n"
"	for (i = 0; i < 16; i++)\n"
"		blk[i] = 0x10 ^ c1[i];\n"
"	ds_aes256_encrypt(rk, blk, c2);\n"
"\n"
"	for (i = 0; i < 16; i++) {\n"
"		cat[i] = c0[i]; cat[16 + i] = c1[i]; cat[32 + i] = c2[i];\n"
"	}\n"
"	ds_sha1(cat, 48, out);\n"
"}\n"
"\n"
"/* DeepSound DSC2 GPU search tool - OpenCL kernels.\n"
" * Build with: -I <dir containing ds_kdf.h> -cl-mad-enable\n"
" */\n"
"\n"
"\n"
"#define DS_TARGET_LEN 20\n"
"\n"
"/* One work-item per candidate from a host-supplied wordlist blob. */\n"
"__kernel void ds_check_word(__global const uchar *pwbuf,   /* n * DS_MAX_UTF16_BYTES */\n"
"                            __global const uint  *pwlen,   /* n, in bytes           */\n"
"                            __global const uchar *target,\n"
"                            __global uchar       *hits,\n"
"                            const uint n)\n"
"{\n"
"	uint gid = get_global_id(0);\n"
"	uchar u16[DS_MAX_UTF16_BYTES];\n"
"	uchar out[20];\n"
"	uint len, i;\n"
"	uchar m = 1;\n"
"\n"
"	if (gid >= n)\n"
"		return;\n"
"\n"
"	len = pwlen[gid];\n"
"	for (i = 0; i < len; i++)\n"
"		u16[i] = pwbuf[(size_t)gid * DS_MAX_UTF16_BYTES + i];\n"
"\n"
"	ds_deepsound_kdf(u16, len, out);\n"
"\n"
"	for (i = 0; i < DS_TARGET_LEN; i++)\n"
"		if (out[i] != target[i]) { m = 0; break; }\n"
"	hits[gid] = m;\n"
"}\n"
"\n"
"/*\n"
" * One work-item per candidate generated on-device.\n"
" *   candidate (UTF-16) = prefix || pos[0] pos[1] ... pos[npos-1] || suffix\n"
" * Charset for position i is csbuf[csoff[i] .. csoff[i]+cslen[i]).\n"
" * Position 0 is the leftmost generated character.\n"
" */\n"
"__kernel void ds_check_pos(__global const uchar *csbuf,\n"
"                           __global const uint  *csoff,\n"
"                           __global const uint  *cslen,\n"
"                           const uint  npos,\n"
"                           __global const uchar *prefix,\n"
"                           const uint  plen,\n"
"                           __global const uchar *suffix,\n"
"                           const uint  slen,\n"
"                           __global const uchar *target,\n"
"                           __global uchar *hits,\n"
"                           const ulong base,\n"
"                           const uint  n)\n"
"{\n"
"	ulong gid = get_global_id(0);\n"
"	ulong v;\n"
"	uchar u16[DS_MAX_UNITS * 2];\n"
"	uchar out[20];\n"
"	uint o = 0, i, c, p;\n"
"	uchar m = 1;\n"
"\n"
"	if (gid >= n)\n"
"		return;\n"
"\n"
"	v = base + gid;\n"
"\n"
"	for (i = 0; i < plen; i++)\n"
"		u16[o++] = prefix[i];\n"
"\n"
"	for (i = 0; i < npos; i++) {\n"
"		uint r = cslen[i];\n"
"		c = (uint)(v % (ulong)r);\n"
"		v /= (ulong)r;\n"
"		p = npos - 1 - i;                 /* fill from the right */\n"
"		u16[o + 2 * p] = csbuf[csoff[p] + c];\n"
"		u16[o + 2 * p + 1] = 0;\n"
"	}\n"
"	o += npos * 2;\n"
"\n"
"	for (i = 0; i < slen; i++)\n"
"		u16[o++] = suffix[i];\n"
"\n"
"	ds_deepsound_kdf(u16, o, out);\n"
"\n"
"	for (i = 0; i < DS_TARGET_LEN; i++)\n"
"		if (out[i] != target[i]) { m = 0; break; }\n"
"	hits[gid] = m;\n"
"}\n"
"\n"
"/*\n"
" * Word/phrase mode: each position is a list of whole words (already UTF-16LE),\n"
" * combined with a separator. Enumerates the cartesian product on-device.\n"
" * Element tables: for position p, elements are elem[pbase[p] .. pbase[p]+pcount[p]).\n"
" * Each element e has bytes at wbuf[eoff[e] .. eoff[e]+elen[e]).\n"
" */\n"
"__kernel void ds_check_words(__global const uchar *wbuf,\n"
"                             __global const uint  *eoff,\n"
"                             __global const uint  *elen,\n"
"                             __global const uint  *pbase,\n"
"                             __global const uint  *pcount,\n"
"                             const uint  npos,\n"
"                             __global const uchar *sep,\n"
"                             const uint  seplen,\n"
"                             __global const uchar *prefix,\n"
"                             const uint  plen,\n"
"                             __global const uchar *suffix,\n"
"                             const uint  slen,\n"
"                             __global const uchar *target,\n"
"                             __global uchar *hits,\n"
"                             const ulong base,\n"
"                             const uint  n)\n"
"{\n"
"	ulong gid = get_global_id(0);\n"
"	ulong v;\n"
"	uchar u16[DS_MAX_UTF16_BYTES];\n"
"	uchar out[20];\n"
"	uint o = 0, i, j, e;\n"
"	uchar m = 1;\n"
"\n"
"	if (gid >= n)\n"
"		return;\n"
"	v = base + gid;\n"
"\n"
"	for (i = 0; i < plen && o < DS_MAX_UTF16_BYTES; i++)\n"
"		u16[o++] = prefix[i];\n"
"\n"
"	for (i = 0; i < npos; i++) {\n"
"		uint r = pcount[i];\n"
"		e = pbase[i] + (uint)(v % (ulong)r);\n"
"		v /= (ulong)r;\n"
"		if (i) {\n"
"			for (j = 0; j < seplen && o < DS_MAX_UTF16_BYTES; j++)\n"
"				u16[o++] = sep[j];\n"
"		}\n"
"		for (j = 0; j < elen[e] && o < DS_MAX_UTF16_BYTES; j++)\n"
"			u16[o++] = wbuf[eoff[e] + j];\n"
"	}\n"
"\n"
"	for (i = 0; i < slen && o < DS_MAX_UTF16_BYTES; i++)\n"
"		u16[o++] = suffix[i];\n"
"\n"
"	ds_deepsound_kdf(u16, o, out);\n"
"\n"
"	for (i = 0; i < DS_TARGET_LEN; i++)\n"
"		if (out[i] != target[i]) { m = 0; break; }\n"
"	hits[gid] = m;\n"
"}\n"
"\n";


#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

#define MAXPW      64            /* UTF-16 code units                        */
#define RECLEN     (2 * MAXPW)   /* must equal DS_MAX_UTF16_BYTES            */
#define MAXPOS     32
#define MAXAFFIX   (2 * MAXPW)

static void die(const char *msg) { fprintf(stderr, "error: %s\n", msg); exit(1); }

static void hex2bin(const char *hex, unsigned char *out, int n)
{
	int i;
	for (i = 0; i < n; i++) {
		unsigned v;
		if (sscanf(hex + 2 * i, "%2x", &v) != 1) die("bad hex in --hash");
		out[i] = (unsigned char)v;
	}
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
			out[o++] = (unsigned char)(0xD8 + (v >> 18));
			out[o++] = (unsigned char)((v >> 10) & 0xFF);
			out[o++] = (unsigned char)(0xDC + ((v >> 8) & 0x03));
			out[o++] = (unsigned char)(v & 0xFF);
		} else {
			out[o++] = (unsigned char)(cp & 0xFF);
			out[o++] = (unsigned char)((cp >> 8) & 0xFF);
		}
	}
	return o;
}

static double now_sec(void)
{
	struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv)
{
	const char *hash = NULL, *wlpath = NULL;
	const char *pos[MAXPOS]; int npos = 0;
	const char *wordfiles[MAXPOS]; int nwordfiles = 0;
	const char *sep = "";
	const char *prefix = "", *suffix = "";
	int use_cpu = 0, device_idx = -1, list_only = 0;
	size_t batch = (size_t)1 << 21;
	unsigned char target[20];
	char kpath[4096] = "ds_gpu.cl", inc[4096] = "-I .";
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
		else if (!strcmp(argv[i], "--wordfile") && i+1 < argc) { if (nwordfiles >= MAXPOS) die("too many --wordfile"); wordfiles[nwordfiles++] = argv[++i]; }
		else if (!strcmp(argv[i], "--sep") && i+1 < argc) sep = argv[++i];
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
		else if (!strcmp(argv[i], "--device") && i+1 < argc) device_idx = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--cpu")) use_cpu = 1;
		else if (!strcmp(argv[i], "--list")) list_only = 1;
		else if (!strcmp(argv[i], "--kernel") && i+1 < argc) {
			strncpy(kpath, argv[++i], sizeof(kpath)-1);
			char *sl = strrchr(kpath, '/');
			if (sl) { *sl = 0; snprintf(inc, sizeof(inc), "-I %s", kpath[0] ? kpath : "."); *sl = '/'; }
		}
		else { fprintf(stderr, "unknown/incomplete option: %s\n", argv[i]); return 1; }
	}
	if (!list_only && (!hash || (!wlpath && !npos && !nwordfiles))) {
		fprintf(stderr, "usage: %s --hash <40hex> (--wordlist FILE | --mask CS LEN | --pos CS ...) [--prefix S] [--suffix S]\n", argv[0]);
		return 1;
	}
	if (hash) hex2bin(hash, target, 20);

	cl_platform_id plat; cl_uint np = 0;
	if (clGetPlatformIDs(1, &plat, &np) != CL_SUCCESS || !np) die("no OpenCL platform");
	cl_device_id devs[64]; cl_uint nd = 0;
	cl_device_type dt = use_cpu ? CL_DEVICE_TYPE_ALL : CL_DEVICE_TYPE_GPU;
	if (clGetDeviceIDs(plat, dt, 64, devs, &nd) != CL_SUCCESS || !nd)
		if (clGetDeviceIDs(plat, CL_DEVICE_TYPE_ALL, 64, devs, &nd) != CL_SUCCESS || !nd)
			die("no devices");
	if (list_only) {
		for (cl_uint k = 0; k < nd; k++) {
			char nm[256] = {0}; cl_ulong m = 0;
			clGetDeviceInfo(devs[k], CL_DEVICE_NAME, sizeof(nm), nm, NULL);
			clGetDeviceInfo(devs[k], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(m), &m, NULL);
			printf("  [%u] %s (%llu MB)\n", k, nm, (unsigned long long)(m >> 20));
		}
		return 0;
	}
	cl_device_id dev = (device_idx >= 0 && (cl_uint)device_idx < nd) ? devs[device_idx] : devs[0];
	{ char nm[256] = {0}; clGetDeviceInfo(dev, CL_DEVICE_NAME, sizeof(nm), nm, NULL);
	  fprintf(stderr, "device: %s\n", nm); }

	const char *ksrc = ds_kernel_src;
	size_t ksz = strlen(ds_kernel_src);
	(void)kpath; (void)inc;

	cl_int err;
	cl_context ctx = clCreateContext(NULL, 1, &dev, NULL, NULL, &err);
	if (err != CL_SUCCESS) die("context");
	cl_command_queue q = clCreateCommandQueue(ctx, dev, 0, &err);
	if (err != CL_SUCCESS) die("queue");
	cl_program prog = clCreateProgramWithSource(ctx, 1, &ksrc, &ksz, &err);
	if (err != CL_SUCCESS) die("program");
	char opts[64] = "-cl-mad-enable";
	err = clBuildProgram(prog, 1, &dev, opts, NULL, NULL);
	if (err != CL_SUCCESS) {
		size_t ls = 0; clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, 0, NULL, &ls);
		char *log = malloc(ls + 1); clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, ls, log, NULL);
		log[ls] = 0; fprintf(stderr, "build failed:\n%s\n", log); return 1;
	}
	cl_kernel kw = clCreateKernel(prog, "ds_check_word", &err);
	if (err != CL_SUCCESS) die("kernel word");
	cl_kernel kp = clCreateKernel(prog, "ds_check_pos", &err);
	if (err != CL_SUCCESS) die("kernel pos");
	cl_kernel kwd = clCreateKernel(prog, "ds_check_words", &err);
	if (err != CL_SUCCESS) die("kernel words");

	cl_mem tgt = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 20, target, &err);
	cl_mem hits = clCreateBuffer(ctx, CL_MEM_WRITE_ONLY, batch, NULL, &err);
	unsigned char *hitbuf = malloc(batch);
	double t0 = now_sec();
	unsigned long long total = 0;
	int found = 0;

	if (wlpath) {
		FILE *wf = strcmp(wlpath, "-") ? fopen(wlpath, "rb") : stdin;
		if (!wf) { perror("wordlist"); return 1; }
		unsigned char *pwbuf = malloc(batch * RECLEN);
		cl_uint *lens = malloc(batch * sizeof(cl_uint));
		char line[8192];
		cl_mem bw = clCreateBuffer(ctx, CL_MEM_READ_ONLY, batch * RECLEN, NULL, &err);
		cl_mem bl = clCreateBuffer(ctx, CL_MEM_READ_ONLY, batch * sizeof(cl_uint), NULL, &err);
		while (!found) {
			size_t cnt = 0;
			while (cnt < batch && fgets(line, sizeof(line), wf)) {
				size_t L = strlen(line);
				while (L && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = 0;
				if (!L) continue;
				memset(pwbuf + cnt * RECLEN, 0, RECLEN);
				lens[cnt] = (cl_uint)utf8_to_utf16le((unsigned char *)line, (int)L, pwbuf + cnt * RECLEN, RECLEN);
				cnt++;
			}
			if (!cnt) break;
			clEnqueueWriteBuffer(q, bw, CL_FALSE, 0, cnt * RECLEN, pwbuf, 0, NULL, NULL);
			clEnqueueWriteBuffer(q, bl, CL_FALSE, 0, cnt * sizeof(cl_uint), lens, 0, NULL, NULL);
			cl_uint n32 = (cl_uint)cnt;
			clSetKernelArg(kw, 0, sizeof(cl_mem), &bw);
			clSetKernelArg(kw, 1, sizeof(cl_mem), &bl);
			clSetKernelArg(kw, 2, sizeof(cl_mem), &tgt);
			clSetKernelArg(kw, 3, sizeof(cl_mem), &hits);
			clSetKernelArg(kw, 4, sizeof(cl_uint), &n32);
			size_t g = cnt;
			clEnqueueNDRangeKernel(q, kw, 1, NULL, &g, NULL, 0, NULL, NULL);
			clEnqueueReadBuffer(q, hits, CL_TRUE, 0, cnt, hitbuf, 0, NULL, NULL);
			total += cnt;
			for (size_t k = 0; k < cnt; k++)
				if (hitbuf[k]) { printf("\n*** MATCH at list position %llu ***\n", total - cnt + k); found = 1; }
			if ((total & 0xFFFFFF) == 0) {
				double d = now_sec() - t0;
				fprintf(stderr, "\r%llu  %.2f M/s   ", total, total / d / 1e6);
			}
		}
		if (wf != stdin) fclose(wf);
	} else if (npos) {
		unsigned char csbuf[4096]; cl_uint csoff[MAXPOS], cslen[MAXPOS];
		unsigned int off = 0;
		unsigned long long space = 1;
		for (i = 0; i < npos; i++) {
			size_t L = strlen(pos[i]);
			if (!L || L > 200) die("bad charset");
			if (off + L > sizeof(csbuf)) die("charsets too large");
			csoff[i] = off; cslen[i] = (cl_uint)L;
			memcpy(csbuf + off, pos[i], L); off += L;
			space *= L;
		}
		unsigned char p16[MAXAFFIX], s16[MAXAFFIX];
		cl_uint plen = (cl_uint)utf8_to_utf16le((unsigned char *)prefix, (int)strlen(prefix), p16, MAXAFFIX);
		cl_uint slen = (cl_uint)utf8_to_utf16le((unsigned char *)suffix, (int)strlen(suffix), s16, MAXAFFIX);
		if (plen / 2 + npos + slen / 2 > MAXPW) die("password too long");

		fprintf(stderr, "mask: %d positions, %llu candidates%s%s%s%s\n", npos, space,
		        prefix[0] ? "  prefix='" : "", prefix[0] ? prefix : "",
		        suffix[0] ? "'  suffix='" : "", suffix[0] ? suffix : "");
		cl_mem bcs  = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, off, csbuf, &err);
		cl_mem boff = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, npos*sizeof(cl_uint), csoff, &err);
		cl_mem blen = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, npos*sizeof(cl_uint), cslen, &err);
		cl_mem bpre = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, plen ? plen : 1, p16, &err);
		cl_mem bsuf = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, slen ? slen : 1, s16, &err);
		cl_uint npos32 = (cl_uint)npos;
		unsigned long long base = 0;
		while (base < space && !found) {
			cl_uint n32 = (cl_uint)((space - base < batch) ? (space - base) : batch);
			cl_ulong b64 = (cl_ulong)base;
			clSetKernelArg(kp, 0, sizeof(cl_mem), &bcs);
			clSetKernelArg(kp, 1, sizeof(cl_mem), &boff);
			clSetKernelArg(kp, 2, sizeof(cl_mem), &blen);
			clSetKernelArg(kp, 3, sizeof(cl_uint), &npos32);
			clSetKernelArg(kp, 4, sizeof(cl_mem), &bpre);
			clSetKernelArg(kp, 5, sizeof(cl_uint), &plen);
			clSetKernelArg(kp, 6, sizeof(cl_mem), &bsuf);
			clSetKernelArg(kp, 7, sizeof(cl_uint), &slen);
			clSetKernelArg(kp, 8, sizeof(cl_mem), &tgt);
			clSetKernelArg(kp, 9, sizeof(cl_mem), &hits);
			clSetKernelArg(kp, 10, sizeof(cl_ulong), &b64);
			clSetKernelArg(kp, 11, sizeof(cl_uint), &n32);
			size_t g = n32;
			clEnqueueNDRangeKernel(q, kp, 1, NULL, &g, NULL, 0, NULL, NULL);
			clEnqueueReadBuffer(q, hits, CL_TRUE, 0, n32, hitbuf, 0, NULL, NULL);
			total += n32;
			for (cl_uint k = 0; k < n32; k++)
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
	} else if (nwordfiles) {
		/* ---------------- word / passphrase combination mode ---------------- */
		static unsigned char wbuf[64 * 1024 * 1024];
		static unsigned int eoff[400000], elen[400000];
		unsigned int pbase[MAXPOS], pcount[MAXPOS];
		unsigned int off = 0, ne = 0;
		unsigned long long space = 1;
		for (i = 0; i < nwordfiles; i++) {
			FILE *f = fopen(wordfiles[i], "rb");
			if (!f) { perror("wordfile"); return 1; }
			pbase[i] = ne;
			char line[4096];
			while (fgets(line, sizeof(line), f)) {
				size_t L = strlen(line);
				unsigned char u16[MAXAFFIX];
				while (L && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = 0;
				if (!L) continue;
				int nb = utf8_to_utf16le((unsigned char *)line, (int)L, u16, MAXAFFIX);
				if (nb <= 0) continue;
				if (off + (unsigned)nb > sizeof(wbuf) || ne >= 400000) { fprintf(stderr, "wordfile too large\n"); return 1; }
				if (eoff[ne]) {}
				memcpy(wbuf + off, u16, nb);
				eoff[ne] = off; elen[ne] = nb; off += nb; ne++;
			}
			fclose(f);
			pcount[i] = ne - pbase[i];
			if (!pcount[i]) die("empty wordfile");
			space *= pcount[i];
		}
		unsigned char sep16[MAXAFFIX], p16[MAXAFFIX], s16[MAXAFFIX];
		cl_uint seplen = (cl_uint)utf8_to_utf16le((unsigned char *)sep, (int)strlen(sep), sep16, MAXAFFIX);
		cl_uint plen = (cl_uint)utf8_to_utf16le((unsigned char *)prefix, (int)strlen(prefix), p16, MAXAFFIX);
		cl_uint slen = (cl_uint)utf8_to_utf16le((unsigned char *)suffix, (int)strlen(suffix), s16, MAXAFFIX);
		fprintf(stderr, "words: %d positions, %u elements, %llu combinations\n", nwordfiles, ne, space);

		cl_mem bw = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, off, wbuf, &err);
		cl_mem beo = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, ne*4, eoff, &err);
		cl_mem bel = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, ne*4, elen, &err);
		cl_mem bpb = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, nwordfiles*4, pbase, &err);
		cl_mem bpc = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, nwordfiles*4, pcount, &err);
		cl_mem bsep = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, seplen?seplen:1, sep16, &err);
		cl_mem bpre = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, plen?plen:1, p16, &err);
		cl_mem bsuf = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, slen?slen:1, s16, &err);
		cl_uint npos32 = (cl_uint)nwordfiles;
		unsigned long long base = 0;
		while (base < space && !found) {
			cl_uint n32 = (cl_uint)((space - base < batch) ? (space - base) : batch);
			cl_ulong b64 = (cl_ulong)base;
			clSetKernelArg(kwd, 0, sizeof(cl_mem), &bw);
			clSetKernelArg(kwd, 1, sizeof(cl_mem), &beo);
			clSetKernelArg(kwd, 2, sizeof(cl_mem), &bel);
			clSetKernelArg(kwd, 3, sizeof(cl_mem), &bpb);
			clSetKernelArg(kwd, 4, sizeof(cl_mem), &bpc);
			clSetKernelArg(kwd, 5, sizeof(cl_uint), &npos32);
			clSetKernelArg(kwd, 6, sizeof(cl_mem), &bsep);
			clSetKernelArg(kwd, 7, sizeof(cl_uint), &seplen);
			clSetKernelArg(kwd, 8, sizeof(cl_mem), &bpre);
			clSetKernelArg(kwd, 9, sizeof(cl_uint), &plen);
			clSetKernelArg(kwd, 10, sizeof(cl_mem), &bsuf);
			clSetKernelArg(kwd, 11, sizeof(cl_uint), &slen);
			clSetKernelArg(kwd, 12, sizeof(cl_mem), &tgt);
			clSetKernelArg(kwd, 13, sizeof(cl_mem), &hits);
			clSetKernelArg(kwd, 14, sizeof(cl_ulong), &b64);
			clSetKernelArg(kwd, 15, sizeof(cl_uint), &n32);
			size_t g = n32;
			clEnqueueNDRangeKernel(q, kwd, 1, NULL, &g, NULL, 0, NULL, NULL);
			clEnqueueReadBuffer(q, hits, CL_TRUE, 0, n32, hitbuf, 0, NULL, NULL);
			total += n32;
			for (cl_uint k = 0; k < n32; k++)
				if (hitbuf[k]) {
					unsigned long long idx = base + k, v = idx;
					printf("\n*** MATCH at combination index %llu (words mode) ***\n", idx);
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
