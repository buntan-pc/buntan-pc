#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef intptr_t Ai;
typedef void *Ap;
typedef void Av;
typedef unsigned char Auc;
typedef signed char Asc;
typedef char *AStr;
typedef const char *ACStr;
#define As	static
#define AClass(nam)    typedef struct nam ## _ nam; struct nam ## _
#define Asz(typ) ( (Ai) sizeof (typ) )
#define Act	continue

As Ap AdsvNop(Ap w, Ap v) { (void) v; return w; }
As Ap AinvNop(Ap w, Ap v) { (void) v; return w; }

AClass(AM) { Auc dmy; };
AClass(AM_Vt) { Ap (*dsv)(Ap,Ap); Ai objSz; Ap (*inv)(Ap,Ap); Ap (*alc)(AM*,Ai); Av (*fre)(AM*,Ap,Ai); Ap (*rlc)(AM*,Ap,Ai,Ai); };
As Ap AM_alc(AM *w, AM_Vt *v, Ai s) { return v->alc(w, s); }
As Av AM_fre(AM *w, AM_Vt *v, Ap p, Ai s) { v->fre(w, p, s); }
As Ap AM_rlc(AM *w, AM_Vt *v, Ap p, Ai s0, Ai s1) { return v->rlc(w, p, s0, s1); }
As Ap Adsv(Ap w, AM_Vt *v) { v->dsv(w, v); return w; }
As Av AM_del(AM *w, AM_Vt *v, Ap p, AM_Vt *pv) { p = Adsv(p, pv); if (p != 0) { AM_fre(w, v, p, pv->objSz); } }
As Ap AM_new(AM *w, AM_Vt *v, AM_Vt *pv) { Ap p = AM_alc(w, v, pv->objSz); return pv->inv(p, pv); }
As AM *AM_m0;
As AM_Vt *AM_m0v;

AClass(AM_Std) { AM w; };
As Ap AM_Std_alc(AM *w, Ai s) { (void) w; return malloc(s); }
As Av AM_Std_fre(AM *w, Ap p, Ai s) { (void) w; (void) s; free(p); }
As Ap AM_Std_rlc(AM *w, Ap p, Ai s0, Ai s1) { (void) w; (void) s0; return realloc(p, s1); }
#define AM_Std_inv AinvNop
As AM_Vt AM_Std_vt[1] = {{ AdsvNop, Asz(AM_Std), AM_Std_inv, AM_Std_alc, AM_Std_fre, AM_Std_rlc }};

AClass(AM_StdNumSz) { AM w; Ai n, s; };
As Ap AM_StdNumSz_alc(AM_StdNumSz *w, Ai s) { w->n++; w->s += s; return malloc(s); }
As Av AM_StdNumSz_fre(AM_StdNumSz *w, Ap p, Ai s) { w->n--; w->s -= s; free(p); }
As Ap AM_StdNumSz_rlc(AM_StdNumSz *w, Ap p, Ai s0, Ai s1) { w->s += s1 - s0; return realloc(p, s1); }
As AM *AM_StdNumSz_inv(AM_StdNumSz *w, Ap v) { (void) v; w->n = w->s = 0; return (Ap) w; }
As AM_Vt AM_StdNumSz_vt[1] = {{ AdsvNop, Asz(AM_StdNumSz), (Ap) AM_StdNumSz_inv, (Ap) AM_StdNumSz_alc, (Ap) AM_StdNumSz_fre, (Ap) AM_StdNumSz_rlc }};

AClass(AX) { Auc *p; Ai n, n1; AM *m; AM_Vt *mv; };
#define AX_rsv(w, sz)	if ((w)->n + (sz) >= (w)->n1) AX_setMargin((w), (sz));
As AX *AX_ini(AX *w) { w->p = 0; w->n = w->n1 = 0; w->m = AM_m0; w->mv = AM_m0v; return w; }
As Ap AX_dst(AX *w) { if (w->p != 0) { AM_fre(w->m, w->mv, w->p, w->n1); } return w; }

