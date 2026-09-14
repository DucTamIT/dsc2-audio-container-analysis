#include <stdio.h>
#include <string.h>
#include "ds_kdf.h"

static void hex(const unsigned char *b, int n) { for (int i=0;i<n;i++) printf("%02x", b[i]); }
static int eq(const unsigned char *b, int n, const char *hx) {
    char s[128]; for (int i=0;i<n;i++) sprintf(s+i*2, "%02x", b[i]); s[n*2]=0;
    return strcmp(s, hx)==0;
}
int main(void) {
    unsigned char out[32];
    int ok = 1;
    ds_sha256((const unsigned char*)"abc", 3, out);
    printf("sha256(abc): "); hex(out,32); printf(" %s\n", eq(out,32,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")?"OK":"FAIL"); 
    ok &= eq(out,32,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    ds_sha1((const unsigned char*)"abc", 3, out);
    printf("sha1(abc)  : "); hex(out,20); printf(" %s\n", eq(out,20,"a9993e364706816aba3e25717850c26c9cd0d89d")?"OK":"FAIL");
    ok &= eq(out,20,"a9993e364706816aba3e25717850c26c9cd0d89d");

    unsigned char key[32], pt[16], ct[16];
    for (int i=0;i<32;i++) key[i]=(unsigned char)i;
    for (int i=0;i<16;i++) pt[i]=(unsigned char)(i*0x11);
    ds_u32 rk[60]; ds_aes256_setkey(key, rk); ds_aes256_encrypt(rk, pt, ct);
    printf("aes256 fips: "); hex(ct,16); printf(" %s\n", eq(ct,16,"8ea2b7ca516745bfeafc49904b496089")?"OK":"FAIL");
    ok &= eq(ct,16,"8ea2b7ca516745bfeafc49904b496089");

    struct { const char *u16hex; const char *want; } tv[] = {
        {"7400650073007400", "5af80536d41e826be11da7317f250ffaaa0eaf8b"},   /* test */
        {"6700610069006e00", "35a42fb91ff63d3bd4b4f318666f252c5d398b4f"},   /* gain */
        {"",                 "ccb7867e119a42b48fa97b19fb15634fd16e226e"},   /* empty */
        {"6800e100740020006200e900", "5b786f06b42bb9fc452f1c1cf8fb34e9f2a1bc15"}, /* hát bé */
    };
    for (unsigned t=0;t<sizeof(tv)/sizeof(tv[0]);t++) {
        unsigned char u16[128]; int n=0;
        for (const char *p=tv[t].u16hex; *p; p+=2) { unsigned v; sscanf(p,"%2x",&v); u16[n++]=v; }
        ds_deepsound_kdf(u16, n, out);
        printf("kdf(%-24s): ", tv[t].u16hex[0]?tv[t].u16hex:"<empty>"); hex(out,20);
        printf(" %s\n", eq(out,20,tv[t].want)?"OK":"FAIL");
        ok &= eq(out,20,tv[t].want);
    }
    printf("\n%s\n", ok ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return ok?0:1;
}
