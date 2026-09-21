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
"__constant uint Te0[256] = {\n"
"	0xc66363a5, 0xf87c7c84, 0xee777799, 0xf67b7b8d, 0xfff2f20d, 0xd66b6bbd, 0xde6f6fb1, 0x91c5c554,\n"
"	0x60303050, 0x02010103, 0xce6767a9, 0x562b2b7d, 0xe7fefe19, 0xb5d7d762, 0x4dababe6, 0xec76769a,\n"
"	0x8fcaca45, 0x1f82829d, 0x89c9c940, 0xfa7d7d87, 0xeffafa15, 0xb25959eb, 0x8e4747c9, 0xfbf0f00b,\n"
"	0x41adadec, 0xb3d4d467, 0x5fa2a2fd, 0x45afafea, 0x239c9cbf, 0x53a4a4f7, 0xe4727296, 0x9bc0c05b,\n"
"	0x75b7b7c2, 0xe1fdfd1c, 0x3d9393ae, 0x4c26266a, 0x6c36365a, 0x7e3f3f41, 0xf5f7f702, 0x83cccc4f,\n"
"	0x6834345c, 0x51a5a5f4, 0xd1e5e534, 0xf9f1f108, 0xe2717193, 0xabd8d873, 0x62313153, 0x2a15153f,\n"
"	0x0804040c, 0x95c7c752, 0x46232365, 0x9dc3c35e, 0x30181828, 0x379696a1, 0x0a05050f, 0x2f9a9ab5,\n"
"	0x0e070709, 0x24121236, 0x1b80809b, 0xdfe2e23d, 0xcdebeb26, 0x4e272769, 0x7fb2b2cd, 0xea75759f,\n"
"	0x1209091b, 0x1d83839e, 0x582c2c74, 0x341a1a2e, 0x361b1b2d, 0xdc6e6eb2, 0xb45a5aee, 0x5ba0a0fb,\n"
"	0xa45252f6, 0x763b3b4d, 0xb7d6d661, 0x7db3b3ce, 0x5229297b, 0xdde3e33e, 0x5e2f2f71, 0x13848497,\n"
"	0xa65353f5, 0xb9d1d168, 0x00000000, 0xc1eded2c, 0x40202060, 0xe3fcfc1f, 0x79b1b1c8, 0xb65b5bed,\n"
"	0xd46a6abe, 0x8dcbcb46, 0x67bebed9, 0x7239394b, 0x944a4ade, 0x984c4cd4, 0xb05858e8, 0x85cfcf4a,\n"
"	0xbbd0d06b, 0xc5efef2a, 0x4faaaae5, 0xedfbfb16, 0x864343c5, 0x9a4d4dd7, 0x66333355, 0x11858594,\n"
"	0x8a4545cf, 0xe9f9f910, 0x04020206, 0xfe7f7f81, 0xa05050f0, 0x783c3c44, 0x259f9fba, 0x4ba8a8e3,\n"
"	0xa25151f3, 0x5da3a3fe, 0x804040c0, 0x058f8f8a, 0x3f9292ad, 0x219d9dbc, 0x70383848, 0xf1f5f504,\n"
"	0x63bcbcdf, 0x77b6b6c1, 0xafdada75, 0x42212163, 0x20101030, 0xe5ffff1a, 0xfdf3f30e, 0xbfd2d26d,\n"
"	0x81cdcd4c, 0x180c0c14, 0x26131335, 0xc3ecec2f, 0xbe5f5fe1, 0x359797a2, 0x884444cc, 0x2e171739,\n"
"	0x93c4c457, 0x55a7a7f2, 0xfc7e7e82, 0x7a3d3d47, 0xc86464ac, 0xba5d5de7, 0x3219192b, 0xe6737395,\n"
"	0xc06060a0, 0x19818198, 0x9e4f4fd1, 0xa3dcdc7f, 0x44222266, 0x542a2a7e, 0x3b9090ab, 0x0b888883,\n"
"	0x8c4646ca, 0xc7eeee29, 0x6bb8b8d3, 0x2814143c, 0xa7dede79, 0xbc5e5ee2, 0x160b0b1d, 0xaddbdb76,\n"
"	0xdbe0e03b, 0x64323256, 0x743a3a4e, 0x140a0a1e, 0x924949db, 0x0c06060a, 0x4824246c, 0xb85c5ce4,\n"
"	0x9fc2c25d, 0xbdd3d36e, 0x43acacef, 0xc46262a6, 0x399191a8, 0x319595a4, 0xd3e4e437, 0xf279798b,\n"
"	0xd5e7e732, 0x8bc8c843, 0x6e373759, 0xda6d6db7, 0x018d8d8c, 0xb1d5d564, 0x9c4e4ed2, 0x49a9a9e0,\n"
"	0xd86c6cb4, 0xac5656fa, 0xf3f4f407, 0xcfeaea25, 0xca6565af, 0xf47a7a8e, 0x47aeaee9, 0x10080818,\n"
"	0x6fbabad5, 0xf0787888, 0x4a25256f, 0x5c2e2e72, 0x381c1c24, 0x57a6a6f1, 0x73b4b4c7, 0x97c6c651,\n"
"	0xcbe8e823, 0xa1dddd7c, 0xe874749c, 0x3e1f1f21, 0x964b4bdd, 0x61bdbddc, 0x0d8b8b86, 0x0f8a8a85,\n"
"	0xe0707090, 0x7c3e3e42, 0x71b5b5c4, 0xcc6666aa, 0x904848d8, 0x06030305, 0xf7f6f601, 0x1c0e0e12,\n"
"	0xc26161a3, 0x6a35355f, 0xae5757f9, 0x69b9b9d0, 0x17868691, 0x99c1c158, 0x3a1d1d27, 0x279e9eb9,\n"
"	0xd9e1e138, 0xebf8f813, 0x2b9898b3, 0x22111133, 0xd26969bb, 0xa9d9d970, 0x078e8e89, 0x339494a7,\n"
"	0x2d9b9bb6, 0x3c1e1e22, 0x15878792, 0xc9e9e920, 0x87cece49, 0xaa5555ff, 0x50282878, 0xa5dfdf7a,\n"
"	0x038c8c8f, 0x59a1a1f8, 0x09898980, 0x1a0d0d17, 0x65bfbfda, 0xd7e6e631, 0x844242c6, 0xd06868b8,\n"
"	0x824141c3, 0x299999b0, 0x5a2d2d77, 0x1e0f0f11, 0x7bb0b0cb, 0xa85454fc, 0x6dbbbbd6, 0x2c16163a,\n"
"};\n"
"\n"
"#define ror32(x, n) rotate((uint)(x), (uint)(32 - (n)))\n"
"#define rol32(x, n) rotate((uint)(x), (uint)(n))\n"
"\n"
"__constant uchar sbox[256] = {\n"
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
"__constant uint K256[64] = {\n"
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
"void sha256_1block(__private const uint *in, __private uint *out) {\n"
"	uint w[16];\n"
"	for (int i = 0; i < 16; i++) w[i] = in[i];\n"
"	uint a = 0x6a09e667, b = 0xbb67ae85, c = 0x3c6ef372, d = 0xa54ff53a;\n"
"	uint e = 0x510e527f, f = 0x9b05688c, g = 0x1f83d9ab, h = 0x5be0cd19;\n"
"	#pragma unroll 16\n"
"	for (int i = 0; i < 64; i++) {\n"
"		if (i >= 16) {\n"
"			uint s0 = ror32(w[(i-15) & 15], 7) ^ ror32(w[(i-15) & 15], 18) ^ (w[(i-15) & 15] >> 3);\n"
"			uint s1 = ror32(w[(i-2) & 15], 17) ^ ror32(w[(i-2) & 15], 19) ^ (w[(i-2) & 15] >> 10);\n"
"			w[i & 15] = w[(i-16) & 15] + s0 + w[(i-7) & 15] + s1;\n"
"		}\n"
"		uint S1 = ror32(e, 6) ^ ror32(e, 11) ^ ror32(e, 25);\n"
"		uint ch = (e & f) ^ ((~e) & g);\n"
"		uint t1 = h + S1 + ch + K256[i] + w[i & 15];\n"
"		uint S0 = ror32(a, 2) ^ ror32(a, 13) ^ ror32(a, 22);\n"
"		uint maj = (a & b) ^ (a & c) ^ (b & c);\n"
"		uint t2 = S0 + maj;\n"
"		h = g; g = f; f = e; e = d + t1;\n"
"		d = c; c = b; b = a; a = t1 + t2;\n"
"	}\n"
"	out[0] = 0x6a09e667 + a;\n"
"	out[1] = 0xbb67ae85 + b;\n"
"	out[2] = 0x3c6ef372 + c;\n"
"	out[3] = 0xa54ff53a + d;\n"
"	out[4] = 0x510e527f + e;\n"
"	out[5] = 0x9b05688c + f;\n"
"	out[6] = 0x1f83d9ab + g;\n"
"	out[7] = 0x5be0cd19 + h;\n"
"}\n"
"\n"
"void aes256_setkey(__private const uint *key, __private uint *rk) {\n"
"	int i;\n"
"	for (i = 0; i < 8; i++) rk[i] = key[i];\n"
"	#pragma unroll 8\n"
"	for (i = 8; i < 60; i++) {\n"
"		uint t = rk[i-1];\n"
"		if (i % 8 == 0) {\n"
"			t = (t << 8) | (t >> 24);\n"
"			t = ((uint)sbox[(t >> 24) & 0xff] << 24) |\n"
"			    ((uint)sbox[(t >> 16) & 0xff] << 16) |\n"
"			    ((uint)sbox[(t >> 8) & 0xff] << 8) |\n"
"			    (uint)sbox[t & 0xff];\n"
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
"			t = ((uint)sbox[(t >> 24) & 0xff] << 24) |\n"
"			    ((uint)sbox[(t >> 16) & 0xff] << 16) |\n"
"			    ((uint)sbox[(t >> 8) & 0xff] << 8) |\n"
"			    (uint)sbox[t & 0xff];\n"
"		}\n"
"		rk[i] = rk[i-8] ^ t;\n"
"	}\n"
"}\n"
"\n"
"void aes256_encrypt_fast(__private const uint *rk, uint s0, uint s1, uint s2, uint s3,\n"
"                         __private uint *o0, __private uint *o1, __private uint *o2, __private uint *o3) {\n"
"	s0 ^= rk[0]; s1 ^= rk[1]; s2 ^= rk[2]; s3 ^= rk[3];\n"
"	#pragma unroll\n"
"	for (int r = 1; r < 14; r++) {\n"
"		uint a0 = s0, a1 = s1, a2 = s2, a3 = s3;\n"
"		s0 = Te0[a0 >> 24] ^ ror32(Te0[(a1 >> 16) & 0xff], 8) ^ ror32(Te0[(a2 >> 8) & 0xff], 16) ^ ror32(Te0[a3 & 0xff], 24) ^ rk[r*4 + 0];\n"
"		s1 = Te0[a1 >> 24] ^ ror32(Te0[(a2 >> 16) & 0xff], 8) ^ ror32(Te0[(a3 >> 8) & 0xff], 16) ^ ror32(Te0[a0 & 0xff], 24) ^ rk[r*4 + 1];\n"
"		s2 = Te0[a2 >> 24] ^ ror32(Te0[(a3 >> 16) & 0xff], 8) ^ ror32(Te0[(a0 >> 8) & 0xff], 16) ^ ror32(Te0[a1 & 0xff], 24) ^ rk[r*4 + 2];\n"
"		s3 = Te0[a3 >> 24] ^ ror32(Te0[(a0 >> 16) & 0xff], 8) ^ ror32(Te0[(a1 >> 8) & 0xff], 16) ^ ror32(Te0[a2 & 0xff], 24) ^ rk[r*4 + 3];\n"
"	}\n"
"	uint a0 = s0, a1 = s1, a2 = s2, a3 = s3;\n"
"	*o0 = (((uint)sbox[a0 >> 24] << 24) | ((uint)sbox[(a1 >> 16) & 0xff] << 16) | ((uint)sbox[(a2 >> 8) & 0xff] << 8) | (uint)sbox[a3 & 0xff]) ^ rk[56];\n"
"	*o1 = (((uint)sbox[a1 >> 24] << 24) | ((uint)sbox[(a2 >> 16) & 0xff] << 16) | ((uint)sbox[(a3 >> 8) & 0xff] << 8) | (uint)sbox[a0 & 0xff]) ^ rk[57];\n"
"	*o2 = (((uint)sbox[a2 >> 24] << 24) | ((uint)sbox[(a3 >> 16) & 0xff] << 16) | ((uint)sbox[(a0 >> 8) & 0xff] << 8) | (uint)sbox[a1 & 0xff]) ^ rk[58];\n"
"	*o3 = (((uint)sbox[a3 >> 24] << 24) | ((uint)sbox[(a0 >> 16) & 0xff] << 16) | ((uint)sbox[(a1 >> 8) & 0xff] << 8) | (uint)sbox[a2 & 0xff]) ^ rk[59];\n"
"}\n"
"\n"
"void sha1_1block(__private const uint *in, __private uint *out) {\n"
"	uint w[16];\n"
"	for (int i = 0; i < 16; i++) w[i] = in[i];\n"
"	uint a = 0x67452301, b = 0xefcdab89, c = 0x98badcfe, d = 0x10325476, e = 0xc3d2e1f0;\n"
"	#pragma unroll 16\n"
"	for (int i = 0; i < 80; i++) {\n"
"		if (i >= 16) {\n"
"			w[i & 15] = rol32(w[(i - 3) & 15] ^ w[(i - 8) & 15] ^ w[(i - 14) & 15] ^ w[(i - 16) & 15], 1);\n"
"		}\n"
"		uint f, k;\n"
"		if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5a827999; }\n"
"		else if (i < 40) { f = b ^ c ^ d; k = 0x6ed9eba1; }\n"
"		else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdc; }\n"
"		else { f = b ^ c ^ d; k = 0xca62c1d6; }\n"
"		uint t = rol32(a, 5) + f + e + k + w[i & 15];\n"
"		e = d; d = c; c = rol32(b, 30); b = a; a = t;\n"
"	}\n"
"	out[0] = 0x67452301 + a;\n"
"	out[1] = 0xefcdab89 + b;\n"
"	out[2] = 0x98badcfe + c;\n"
"	out[3] = 0x10325476 + d;\n"
"	out[4] = 0xc3d2e1f0 + e;\n"
"}\n"
"\n"
"__kernel void ds_check_pos_fast(__global const uchar *csbuf,\n"
"                                __global const uint  *csoff,\n"
"                                __global const uint  *cslen,\n"
"                                const uint  npos,\n"
"                                __global const uchar *prefix,\n"
"                                const uint  plen,\n"
"                                __global const uchar *suffix,\n"
"                                const uint  slen,\n"
"                                __global const uint  *target,\n"
"                                __global uchar       *hits,\n"
"                                const ulong base,\n"
"                                const uint  n)\n"
"{\n"
"	ulong gid = get_global_id(0);\n"
"	if (gid >= n) return;\n"
"	ulong v = base + gid;\n"
"\n"
"	uchar chars[32];\n"
"	for (uint i = 0; i < npos; i++) {\n"
"		uint r = cslen[i];\n"
"		uint c = (uint)(v % (ulong)r);\n"
"		v /= (ulong)r;\n"
"		chars[npos - 1 - i] = csbuf[csoff[npos - 1 - i] + c];\n"
"	}\n"
"\n"
"	uint u16_words[16];\n"
"	for (int i = 0; i < 16; i++) u16_words[i] = 0;\n"
"\n"
"	// Build UTF-16LE words\n"
"	uint cur_byte = 0;\n"
"	for (uint i = 0; i < plen; i++) {\n"
"		uint w_idx = cur_byte >> 2;\n"
"		uint b_idx = 3 - (cur_byte & 3);\n"
"		u16_words[w_idx] |= ((uint)prefix[i]) << (b_idx * 8);\n"
"		cur_byte++;\n"
"	}\n"
"	for (uint i = 0; i < npos; i++) {\n"
"		uint w_idx = cur_byte >> 2;\n"
"		uint b_idx = 3 - (cur_byte & 3);\n"
"		u16_words[w_idx] |= ((uint)chars[i]) << (b_idx * 8);\n"
"		cur_byte += 2; // character + zero byte in UTF-16LE\n"
"	}\n"
"	for (uint i = 0; i < slen; i++) {\n"
"		uint w_idx = cur_byte >> 2;\n"
"		uint b_idx = 3 - (cur_byte & 3);\n"
"		u16_words[w_idx] |= ((uint)suffix[i]) << (b_idx * 8);\n"
"		cur_byte++;\n"
"	}\n"
"\n"
"	// Padding 0x80\n"
"	uint w_idx = cur_byte >> 2;\n"
"	uint b_idx = 3 - (cur_byte & 3);\n"
"	u16_words[w_idx] |= (0x80u) << (b_idx * 8);\n"
"\n"
"	// Length in bits in last word\n"
"	u16_words[15] = cur_byte * 8;\n"
"\n"
"	// SHA-256\n"
"	uint key[8];\n"
"	sha256_1block(u16_words, key);\n"
"\n"
"	// AES-256\n"
"	uint rk[60];\n"
"	aes256_setkey(key, rk);\n"
"\n"
"	uint c0_0, c0_1, c0_2, c0_3;\n"
"	uint c1_0, c1_1, c1_2, c1_3;\n"
"	uint c2_0, c2_1, c2_2, c2_3;\n"
"\n"
"	aes256_encrypt_fast(rk, 0, 0, 0, 0, &c0_0, &c0_1, &c0_2, &c0_3);\n"
"	aes256_encrypt_fast(rk, key[4] ^ c0_0, key[5] ^ c0_1, key[6] ^ c0_2, key[7] ^ c0_3,\n"
"	                    &c1_0, &c1_1, &c1_2, &c1_3);\n"
"	aes256_encrypt_fast(rk, 0x10101010 ^ c1_0, 0x10101010 ^ c1_1, 0x10101010 ^ c1_2, 0x10101010 ^ c1_3,\n"
"	                    &c2_0, &c2_1, &c2_2, &c2_3);\n"
"\n"
"	// SHA-1\n"
"	uint sha1_in[16];\n"
"	sha1_in[0] = c0_0; sha1_in[1] = c0_1; sha1_in[2] = c0_2; sha1_in[3] = c0_3;\n"
"	sha1_in[4] = c1_0; sha1_in[5] = c1_1; sha1_in[6] = c1_2; sha1_in[7] = c1_3;\n"
"	sha1_in[8] = c2_0; sha1_in[9] = c2_1; sha1_in[10] = c2_2; sha1_in[11] = c2_3;\n"
"	sha1_in[12] = 0x80000000;\n"
"	sha1_in[13] = 0;\n"
"	sha1_in[14] = 0;\n"
"	sha1_in[15] = 0x00000180;\n"
"\n"
"	uint out[5];\n"
"	sha1_1block(sha1_in, out);\n"
"\n"
"	if (out[0] != target[0]) { hits[gid] = 0; return; }\n"
"	hits[gid] = (out[1] == target[1] && out[2] == target[2] && out[3] == target[3] && out[4] == target[4]) ? 1 : 0;\n"
"}\n"
;

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
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
	size_t batch = (size_t)1 << 23;
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
	cl_kernel kw = clCreateKernel(prog, "ds_check_pos_fast", &err);
	if (err != CL_SUCCESS) die("kernel word");
	cl_kernel kp = clCreateKernel(prog, "ds_check_pos_fast", &err);
	if (err != CL_SUCCESS) die("kernel pos");
	cl_kernel kwd = clCreateKernel(prog, "ds_check_pos_fast", &err);
	if (err != CL_SUCCESS) die("kernel words");

	
	cl_uint tgt_words[5];
	for (int ti = 0; ti < 5; ti++) {
		tgt_words[ti] = ((cl_uint)target[ti*4] << 24) | ((cl_uint)target[ti*4+1] << 16) | ((cl_uint)target[ti*4+2] << 8) | (cl_uint)target[ti*4+3];
	}
	cl_mem tgt = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 20, tgt_words, &err);

	cl_mem hits = clCreateBuffer(ctx, CL_MEM_WRITE_ONLY, batch, NULL, &err);
	unsigned char *hitbuf = malloc(batch);
	double t0 = now_sec();
	unsigned long long total = 0;
	int found = 0;

	if (wlpath) {
		FILE *wf;
		if (!strcmp(wlpath, "-")) {
#ifdef _WIN32
			_setmode(_fileno(stdin), _O_BINARY);
#endif
			wf = stdin;
		} else {
			wf = fopen(wlpath, "rb");
		}
		if (!wf) { perror("wordlist"); return 1; }
		setvbuf(wf, NULL, _IOFBF, 16 * 1024 * 1024);
		unsigned char *pwbuf = malloc(batch * RECLEN);
		cl_uint *lens = malloc(batch * sizeof(cl_uint));
		char line[8192];
		cl_mem bw = clCreateBuffer(ctx, CL_MEM_READ_ONLY, batch * RECLEN, NULL, &err);
		cl_mem bl = clCreateBuffer(ctx, CL_MEM_READ_ONLY, batch * sizeof(cl_uint), NULL, &err);
		while (!found) {
			size_t cnt = 0;
			while (cnt < batch && fgets(line, sizeof(line), wf)) {
				char *p = line;
				while (*p && *p != '\n' && *p != '\r') p++;
				size_t L = (size_t)(p - line);
				if (!L) continue;
				lens[cnt] = (cl_uint)utf8_to_utf16le((unsigned char *)line, (int)L, pwbuf + cnt * RECLEN, RECLEN);
				cnt++;
			}
			if (!cnt) {
				if (feof(wf)) break;
				continue;
			}
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
			size_t local = 256;
			while (g % local != 0 && local > 32) local /= 2;
			clEnqueueNDRangeKernel(q, kp, 1, NULL, &g, (g % local == 0) ? &local : NULL, 0, NULL, NULL);
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