As Av AX_setMargin(AX *w, Ai sz)
{
	Ai n1 = w->n1, n2 = w->n + sz;
	if (n1 == 0) {
		w->n1 = n1 = 256;
		w->p = AM_alc(w->m, w->mv, n1);
	}
	while (n2 > n1)
		n1 <<= 1;
	if (n1 > w->n1) {
		w->p = AM_rlc(w->m, w->mv, w->p, w->n1, n1);
		w->n1 = n1;
	}
}

As Av AX_ini4(AX *x0, AX *x1, AX *x2, AX *x3)
{
	if (x0 != NULL) AX_ini(x0);
	if (x1 != NULL) AX_ini(x1);
	if (x2 != NULL) AX_ini(x2);
	if (x3 != NULL) AX_ini(x3);
}

As Av AX_dst4(AX *x0, AX *x1, AX *x2, AX *x3)
{
	if (x0 != NULL) AX_dst(x0);
	if (x1 != NULL) AX_dst(x1);
	if (x2 != NULL) AX_dst(x2);
	if (x3 != NULL) AX_dst(x3);
}

As Av AX_apnPtr(AX *x, Ap p) { AX_rsv(x, Asz(Ap)); *((Ap *)(x->p + x->n)) = p; x->n += Asz(Ap); }

As inline Av AputBytes(Auc *p, Ai i, Ai s0, Ai s1, Ai s2, Ai s3)
{
	if (s0 >= 0) p[0] = (i >> s0) & 255;
	if (s1 >= 0) p[1] = (i >> s1) & 255;
	if (s2 >= 0) p[2] = (i >> s2) & 255;
	if (s3 >= 0) p[3] = (i >> s3) & 255;
}

As inline Ai AgetBytes(Auc *p, Ai s0, Ai s1, Ai s2, Ai s3)
{
	Ai i = 0;
	if (s0 >= 0) i  = p[0] << s0;
	if (s1 >= 0) i |= p[1] << s1;
	if (s2 >= 0) i |= p[2] << s2;
	if (s3 >= 0) i |= p[3] << s3;
	return i;
}

AClass(ATokenMgr) { AX t0[1], t1[1]; AM *m, *tm; AM_Vt *mv, *tmv; Ai n; };
As AStr ATokenMgr_i2s(ATokenMgr *w, Ai i) { return ((AStr *)w->t0->p)[i]; }

As Ap ATokenMgr_dst(ATokenMgr *w)
{
	Ai n = w->n; AM *m = w->m; AM_Vt *mv = w->mv; AStr *t0 = (AStr *) w->t0->p;
	while (n > 0) {
		Auc *u = (Auc *) t0[--n];
		AM_fre(m, mv, u - 8, AgetBytes(u - 4, 24, 16, 8, 0) + 9);
	}
	AX_dst4(w->t0, w->t1, 0, 0);
	return w;
}

