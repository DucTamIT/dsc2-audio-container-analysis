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

	FILE *fp = fopen(kpath, "rb");
	if (!fp) { perror("kernel"); return 1; }
	fseek(fp, 0, SEEK_END); long ksz = ftell(fp); fseek(fp, 0, SEEK_SET);
	char *ksrc = malloc(ksz + 1);
	if (fread(ksrc, 1, ksz, fp) != (size_t)ksz) die("read kernel");
	ksrc[ksz] = 0; fclose(fp);

	cl_int err;
	cl_context ctx = clCreateContext(NULL, 1, &dev, NULL, NULL, &err);
	if (err != CL_SUCCESS) die("context");
	cl_command_queue q = clCreateCommandQueue(ctx, dev, 0, &err);
	if (err != CL_SUCCESS) die("queue");
	cl_program prog = clCreateProgramWithSource(ctx, 1, (const char **)&ksrc, (const size_t *)&ksz, &err);
	if (err != CL_SUCCESS) die("program");
	char opts[4300]; snprintf(opts, sizeof(opts), "%s -cl-mad-enable", inc);
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
