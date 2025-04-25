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
#define As	static
#define AClass(nam)    typedef struct nam ## _ nam; struct nam ## _
#define ASz(typ) ( (Ai) sizeof (typ) )

As Ap aDsvNop(Ap w, Ap v) { (void) v; return w; }
As Ap aInvNop(Ap w, Ap v) { (void) v; return w; }

AClass(AMlc) { Auc dmy; };
AClass(AMlc_Vt) { Ap (*dsv)(Ap,Ap); Ai objSz; Ap (*inv)(Ap,Ap); Ap (*alc)(AMlc*,Ai); Av (*fre)(AMlc*,Ap,Ai); Ap (*rlc)(AMlc*,Ap,Ai,Ai); };
As Ap AMlc_alc(AMlc *w, AMlc_Vt *v, Ai s) { return v->alc(w, s); }
As Av AMlc_fre(AMlc *w, AMlc_Vt *v, Ap p, Ai s) { v->fre(w, p, s); }
As Ap AMlc_rlc(AMlc *w, AMlc_Vt *v, Ap p, Ai s0, Ai s1) { return v->rlc(w, p, s0, s1); }
As Ap aDsv(Ap w, AMlc_Vt *v) { v->dsv(w, v); return w; }
As Av AMlc_del(AMlc *w, AMlc_Vt *v, Ap p, AMlc_Vt *pv) { p = aDsv(p, pv); if (p != 0) { AMlc_fre(w, v, p, pv->objSz); } }
As Ap AMlc_new(AMlc *w, AMlc_Vt *v, AMlc_Vt *pv) { Ap p = AMlc_alc(w, v, pv->objSz); return pv->inv(p, pv); }
As AMlc *AMlc_m0;
As AMlc_Vt *AMlc_m0v;

AClass(AMlcStd) { AMlc w; };
As Ap AMlcStd_alc(AMlc *w, Ai s) { (void) w; return malloc(s); }
As Av AMlcStd_fre(AMlc *w, Ap p, Ai s) { (void) w; (void) s; free(p); }
As Ap AMlcStd_rlc(AMlc *w, Ap p, Ai s0, Ai s1) { (void) w; (void) s0; return realloc(p, s1); }
#define AMlcStd_inv aInvNop
As AMlc_Vt AMlcStd_vt[1] = {{ aDsvNop, ASz(AMlcStd), AMlcStd_inv, AMlcStd_alc, AMlcStd_fre, AMlcStd_rlc }};

AClass(AMlcStdNumSz) { AMlc w; Ai n, s; };
As Ap AMlcStdNumSz_alc(AMlcStdNumSz *w, Ai s) { w->n++; w->s += s; return malloc(s); }
As Av AMlcStdNumSz_fre(AMlcStdNumSz *w, Ap p, Ai s) { w->n--; w->s -= s; free(p); }
As Ap AMlcStdNumSz_rlc(AMlcStdNumSz *w, Ap p, Ai s0, Ai s1) { w->s += s1 - s0; return realloc(p, s1); }
As AMlc *AMlcStdNumSz_inv(AMlcStdNumSz *w, Ap v) { (void) v; w->n = w->s = 0; return (Ap) w; }
As AMlc_Vt AMlcStdNumSz_vt[1] = {{ aDsvNop, ASz(AMlcStdNumSz), (Ap) AMlcStdNumSz_inv, (Ap) AMlcStdNumSz_alc, (Ap) AMlcStdNumSz_fre, (Ap) AMlcStdNumSz_rlc }};

AClass(ATokenMgr) { AMlc *m, *tm; AMlc_Vt *mv, *tmv; AStr *t0, *t1; Ai n, n1; };
As AStr ATokenMgr_i2s(ATokenMgr *w, Ai i) { return w->t0[i]; }

As Ap ATokenMgr_dst(ATokenMgr *w)
{
	Ai n = w->n; AMlc *m = w->m; AMlc_Vt *mv = w->mv; AStr *t0 = w->t0;
	while (n > 0) {
		Auc *u = (Auc *) t0[--n];
		AMlc_fre(m, mv, u - 8, (u[-4] << 24 | u[-3] << 16 | u[-2] << 8 | u[-1]) + 9);
	}
	if (w->n1 > 0) {
		AMlc_fre(m, mv,    t0, w->n1 * ASz(AStr));
		AMlc_fre(m, mv, w->t1, w->n1 * ASz(AStr));
	}
	return w;
}

