#include "aclmini.c"

AClass(Label) { Ai sec, ofs; };

Ai istk0; // 多分将来的に不要になるので、operand0()の引数には積まない.

Auc *skpSpc(Auc *p) { while (0 < *p && *p <= ' ') p++; return p; }
#define cmd(s, t)	(strcmp(s, t) == 0)
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
	Ai pass, i, im = 0; Auc *p, *q, *src0 = src;
	Ai pmem0 = pmem->n, dmem0 = dmem->n;
	for (pass = 0; pass < 2; pass++) {
		Ai sec = -1, ofs[2], bas, isp = 15;
		int16_t istk[16]; istk[isp] = 0; istk[(isp + 1) & 15] = 0;
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

			AExpMem_rsv(labl, ASz(Label) * (tokn->n + 9));
			AExpMem_rsv(pmem, ASz(int32_t) * 9);
			AExpMem_rsv(dmem, ASz(Auc) * 9999);
			int32_t *ppm = (int32_t *) (pmem->p + pmem->n);
			Auc     *pdm = dmem->p + dmem->n;
			Label   *lb  = (Label *) labl->p;
			istk0 = istk[isp & 15];

			if ((q = (Auc *) strchr((AStr) p, ':')) != NULL && sec >= 0) {	// def-label.
				i = ATokenMgr_s2i(tokn, (AStr) p, q - p);
				ofs[0] = pmem->n / ASz(int32_t);
				ofs[1] = dmem->n;
				lb[i].sec = sec; lb[i].ofs = ofs[sec]; continue;
			}

			char r[16];
			for (i = 0; i < 14 && p[i] > ' '; i++)
				r[i + 1] = p[i];
			r[i + 1] = '\0';
			q = skpSpc(p + i);
			r[0] = '0';
			if (*q != '\0') {
				r[0] = '2';
				if (q[0] == 'f' && q[1] == 'p' && q[2] == ',') { q = skpSpc(q + 3); r[0] = '4'; }
				if (cmd(r, "2section")) { // 特別なオペランドを持つ命令.
					sec = -1;
					if (cmd((AStr) q, ".text")) sec = 0;
					if (cmd((AStr) q, ".data")) sec = 1;
					continue;
				}
				if (cmd(r, "2pop")) { *ppm = 0x1c820 | operand1(q); goto next; }
				im = operand0(q, 0, &bas, tokn, lb); // 一般的なオペランドを持つ命令.
				if (bas != 0) r[0]++;
			}

			if (cmd(r, "2db") && sec == 1) {
				for (;;) {
					im = operand0(q, &q, &bas, tokn, lb);
					*pdm++ = im & 0xff;
					dmem->n++;
					if (*q != ',') break;
					q = skpSpc(q + 1);
				}
				continue;
			}

			int16_t rhs = istk[isp & 15], lhs = istk[(isp + 1) & 15];
			uint16_t rhu = (uint16_t) rhs, lhu = (uint16_t) lhs;
			if (cmd(r, "2.push")) { istk[(--isp) & 15] = im; continue; }
			if (cmd(r, "0.sign")) { istk[isp & 15] ^= 0x8000; continue; }
			if (cmd(r, "0.not"))  { istk[isp & 15] ^= -1; continue; }
			if (cmd(r, "0.add"))  { istk[(++isp) & 15] =  lhs + rhs;   continue; }
			if (cmd(r, "0.sub"))  { istk[(++isp) & 15] =  lhs - rhs;   continue; }
			if (cmd(r, "0.mul"))  { istk[(++isp) & 15] =  lhs * rhs;   continue; }
			if (cmd(r, "0.eq"))   { istk[(++isp) & 15] = (lhs == rhs); continue; }
			if (cmd(r, "0.neq"))  { istk[(++isp) & 15] = (lhs != rhs); continue; }
			if (cmd(r, "0.lt"))   { istk[(++isp) & 15] = (lhs <  rhs); continue; }
			if (cmd(r, "0.le"))   { istk[(++isp) & 15] = (lhs <= rhs); continue; }
			if (cmd(r, "0.bt"))   { istk[(++isp) & 15] = (lhu <  rhu); continue; }
			if (cmd(r, "0.be"))   { istk[(++isp) & 15] = (lhu <= rhu); continue; }
			if (cmd(r, "0.and"))  { istk[(++isp) & 15] =  lhs &  rhs;  continue; }
			if (cmd(r, "0.or"))   { istk[(++isp) & 15] =  lhs |  rhs;  continue; }
			if (cmd(r, "0.xor"))  { istk[(++isp) & 15] =  lhs ^  rhs;  continue; }
			if (cmd(r, "0.shr"))  { istk[(++isp) & 15] =  lhu >> rhs;  continue; }
			if (cmd(r, "0.sar"))  { istk[(++isp) & 15] =  lhs >> rhs;  continue; }
			if (cmd(r, "0.shl"))  { istk[(++isp) & 15] =  lhs << rhs;  continue; }
			if (sec != 0) goto err;

			AClass(OpecodeTable) { AStr s; int32_t op; };
			As OpecodeTable ot[] = {
				{ "2ld1",  0x01008000 }, { "2st1",  0x0100c000 }, { "2ld",   0x01010000 }, { "2st",   0x01010001 }, // group 01
				{ "3ld1",  0x01008000 }, { "3st1",  0x0100c000 }, { "3ld",   0x01010000 }, { "3st",   0x01010001 },

				{ "4add",  0x01005000 }, { "2push", 0x02030000 }, { "3push", 0x01014000 }, { "2dup",  0x0301c080 }, // group 01-03

				{ "2call", 0x04000000 }, { "2jmp",  0x05004000 }, { "2jz",   0x05006000 },                          // group 04-05

				{ "0nop",  0x0001c000 }, { "0inc",  0x0001c001 }, { "0inc2", 0x0001c002 }, { "0not",  0x0001c004 }, // group 00
				{ "0sign", 0x0001c005 }, { "0exts", 0x0001c006 }, { "0pop",  0x0001c04f }, { "0and",  0x0001c050 },
				{ "0or",   0x0001c051 }, { "0xor",  0x0001c052 }, { "0shr",  0x0001c054 }, { "0sar",  0x0001c055 },
				{ "0shl",  0x0001c056 }, { "0add",  0x0001c060 }, { "0sub",  0x0001c061 }, { "0mul",  0x0001c062 },
				{ "0eq",   0x0001c068 }, { "0neq",  0x0001c069 }, { "0lt",   0x0001c06a }, { "0le",   0x0001c06b },
				{ "0bt",   0x0001c06c }, { "0be",   0x0001c06d }, { "0dup",  0x0001c080 },
				{ "0ret",  0x0001c800 }, { "0call", 0x0001c801 }, { "0ldd",  0x0001c808 }, { "0ldd1", 0x0001c809 },
				{ "0sta",  0x0001c80c }, { "0sta1", 0x0001c80d }, { "0std",  0x0001c80e }, { "0std1", 0x0001c80f },
				{ "0int",  0x0001c810 }, { "0iret", 0x0001c812 }, { "0spha", 0x0001c824 }, { "0spla", 0x0001c825 },
				{ "", -1 }
			};
			for (i = 0; ot[i].op != -1; i++) {
				if (cmd(r, ot[i].s)) break;
			}
			Ai grp = ot[i].op >> 24, op = ot[i].op & 0xffffff;
			if (grp == 0x00) {
				*ppm = op;
next:
				pmem->n += ASz(int32_t);
				continue;
			}
			if (grp == 0x01) { *ppm = op | bas << 12 | (im & 0xfff); goto next; }
			if (grp == 0x02) { *ppm = op | (im & 0xffff); goto next; } // push
			if (grp == 0x03) { *ppm = op | ((-im) & 0xf); goto next; } // dup 0 / dup 1
			if (grp == 0x04) { Ai ip = pmem->n / ASz(int32_t) + 1; *ppm = op | ((im - ip) & 0x3fff); goto next; } // call
			if (grp == 0x05) { Ai ip = pmem->n / ASz(int32_t) + 1; *ppm = op | ((im - ip) & 0x0fff); goto next; } // jmp / jz
err:
			sprintf(errMsg, "mikan(abort): %s\n", p); 
			return 1;
		}
	}
	return 0;
}

#define UseMlcNumSz		0

FILE *myOpen(const char *path, const char *mod)
{
	FILE *fp = fopen(path, mod);
	if (fp == NULL) { fprintf(stderr, "fopen error : %s\n", path); exit(1); }
	return fp;
}

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
		fp = myOpen(inp, "rb");
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
	int errCode = uAsmMain(src->p, pmem, dmem, tokn, labl, errMsg);
	if (errCode != 0) fprintf(stderr, "%s\n", errMsg);
	fprintf(stderr, "size of .data: %d bytes\n", (int) dmem->n);
	fprintf(stderr, "size of .text: %d instructions\n", (int) (pmsz = pmem->n / ASz(int32_t)));
	if (dmf != NULL) {
		fp = myOpen(dmf, "wt");
		for (i = 0; i < dmem->n; i += 2)
			fprintf(fp, "%02X%02X\n", dmem->p[i + 1], dmem->p[i]);
		fclose(fp);
	}
	if (pmf != NULL) {
		fp = myOpen(pmf, "wt");
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
		fp = myOpen(exe, "wb");
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
		fp = myOpen(map, "wt");
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

	return errCode;
}
