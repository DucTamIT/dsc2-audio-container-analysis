/* DeepSound DSC2 GPU search tool - OpenCL kernels.
 * Build with: -I <dir containing ds_kdf.h> -cl-mad-enable
 */

#include "ds_kdf.h"

#define DS_TARGET_LEN 20

/* One work-item per candidate from a host-supplied wordlist blob. */
__kernel void ds_check_word(__global const uchar *pwbuf,   /* n * DS_MAX_UTF16_BYTES */
                            __global const uint  *pwlen,   /* n, in bytes           */
                            __global const uchar *target,
                            __global uchar       *hits,
                            const uint n)
{
	uint gid = get_global_id(0);
	uchar u16[DS_MAX_UTF16_BYTES];
	uchar out[20];
	uint len, i;
	uchar m = 1;

	if (gid >= n)
		return;

	len = pwlen[gid];
	for (i = 0; i < len; i++)
		u16[i] = pwbuf[(size_t)gid * DS_MAX_UTF16_BYTES + i];

	ds_deepsound_kdf(u16, len, out);

	for (i = 0; i < DS_TARGET_LEN; i++)
		if (out[i] != target[i]) { m = 0; break; }
	hits[gid] = m;
}

/*
 * One work-item per candidate generated on-device.
 *   candidate (UTF-16) = prefix || pos[0] pos[1] ... pos[npos-1] || suffix
 * Charset for position i is csbuf[csoff[i] .. csoff[i]+cslen[i]).
 * Position 0 is the leftmost generated character.
 */
__kernel void ds_check_pos(__global const uchar *csbuf,
                           __global const uint  *csoff,
                           __global const uint  *cslen,
                           const uint  npos,
                           __global const uchar *prefix,
                           const uint  plen,
                           __global const uchar *suffix,
                           const uint  slen,
                           __global const uchar *target,
                           __global uchar *hits,
                           const ulong base,
                           const uint  n)
{
	ulong gid = get_global_id(0);
	ulong v;
	uchar u16[DS_MAX_UNITS * 2];
	uchar out[20];
	uint o = 0, i, c, p;
	uchar m = 1;

	if (gid >= n)
		return;

	v = base + gid;

	for (i = 0; i < plen; i++)
		u16[o++] = prefix[i];

	for (i = 0; i < npos; i++) {
		uint r = cslen[i];
		c = (uint)(v % (ulong)r);
		v /= (ulong)r;
		p = npos - 1 - i;                 /* fill from the right */
		u16[o + 2 * p] = csbuf[csoff[p] + c];
		u16[o + 2 * p + 1] = 0;
	}
	o += npos * 2;

	for (i = 0; i < slen; i++)
		u16[o++] = suffix[i];

	ds_deepsound_kdf(u16, o, out);

	for (i = 0; i < DS_TARGET_LEN; i++)
		if (out[i] != target[i]) { m = 0; break; }
	hits[gid] = m;
}

/*
 * Word/phrase mode: each position is a list of whole words (already UTF-16LE),
 * combined with a separator. Enumerates the cartesian product on-device.
 * Element tables: for position p, elements are elem[pbase[p] .. pbase[p]+pcount[p]).
 * Each element e has bytes at wbuf[eoff[e] .. eoff[e]+elen[e]).
 */
__kernel void ds_check_words(__global const uchar *wbuf,
                             __global const uint  *eoff,
                             __global const uint  *elen,
                             __global const uint  *pbase,
                             __global const uint  *pcount,
                             const uint  npos,
                             __global const uchar *sep,
                             const uint  seplen,
                             __global const uchar *prefix,
                             const uint  plen,
                             __global const uchar *suffix,
                             const uint  slen,
                             __global const uchar *target,
                             __global uchar *hits,
                             const ulong base,
                             const uint  n)
{
	ulong gid = get_global_id(0);
	ulong v;
	uchar u16[DS_MAX_UTF16_BYTES];
	uchar out[20];
	uint o = 0, i, j, e;
	uchar m = 1;

	if (gid >= n)
		return;
	v = base + gid;

	for (i = 0; i < plen && o < DS_MAX_UTF16_BYTES; i++)
		u16[o++] = prefix[i];

	for (i = 0; i < npos; i++) {
		uint r = pcount[i];
		e = pbase[i] + (uint)(v % (ulong)r);
		v /= (ulong)r;
		if (i) {
			for (j = 0; j < seplen && o < DS_MAX_UTF16_BYTES; j++)
				u16[o++] = sep[j];
		}
		for (j = 0; j < elen[e] && o < DS_MAX_UTF16_BYTES; j++)
			u16[o++] = wbuf[eoff[e] + j];
	}

	for (i = 0; i < slen && o < DS_MAX_UTF16_BYTES; i++)
		u16[o++] = suffix[i];

	ds_deepsound_kdf(u16, o, out);

	for (i = 0; i < DS_TARGET_LEN; i++)
		if (out[i] != target[i]) { m = 0; break; }
	hits[gid] = m;
}