As Ai ATokenMgr_s2i(ATokenMgr *w, AStr s, Ai l)
{
	char buf[64];
	AStr t = buf, p; Ai i, a, b, c, m;
	Auc *u;
	if (l >= 64)
		t = AMlc_alc(w->tm, w->tmv, l + 1);
	memcpy(t, s, l);
	t[l] = '\0';

	i = 0; if (w->n == 0) goto ins;
	c = strcmp(t, p = w->t1[0]); if (c == 0) goto fin;
	if (c < 0) goto ins;
	i = 1; if (w->n == 1) goto ins;
	c = strcmp(t, p = w->t1[w->n - 1]); if (c == 0) goto fin;
	i = w->n; if (c > 0) goto ins;
	a = 0; b = w->n - 1; m = a + ((b - a) >> 1);
	i = b; if (m == a) goto ins;
	do {
		c = strcmp(t, p = w->t1[m]); if (c == 0) goto fin;
		if (c < 0) { b = m; } else { a = m; }
		m = a + ((b - a) >> 1);
		i = b;
	} while (m > a);
ins:
	if (w->n == w->n1) {
		if (w->n1 == 0) {
			w->n1 = 16;
			w->t0 = AMlc_alc(w->m, w->mv, w->n1 * ASz(AStr));
			w->t1 = AMlc_alc(w->m, w->mv, w->n1 * ASz(AStr));
		} else {
			w->n1 <<= 1;
			w->t0 = AMlc_rlc(w->m, w->mv, w->t0, w->n * ASz(AStr), w->n1 * ASz(AStr));
			w->t1 = AMlc_rlc(w->m, w->mv, w->t1, w->n * ASz(AStr), w->n1 * ASz(AStr));
		}
	}
	u = AMlc_alc(w->m, w->mv, l + 9);
	memcpy(p = (char *) u + 8, t, l);
	p[l] = 0;
	w->t0[a = w->n] = p;
	u[0] = (a >> 24) & 255; u[1] = (a >> 16) & 255; u[2] = (a >> 8) & 255; u[3] = a & 255;
	u[4] = (l >> 24) & 255; u[5] = (l >> 16) & 255; u[6] = (l >> 8) & 255; u[7] = l & 255;
	if (i < w->n) { memmove(&w->t1[i + 1], &w->t1[i], (w->n - i) * ASz(AStr)); }
	w->t1[i] = p;
	w->n++;
fin:
	if (l >= 64) { AMlc_fre(w->tm, w->tmv, t, l + 1); }
	u = (Auc *) p;
	return u[-8] << 24 | u[-7] << 16 | u[-6] << 8 | u[-5];
}

As ATokenMgr *ATokenMgr_ini(ATokenMgr *w)
{
	w->m = w->tm = AMlc_m0; w->mv = w->tmv = AMlc_m0v; w->n = 0; w->n1 = 0; // ini直後にmを変更しやすいように、この時点ではn1=0.
	return w;
}

AClass(AExpMem) { Auc *p; Ai n, n1; AMlc *m; AMlc_Vt *mv; };
#define AExpMem_rsv(w, sz)	if ((w)->n + (sz) >= (w)->n1) AExpMem_setMargin((w), (sz));
As AExpMem *AExpMem_ini(AExpMem *w) { w->p = 0; w->n = w->n1 = 0; w->m = AMlc_m0; w->mv = AMlc_m0v; return w; }
As Ap AExpMem_dst(AExpMem *w) { if (w->p != 0) { AMlc_fre(w->m, w->mv, w->p, w->n1); } return w; }

As Av AExpMem_setMargin(AExpMem *w, Ai sz)
{
	Ai n1 = w->n1, n2 = w->n + sz;
	if (n1 == 0) {
		w->n1 = n1 = 256;
		w->p = AMlc_alc(w->m, w->mv, n1);
	}
	while (n2 > n1)
		n1 <<= 1;
	if (n1 > w->n1) {
		w->p = AMlc_rlc(w->m, w->mv, w->p, w->n1, n1);
		w->n1 = n1;
	}
}

