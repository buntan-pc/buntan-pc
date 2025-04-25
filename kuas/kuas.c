#include "aclmini.c"

Auc *skpSpc(Auc *p)
{
	while (0 < *p && *p <= ' ') p++;
	return p;
}

int cmd(Auc *p, Auc **q, const char *s)
{
	Ai l = strlen(s);
	if (strncmp((AStr) p, s, l) == 0 && p[l] <= ' ') {
		if (q != 0)
			*q = skpSpc(p + l);
		return 1;
	}
	return 0;
}

AClass(Label) { Ai sec, ofs; };

Ai istk0; // 多分将来的に不要になるので、operand0()の引数には積まない.

int opr0Sub(Auc c) { return (c == '+' || c == '-' || c <= ' ' || c == ','); }

Ai operand0(Auc *p, Auc **pp, Ai *pbas, ATokenMgr *tokn, Label *lb)
{
	Ai bas = 0, i = 0, j, sgn;
	while (*p != '\0') {
		sgn = +1;
		if (*p == '+') p = skpSpc(p + 1);
		if (*p == '-') { p = skpSpc(p + 1); sgn = -1; }
		Auc *q = p;
		if (bas == 0 && p[0] == 'z' && opr0Sub(p[2])) { bas = 0; p++; }
		if (bas == 0 && p[0] == 'f' && p[1] == 'p' && opr0Sub(p[2]) && sgn > 0) { bas = 1; p += 2; }
		if (bas == 0 && p[0] == 'g' && p[1] == 'p' && opr0Sub(p[2]) && sgn > 0) { bas = 2; p += 2; }
		if (bas == 0 && strncmp((AStr) p, "zero", 4) == 0 && opr0Sub(p[4]))     { bas = 0; p += 4; }
		if (p > q) { p = skpSpc(p); continue; }
		j = strtol((AStr) q, (char **) &p, 0); // 定数.
		if (p > q) { p = skpSpc(p); i += sgn * j; continue; }
		for (j = 0; !opr0Sub(p[j]); j++); // ラベル参照.
		if (j == 4 && memcmp(p, "$top", 4) == 0) { i += sgn * istk0; p = skpSpc(p + j); continue; }
		if (j > 0) {
			i += sgn * lb[ATokenMgr_s2i(tokn, (AStr) p, j)].ofs;
			p = skpSpc(p + j);
			continue;
		}
		break;
	}
	*pbas = bas;
	if (pp != NULL) *pp = p;
	return i;
}

Ai operand1(Auc *p)
{
	if (p[0] == 'f' && p[1] == 'p'        && p[2] <= ' ') return 0;
	if (p[0] == 'g' && p[1] == 'p'        && p[2] <= ' ') return 1;
	if (strncmp((AStr) p, "isr", 3) == 0  && p[3] <= ' ') return 2;
	return 3;
}