As Ai ATokenMgr_s2i(ATokenMgr *w, ACStr s, Ai l)
{
	char buf[64];
	AStr t = buf, p, *t1 = (AStr *) w->t1->p; Ai i, a, b, c, m;
	Auc *u;
	if (l >= 64)
		t = AM_alc(w->tm, w->tmv, l + 1);
	memcpy(t, s, l);
	t[l] = '\0';

	i = 0; if (w->n == 0) goto ins;
	c = strcmp(t, p = t1[0]); if (c == 0) goto fin;
	if (c < 0) goto ins;
	i = 1; if (w->n == 1) goto ins;
	c = strcmp(t, p = t1[w->n - 1]); if (c == 0) goto fin;
	i = w->n; if (c > 0) goto ins;
	a = 0; b = w->n - 1; m = a + ((b - a) >> 1);
	i = b; if (m == a) goto ins;
	do {
		c = strcmp(t, p = t1[m]); if (c == 0) goto fin;
		if (c < 0) { b = m; } else { a = m; }
		m = a + ((b - a) >> 1);
		i = b;
	} while (m > a);
ins:
	u = AM_alc(w->m, w->mv, l + 9);
	memcpy(p = (AStr) u + 8, t, l); p[l] = 0;
	AX_apnPtr(w->t0, p); a = w->n++;
	AX_rsv(w->t1, Asz(AStr)); t1 = (AStr *) w->t1->p;
	AputBytes(u, a, 24, 16, 8, 0); AputBytes(u + 4, l, 24, 16, 8, 0);
	if (i < a) { memmove(&t1[i + 1], &t1[i], (a - i) * Asz(AStr)); }
	t1[i] = p; w->t1->n += Asz(AStr);
fin:
	if (l >= 64) { AM_fre(w->tm, w->tmv, t, l + 1); }
	return AgetBytes((Auc *) p - 8, 24, 16, 8, 0);
}

As ATokenMgr *ATokenMgr_ini(ATokenMgr *w)
{
	w->m = w->tm = AM_m0; w->mv = w->tmv = AM_m0v; w->n = 0; AX_ini4(w->t0, w->t1, 0, 0); // ini直後にmを変更しやすいように、この時点ではn1=0.
	return w;
}

As FILE *AOpen(ACStr path, ACStr mod)
{
	if (path == NULL) { fprintf(stderr, "fopen error (path==NULL) : mod='%s'\n", mod); exit(1); }
	FILE *fp = fopen(path, mod);
	if (fp == NULL) { fprintf(stderr, "fopen error : %s\n", path); exit(1); }
	return fp;
}

As Ai AX_fread(AX *x, FILE *fp)
{
	Ai i, n0 = x->n;
	do {
		AX_rsv(x, 65536);
		i = fread(x->p + x->n, 1, 65536, fp);
		x->n += i;
	} while (i > 0);
	return x->n - n0;
}

As Av AX_putc(AX *x, Ai c) { AX_rsv(x, 1); x->p[x->n++] = c; }
As Av AX_puts0(AX *x, ACStr s, Ai l) { AX_rsv(x, l); if (l > 0) { memcpy(x->p + x->n, s, l); x->n += l; } }
As Av AX_puts(AX *x, ACStr s) { AX_puts0(x, s, strlen(s)); }

As Av AX_putBtyes(AX *x, Ai i, Ai s0, Ai s1, Ai s2, Ai s3)
{
	if (s0 >= 0) AX_putc(x, (i >> s0) & 255);
	if (s1 >= 0) AX_putc(x, (i >> s1) & 255);
	if (s2 >= 0) AX_putc(x, (i >> s2) & 255);
	if (s3 >= 0) AX_putc(x, (i >> s3) & 255);
}

As Ai AdelCr(AStr s, Ai l)
{
	Ai i, j;
	for (i = j = 0; i < l; i++) {
		if (s[i] != '\r') { s[j++] = s[i]; }
	}
	return j;
}

As Av AX_fread1(AX *x, ACStr path, Ai flg)
{
	if (path != NULL && path[0] == '-' && path[1] == '\0') {
		AX_fread(x, stdin);
	} else {
		FILE *fp = AOpen(path, "rb");
		AX_fread(x, fp);
		fclose(fp);
	}
	if ((flg & 4) != 0) { x->n = AdelCr((AStr) x->p, x->n); }
	if ((flg & 2) != 0) AX_putc(x, '\n');
	if ((flg & 1) != 0) { AX_rsv(x, 16); memset(x->p + x->n, 0, 16); x->n += 16; }
}

As Av AX_fwrite1(AX *x, ACStr path, ACStr mod)
{
	if (path != NULL && path[0] == '-' && path[1] == '\0') {
		fwrite(x->p, 1, x->n, stdout);
	} else {
		FILE *fp = AOpen(path, mod);
		fwrite(x->p, 1, x->n, fp);
		fclose(fp);
	}
}

