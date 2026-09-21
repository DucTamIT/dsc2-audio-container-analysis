with open('src/ds_gpu_v2.c', 'r', encoding='utf-8') as f:
    content = f.read()

word_fast_kernel = r'''"__kernel void ds_check_word_fast(__global const uchar *pwbuf,\n"
"                                 __global const uint  *pwlen,\n"
"                                 __global const uint  *target,\n"
"                                 __global uchar       *hits,\n"
"                                 const uint n)\n"
"{\n"
"	uint gid = get_global_id(0);\n"
"	if (gid >= n) return;\n"
"	uint len = pwlen[gid];\n"
"\n"
"	uint u16_words[16];\n"
"	for (int i = 0; i < 16; i++) u16_words[i] = 0;\n"
"\n"
"	__global const uchar *p = pwbuf + (size_t)gid * 128;\n"
"	uint copy_len = len > 54 ? 54 : len;\n"
"	for (uint i = 0; i < copy_len; i++) {\n"
"		uint w_idx = i >> 2;\n"
"		uint b_idx = 3 - (i & 3);\n"
"		u16_words[w_idx] |= ((uint)p[i]) << (b_idx * 8);\n"
"	}\n"
"	uint w_idx = copy_len >> 2;\n"
"	uint b_idx = 3 - (copy_len & 3);\n"
"	u16_words[w_idx] |= (0x80u) << (b_idx * 8);\n"
"	u16_words[15] = copy_len * 8;\n"
"\n"
"	uint key[8];\n"
"	sha256_1block(u16_words, key);\n"
"\n"
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
'''

idx = content.find(';\n\n#ifdef __APPLE__')
if idx == -1:
    idx = content.find(';\r\n\r\n#ifdef __APPLE__')
print('Kernel end index:', idx)
content = content[:idx] + word_fast_kernel + content[idx:]

kw_old = 'cl_kernel kw = clCreateKernel(prog, "ds_check_pos_fast", &err);'
kw_new = 'cl_kernel kw = clCreateKernel(prog, "ds_check_word_fast", &err);'
assert kw_old in content, 'kw_old not found!'
content = content.replace(kw_old, kw_new, 1)

with open('src/ds_gpu_v3.c', 'w', encoding='utf-8') as f:
    f.write(content)

print('Successfully created src/ds_gpu_v3.c')