int uAsmMain(Auc *src, AExpMem *pmem, AExpMem *dmem, ATokenMgr *tokn, AExpMem *labl, char *errMsg)
{
	Ai pass, i, im; Auc *p, *q, *src0 = src;
	Ai pmem0 = pmem->n, dmem0 = dmem->n;
	for (pass = 0; pass < 2; pass++) {
		Ai sec = -1, ofs[2], bas, isp = 15;
		int16_t istk[16], rhs; istk[isp] = 0;
		pmem->n = pmem0; dmem->n = dmem0;
		for (src = src0; *src != '\0'; ) {
			Auc linBuf[1000];
			while (*src <= ' ' && *src != '\n')
				src++; // 行頭のスペース類を読み飛ばす.
			p = (Auc *) strchr((AStr) src, '\n');
			i = p - src;
			if (i >= (Ai) sizeof linBuf) {
				sprintf(errMsg, "too long line (linBufSz=%d)\n", (int) sizeof linBuf);
				return 1;
			}
			memcpy(linBuf, src, i);
			src += i + 1;
			while (i > 0 && linBuf[i - 1] <= ' ')
				i--; // 行末のスペース類を取り除く.
			linBuf[i] = '\0';
			p = linBuf;

			if (*p == '\0') continue; // 空行.
			if (cmd(p, &q, "section")) {
				sec = -1;
				if (cmd(q, 0, ".text")) sec = 0;
				if (cmd(q, 0, ".data")) sec = 1;
				continue;
			}

			AExpMem_rsv(labl, ASz(Label) * (tokn->n + 9));
			AExpMem_rsv(pmem, ASz(int32_t) * 9);
			AExpMem_rsv(dmem, ASz(Auc) * 9999);
			int32_t *ppm = (int32_t *) (pmem->p + pmem->n);
			Auc     *pdm = dmem->p + dmem->n;
			Label   *lb  = (Label *) labl->p;
			istk0 = istk[isp&15];
			if (cmd(p, &q, ".push") && *q != '\0') { istk[(--isp)&15] = operand0(q, &q, &bas, tokn, lb); continue; }
			if (cmd(p, &q, ".add")  && *q == '\0') { rhs = istk[(isp++)&15]; istk[isp&15] += rhs; continue; }
			if (cmd(p, &q, ".sub")  && *q == '\0') { rhs = istk[(isp++)&15]; istk[isp&15] -= rhs; continue; }
			if (cmd(p, &q, ".mul")  && *q == '\0') { rhs = istk[(isp++)&15]; istk[isp&15] *= rhs; continue; }
			if (cmd(p, &q, ".sign") && *q == '\0') { istk[isp&15] ^= 0x8000; continue; }
			if (cmd(p, &q, ".eq")   && *q == '\0') { rhs = istk[(isp++)&15]; istk[isp&15] = (istk[isp&15] == rhs); continue; }
			if (cmd(p, &q, ".neq")  && *q == '\0') { rhs = istk[(isp++)&15]; istk[isp&15] = (istk[isp&15] != rhs); continue; }
			if (cmd(p, &q, ".lt")   && *q == '\0') { rhs = istk[(isp++)&15]; istk[isp&15] = (istk[isp&15] <  rhs); continue; }
			if (cmd(p, &q, ".le")   && *q == '\0') { rhs = istk[(isp++)&15]; istk[isp&15] = (istk[isp&15] <= rhs); continue; }
			if (cmd(p, &q, ".bt")   && *q == '\0') { rhs = istk[(isp++)&15]; istk[isp&15] = ((uint16_t) istk[isp&15] <  (uint16_t) rhs); continue; }
			if (cmd(p, &q, ".be")   && *q == '\0') { rhs = istk[(isp++)&15]; istk[isp&15] = ((uint16_t) istk[isp&15] <= (uint16_t) rhs); continue; }
			if (cmd(p, &q, ".not")  && *q == '\0') { istk[isp&15] ^= -1; continue; }
			if (cmd(p, &q, ".and")  && *q == '\0') { rhs = istk[(isp++)&15]; istk[isp&15] &= rhs; continue; }
			if (cmd(p, &q, ".or")   && *q == '\0') { rhs = istk[(isp++)&15]; istk[isp&15] |= rhs; continue; }
			if (cmd(p, &q, ".xor")  && *q == '\0') { rhs = istk[(isp++)&15]; istk[isp&15] ^= rhs; continue; }
			if (cmd(p, &q, ".shr")  && *q == '\0') { rhs = istk[(isp++)&15]; istk[isp&15] = (uint16_t) istk[isp&15] >> rhs; continue; }
			if (cmd(p, &q, ".sar")  && *q == '\0') { rhs = istk[(isp++)&15]; istk[isp&15] = (int16_t)  istk[isp&15] >> rhs; continue; }
			if (cmd(p, &q, ".shl")  && *q == '\0') { rhs = istk[(isp++)&15]; istk[isp&15] <<= rhs; continue; }
			if (sec < 0) continue;

			if (cmd(p, &q, "db") && sec == 1 && *q != '\0') {
				for (;;) {
					im = operand0(q, &q, &bas, tokn, lb);
					*pdm++ = im & 0xff;
					dmem->n++;
					if (*q != ',') break;
					q = skpSpc(q + 1);
				}
				continue;
			}
			if (cmd(p, &q, "push") && sec == 0 && *q != '\0') {
				im = operand0(q, 0, &bas, tokn, lb);
				if (bas == 0) {
					*ppm = 0x30000 | (im & 0xffff);
				} else {
					*ppm = 0x14000 | bas << 12 | (im & 0xfff);
				}
nextPmem:
				pmem->n += ASz(int32_t);
				continue;
			}
			if (cmd(p, &q, "add") && sec == 0 && q[0] == 'f' && q[1] == 'p' && q[2] == ',') {
				im = operand0(skpSpc(q + 3), 0, &bas, tokn, lb);
				if (bas == 0) {
					*ppm = 0x05000 | (im & 0xfff);
					goto nextPmem;
				}
			}
			if (cmd(p, &q, "dup") && sec == 0 && *q != '\0') {
				im = operand0(q, 0, &bas, tokn, lb);
				if (im == 0) { *ppm = 0x1c080; goto nextPmem; }
				if (im == 1) { *ppm = 0x1c08f; goto nextPmem; }
			}
			AClass(OpecodeTable) { AStr s; int32_t op; };
			As OpecodeTable ot0[] = {
				{ "ld1", 0x08000 }, { "st1",  0x0c000 }, { "ld", 0x10000 }, { "st",  0x10001 }, { "", 0 }
			};
			for (i = 0; ot0[i].s[0] != '\0'; i++) {
				if (cmd(p, &q, ot0[i].s)) break;
			}
			if (ot0[i].s[0] != '\0' && sec == 0 && *q != '\0') {
				im = operand0(q, 0, &bas, tokn, lb);
				*ppm = ot0[i].op | bas << 12 | (im & 0xfff);
				goto nextPmem;
			}
			As OpecodeTable ot1[] = {
				{ "call", 0x00000 }, { "jmp", 0x04000 }, { "jz", 0x06000 }, { "", 0 }
			};
			for (i = 0; ot1[i].s[0] != '\0'; i++) {
				if (cmd(p, &q, ot1[i].s)) break;
			}
			if (ot1[i].s[0] != '\0' && sec == 0 && *q != '\0') {
				im = operand0(q, 0, &bas, tokn, lb);
				if (bas == 0) {
					Ai ip = pmem->n / ASz(int32_t) + 1, msk = 0x3fff;
					if (i > 0) msk = 0xfff;
					*ppm = ot1[i].op | ((im - ip) & msk);
					goto nextPmem;
				}
			}
			As OpecodeTable ot2[] = {
				{ "nop",  0x1c000 }, { "inc",  0x1c001 }, { "inc2", 0x1c002 }, { "not",  0x1c004 },
				{ "sign", 0x1c005 }, { "exts", 0x1c006 }, { "and",  0x1c050 }, { "or",   0x1c051 },
				{ "xor",  0x1c052 }, { "shr",  0x1c054 }, { "sar",  0x1c055 }, { "shl",  0x1c056 },
				{ "add",  0x1c060 }, { "sub",  0x1c061 }, { "mul",  0x1c062 }, { "eq",   0x1c068 },
				{ "neq",  0x1c069 }, { "lt",   0x1c06a }, { "le",   0x1c06b }, { "bt",   0x1c06c },
				{ "be",   0x1c06d }, { "ret",  0x1c800 }, { "ldd",  0x1c808 }, { "ldd1", 0x1c809 },
				{ "sta",  0x1c80c }, { "sta1", 0x1c80d }, { "std",  0x1c80e }, { "std1", 0x1c80f },
				{ "int",  0x1c810 }, { "iret", 0x1c812 }, { "spha", 0x1c824 }, { "spla", 0x1c825 },
				{ "dup",  0x1c080 }, { "pop",  0x1c04f }, { "call", 0x1c801 },
				{ "",     0 }
			};
			for (i = 0; ot2[i].s[0] != '\0'; i++) {
				if (cmd(p, &q, ot2[i].s)) break;
			}
			if (ot2[i].s[0] != '\0' && sec == 0 && *q == '\0') {
				*ppm = ot2[i].op;
				goto nextPmem;
			}
			if (cmd(p, &q, "pop") && sec == 0 && *q > 0) {
				*ppm = 0x1c820 | operand1(q);
				goto nextPmem;
			}
			if ((q = (Auc *) strchr((AStr) p, ':')) != NULL) {	// def-label.
				i = ATokenMgr_s2i(tokn, (AStr) p, q - p);
				ofs[0] = pmem->n / ASz(int32_t);
				ofs[1] = dmem->n;
				lb[i].sec = sec; lb[i].ofs = ofs[sec]; continue;
			}

			if (pass == 0) {
				sprintf(errMsg, "mikan(abort): %s\n", p); 
				return 1;
			}
		}
	}
	return 0;
}