As Av AX_gets0(AX *x, ACStr p0, Ai l)
{
	x->n = 0;
	AX_rsv(x, l);
	if (l > 0)
		memcpy(x->p, p0, l);
	x->n = l;
}

As AStr Astrstr1(ACStr s, ACStr t, Ai flg)
{
	ACStr p = strstr(s, t);
	if ((flg & 2) != 0 && p != NULL) p += strlen(t);
	if ((flg & 1) != 0 && p == NULL) p = s + strlen(s);
	return (AStr) p;
}

As AStr Astrchr1(ACStr s, char c, Ai flg)
{
	ACStr p = strchr(s, c);
	if ((flg & 2) != 0 && p != NULL) p++;
	if ((flg & 1) != 0 && p == NULL) p = s + strlen(s);
	return (AStr) p;
}

As AStr AX_gets(AX *x, ACStr s)
{
	ACStr s1 = Astrchr1(s, '\n', 3);
	AX_gets0(x, s, s1 - s);
	AX_putc(x, '\0');
	return (AStr) s1;
}

As AStr AskpSpc(ACStr s)
{
	while ('\0' < *s && *s <= ' ') s++;
	return (AStr) s;
}

As AStr AliteralEnd(ACStr s, char t)
{
	for (;;) {
		char c = *s;
		if (c == '\n' || c == '\0' || c == t) break;
		s++;
		if (c == '\\' && *s == t) s++;
	}
	return (AStr) s;
}

As AStr AparenthesisEnd(ACStr s, char p0, char p1, Ai flg)
{
	Ai k = 0;
	do {
		char c = *s++;
		if (c == '\0') return NULL;
		if (c == p0) k++;
		if (c == p1) k--;
		if ((flg & 1) != 0 && (c == 0x22 || c == 0x27)) {
			s = AliteralEnd(s, c);
			if ((flg & 2) == 0 && *s != c) return NULL;
			if (*s != '\0') s++;
		}
		if ((flg & 4) != 0 && c == '/' && *s == '/')
			s = Astrchr1(s, '\n', 3);
	} while (k > 0);
	return (AStr) s;
}

As AStr AparenthesisEnd1(ACStr s, Ai flg)
{
	if (*s == '(') return AparenthesisEnd(s, '(', ')', flg);
	if (*s == '[') return AparenthesisEnd(s, '[', ']', flg);
	if (*s == '{') return AparenthesisEnd(s, '{', '}', flg);
	if (*s == '<') return AparenthesisEnd(s, '<', '>', flg);
	return NULL;
}

As Ai AnamLen(ACStr s)
{
	Ai l = 1;
	Auc c = (Auc) *s;
	if (!('A' <= c && c <= 'Z') && !('a' <= c && c <= 'z') && c != '_') return 0;
	for (; (c = (Auc) s[l]) != '\0'; l++) {
		if (!('A' <= c && c <= 'Z') && !('a' <= c && c <= 'z') && !('0' <= c && c <= '9') && c != '_') break;
	}
	return l;
}

As Ai AnamLen1(ACStr s, ACStr t)
{
	Ai l = 1;
	Auc c = (Auc) *s;
	if (!('A' <= c && c <= 'Z') && !('a' <= c && c <= 'z') && c != '_' && strchr(t, c) == 0) return 0;
	for (; (c = (Auc) s[l]) != '\0'; l++) {
		if (!('A' <= c && c <= 'Z') && !('a' <= c && c <= 'z') && !('0' <= c && c <= '9') && c != '_' && strchr(t, c) == 0) break;
	}
	return l;
}

As inline int AnamCmp(ACStr s, ACStr t, Ai l) { return strncmp(s, t, l) == 0 && s[l] == '\0'; }