#define UseMlcNumSz		0

int main(int argc, const char **argv)
{
	#if (UseMlcNumSz != 0)
		AMlcStdNumSz m0[1];	AMlc_m0v = AMlcStdNumSz_vt; AMlc_m0 = AMlcStdNumSz_inv(m0, AMlc_m0v);
	#else
		AMlcStd m0[1];	AMlc_m0v = AMlcStd_vt; AMlc_m0 = AMlcStd_inv(m0, AMlc_m0v);
	#endif

	const char *exe = NULL, *inp = NULL, *map = NULL, *dmf = NULL, *pmf = NULL;
	Ai i, j, pmsz;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-o")     == 0) { exe = argv[++i]; continue; }
		if (strcmp(argv[i], "--map")  == 0) { map = argv[++i]; continue; }
		if (strcmp(argv[i], "--dmem") == 0) { dmf = argv[++i]; continue; }
		if (strcmp(argv[i], "--pmem") == 0) { pmf = argv[++i]; continue; }
		if (inp == NULL) { inp = argv[i]; continue; }
		fprintf(stderr, "multiple inputs are not supported: '%s'\n", argv[i]);
		return 1;
	}
	AExpMem src[1], pmem[1], dmem[1], labl[1];
	AExpMem_ini(src); AExpMem_ini(pmem); AExpMem_ini(dmem); AExpMem_ini(labl);
	FILE *fp = stdin;
	if (inp != NULL && strcmp(inp, "-") != 0)
		fp = fopen(inp, "rb");
	if (fp == NULL) { fprintf(stderr, "fopen error : %s\n", inp); exit(1); }
	do {
		AExpMem_rsv(src, 65536);
		i = fread(src->p + src->n, 1, 65536, fp);
		src->n += i;
	} while (i > 0);
	if (fp != stdin) fclose(fp);
	AExpMem_rsv(src, 2);
	src->p[src->n++] = '\n'; // LFのない行はない.
	src->p[src->n++] = '\0';

	char errMsg[256];
	ATokenMgr tokn[1]; ATokenMgr_ini(tokn);
	if (exe != NULL) {
		AExpMem_rsv(dmem, 4);
		dmem->n = 4;
	}
	i = uAsmMain(src->p, pmem, dmem, tokn, labl, errMsg);
	if (i != 0) fprintf(stderr, "%s\n", errMsg);
	fprintf(stderr, "size of .data: %d bytes\n", (int) dmem->n);
	fprintf(stderr, "size of .text: %d instructions\n", (int) (pmsz = pmem->n / ASz(int32_t)));
	if (dmf != NULL) {
		fp = fopen(dmf, "wt");
		for (i = 0; i < dmem->n; i += 2)
			fprintf(fp, "%02X%02X\n", dmem->p[i + 1], dmem->p[i]);
		fclose(fp);
	}
	if (pmf != NULL) {
		fp = fopen(pmf, "wt");
		for (i = 0; i < pmem->n; i += ASz(int32_t))
			fprintf(fp, "%05X\n", *(int32_t *)(pmem->p + i));
		fclose(fp);
	}
	if (exe != NULL) {
		dmem->p[0] =  pmsz       & 0xff;
		dmem->p[1] = (pmsz >> 8) & 0xff;
		dmem->p[2] =  dmem->n       & 0xff;
		dmem->p[3] = (dmem->n >> 8) & 0xff;
		AExpMem_rsv(dmem, 512);
		memset(dmem->p + dmem->n, 0, 512);
		fp = fopen(exe, "wb");
		if (fp == NULL) { fprintf(stderr, "fopen error : %s\n", exe); exit(1); }
		fwrite(dmem->p, 1, (dmem->n + 511) & -512, fp);
		Auc *pc = AMlc_alc(AMlc_m0, AMlc_m0v, pmsz * 3);
		int32_t *pi = (int32_t *) pmem->p;
		for (i = j = 0; i < pmsz; i++, j += 3) {
			pc[j]     =  pi[i]        & 0xff;
			pc[j + 1] = (pi[i] >>  8) & 0xff;
			pc[j + 2] = (pi[i] >> 16) & 0xff;
		}
		fwrite(pc, 1, j, fp);
		fclose(fp);
		AMlc_fre(AMlc_m0, AMlc_m0v, pc, j);
	}
	if (map != NULL) {
		fp = fopen(map, "wt");
		if (fp == NULL) { fprintf(stderr, "fopen error : %s\n", map); exit(1); }
		Label *lb  = (Label *) labl->p;
		for (i = 0; i < tokn->n; i++)
			fprintf(fp, "#%04d sec=%d ofs=0x%04x : %s\n", (int) i, (int) lb[i].sec, (int) lb[i].ofs, ATokenMgr_i2s(tokn, i));
		fclose(fp);
	}
	ATokenMgr_dst(tokn);
	AExpMem_dst(src); AExpMem_dst(pmem); AExpMem_dst(dmem); AExpMem_dst(labl);

	#if (UseMlcNumSz != 0)
		fprintf(stderr, "m0.n=%d, m0.s=%d\n", m0->n, m0->s);
	#endif

	return 0;
}
