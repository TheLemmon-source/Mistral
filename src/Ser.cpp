// ============================================================================
//  ser.cpp  -  Sistema "Ser" v2. Todo es condicion + asignacion.
//  Un solo archivo; los controles SON ramas del serializador universal.
//   Win64 : g++ -O2 -std=c++11 ser.cpp -o ser.exe -luser32 -lgdi32
//   aqui  : g++ -O2 -std=c++11 ser.cpp -o ser      (pruebas + ser_check.ppm)
//
//  ser      : rama anidable con A[] (ranuras), C[] volatile long (estado en
//             fijo QZ=256), M[]/TM[] matrices dinamicas por demanda ('f6'
//             fila de condicion y asignacion, 's' texto).
//  SM       : fila [cond, ci, cmp, op, dst, arg]; cond 0=siempre 1=lt 2=gt
//             3=eq ; op 0=set 1=add 2=toggle 3=clip 4=mul.
//  escritor : unico (par obrero en Win; sicrono en pruebas). La GUI encola.
//  raster   : cache por version (solo repinta si cambia el gen), colado en
//             orden de A[] (el mismo orden sirve para pick, multiply z).
//  guardia  : asignaciones de x/y se recortan al fol y esquivan hermanos.
//  oroga.dbg: N = nodo, V = fila SM; carga si existe y guarda el arbol vivo.
// ============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define QZ    256
#define CI_X   0
#define CI_Y   1
#define CI_W   2
#define CI_H   3
#define CI_R   4
#define CI_G   5
#define CI_B   6
#define CI_A   7
#define CI_ABI 8
#define CI_FLG 9
#define CI_RED 10
#define CI_VAL 11
#define CI_EXT 12
#define CI_SEL 13
#define CI_MAX 14

// ---- [S] nucleo ser ---------------------------------------------------------
static void* xM(unsigned long nb) {
    void* p = malloc(nb ? nb : 1);
    if (!p) { fprintf(stderr, "ser: sin memoria\n"); exit(1); }
    return p;
}
static void xMe(void* p) { free(p); }
static char* xSt(const char* t) {
    long n = 0; while (t[n]) n++;
    char* r = (char*)xM((unsigned long)n + 1);
    for (long i = 0; i <= n; i++) r[i] = t[i];
    return r;
}

struct Ser {
    char    n2[3];
    char    tc;         // 'r' raiz 'p' panel 'b' boton 'k' check 's' slider
                        // 'l' texto 'e' editor 'c' cubo
    char*   lb;
    Ser*    fol;
    Ser**   A;  long nA, cA;      // ranuras de convencion y anidados
    void**  M;  long nM, cM;      // matrices dinamicas por demanda
    char**  TM;                   // 'f6' fila SM, 's' texto
    volatile long* C; long nC, cC;  // volatile C[]: todo el estado
    volatile long vr;
};

static void grC(Ser* s, long need) {      // crecimiento pedido + 2
    if (need < s->cA && need < s->cM && need < s->cC) return;
    long na = (need >= s->cA ? need : s->cA) + 2;
    long nm = (need >= s->cM ? need : s->cM) + 2;
    long nc = (need >= s->cC ? need : s->cC) + 2;
    Ser** oa = s->A;
    s->A = (Ser**)xM((unsigned long)na * sizeof(Ser*));
    for (long i = 0; i < s->nA; i++) s->A[i] = oa[i];
    for (long i = s->nA; i < na; i++) s->A[i] = 0;
    if (oa) xMe(oa);
    s->cA = na;
    void** om = s->M; char** ot = s->TM;
    s->M  = (void**)xM((unsigned long)nm * sizeof(void*));
    s->TM = (char**)xM((unsigned long)nm * sizeof(char*));
    for (long i = 0; i < s->nM; i++) { s->M[i] = om[i]; s->TM[i] = ot[i]; }
    for (long i = s->nM; i < nm; i++) { s->M[i] = 0; s->TM[i] = 0; }
    if (om) xMe(om);
    if (ot) xMe(ot);
    s->cM = nm;
    volatile long* oc = s->C;
    s->C = (volatile long*)xM((unsigned long)nc * sizeof(long));
    for (long i = 0; i < s->nC; i++) s->C[i] = oc[i];
    for (long i = s->nC; i < nc; i++) s->C[i] = 0;
    if (oc) xMe((void*)oc);
    s->cC = nc;
}

static Ser** serPool = 0; static long serPoolN = 0, serPoolU = 0;
static volatile long gVivos = 0, gGen = 1;   // gen: version global (cache)

static Ser* serCr(Ser* fol, char tc, const char* n2, const char* lb) {
    Ser* s;
    if (serPoolU > 0) { s = serPool[--serPoolU]; s->vr++; }
    else { s = (Ser*)xM(sizeof(Ser)); if (serPoolU >= serPoolN) serPool = (Ser**)realloc(serPool, (serPoolN + 8) * sizeof(Ser*)); if (!serPool) exit(1); serPoolN += 8;
           s->A = 0; s->cA = 0; s->nA = 0; s->M = 0; s->TM = 0; s->nM = 0;
           s->cM = 0; s->C = 0; s->cC = 0; s->nC = 0; s->vr = 0; }
    s->n2[0] = n2 ? n2[0] : 0;
    s->n2[1] = (n2 && n2[0]) ? n2[1] : 0;
    s->n2[2] = 0;
    s->tc = tc;
    s->lb = lb ? xSt(lb) : xSt("");
    s->fol = fol;
    grC(s, CI_MAX);
    s->nC = CI_MAX;
    if (fol) {
        for (Ser* p0 = fol; p0; p0 = p0->fol) p0->vr++;   // invalida grupos
        long k = -1;
        for (long i = 0; i < fol->nA; i++) if (!fol->A[i]) { k = i; break; }
        if (k < 0) { k = fol->nA; if (k >= fol->cA) grC(fol, k); fol->nA = k + 1; }
        fol->A[k] = s;
    }
    gVivos++;
    return s;
}
static void serAl(Ser* s) {
    while (s->nA > 0) { Ser* h = s->A[s->nA - 1]; s->nA--; serAl(h); }
    if (s->fol)
        for (long i = 0; i < s->fol->cA; i++)
            if (s->fol->A[i] == s) s->fol->A[i] = 0;
    s->fol = 0;
    if (s->lb) { xMe(s->lb); s->lb = 0; }
    for (long i = 0; i < s->nM; i++) if (s->M[i]) { xMe(s->M[i]); s->M[i] = 0; }
    s->nM = 0;
    if (serPoolU < serPoolN) serPool[serPoolU++] = s;
    gVivos--;
}

static long ccG(Ser* s, long i) {
    if (i < 0) return 0;
    if (i >= s->nC) { if (i >= s->cC) grC(s, i); s->nC = i + 1; }
    return s->C[i];
}
static void ccS(Ser* s, long i, long v) {
    ccG(s, i);
    s->C[i] = v;
    s->vr++;
    gGen++;
}

// ---- [V] SM: filas de condicion y asignacion --------------------------------
static double* f6Add(Ser* s, double cond, double ci, double cmp,
                     double op, double dst, double arg) {
    long k = -1;
    for (long i = 0; i < s->nM; i++) if (!s->M[i] || !s->TM[i]) { k = i; break; }
    if (k < 0) { k = s->nM; if (k >= s->cM) grC(s, k); s->nM = k + 1; }
    if (s->M[k]) xMe(s->M[k]);
    s->M[k] = xM(6 * sizeof(double));
    s->TM[k] = (char*)"f6";
    double* f = (double*)s->M[k];
    f[0] = cond; f[1] = ci; f[2] = cmp; f[3] = op; f[4] = dst; f[5] = arg;
    return f;
}
static void exRows(Ser* s) {              // ejecucion de filas SM del nodo
    for (long i = 0; i < s->nM; i++) {
        if (!s->M[i] || !s->TM[i] || s->TM[i][0] != 'f') continue;
        double* f = (double*)s->M[i];
        long cond = (long)f[0], ci = (long)f[1], cmp = (long)f[2];
        long op = (long)f[3], dst = (long)f[4], arg = (long)f[5];
        long v = ccG(s, ci);
        if (cond == 1) { if (!(v < cmp)) continue; }
        else if (cond == 2) { if (!(v > cmp)) continue; }
        else if (cond == 3) { if (v != cmp) continue; }
        long* d = 0;
        if (dst >= 0 && dst < s->nC) d = (long*)&s->C[dst];
        if (!d) continue;
        if (op == 0) *d = arg;
        else if (op == 1) *d += arg;
        else if (op == 2) *d ^= arg;
        else if (op == 3) { if (*d > arg) *d = arg; if (*d < cmp) *d = cmp; }
        else if (op == 4) *d *= arg;
        s->vr++; gGen++;
    }
}

// ---- [Q] cola de ordenes (GUI -> escritor unico) ----------------------------
struct Ord { long k; Ser* t; long r[6]; char ch; char* tx; };
static Ord  oRing[256];
static volatile long oHead = 0, oTail = 0;
static char  gRuta[512] = "oroga.dbg";

static void ordP(long k, Ser* t, long a, long b, long c, long d, long e, long f) {
    long h = oHead;
    oRing[h & 255].k = k;
    oRing[h & 255].t = t;
    oRing[h & 255].r[0] = a; oRing[h & 255].r[1] = b; oRing[h & 255].r[2] = c;
    oRing[h & 255].r[3] = d; oRing[h & 255].r[4] = e; oRing[h & 255].r[5] = f;
    oRing[h & 255].ch = 0;
    oRing[h & 255].tx = 0;
    oHead = h + 1;
}
static void ordCr(char tc, Ser* fol) {         // crear rama nueva (controles)
    long h = oHead;
    oRing[h & 255].k = 1;
    oRing[h & 255].t = fol;
    oRing[h & 255].ch = tc;
    oRing[h & 255].tx = 0;
    oHead = h + 1;
}

static Ser* gRoot = 0;
static Ser* gSes  = 0;                  // contenedor de sesiones (ramas q)
static volatile long gFS = 2;           // escala de texto: celdas por glifo
static long gNvoX = 24, gNvoY = 24;
static void nvoN2(char* out, char t) {           // 2 chars unicos
    const char* ab[9] = { "b1","k1","s1","l1","p1","e1","c1","q1","r1" };
    long i = (t == 'b') ? 0 : (t == 'k') ? 1 : (t == 's') ? 2 : (t == 'l') ? 3
           : (t == 'p') ? 4 : (t == 'e') ? 5 : (t == 'q') ? 7 : 6;
    out[0] = ab[i][0]; out[1] = ab[i][1];
}
static void zapFol(Ser* s) {                     // guardia: recorte al fol
    Ser* f = s->fol;
    if (!f || s->tc == 'r') return;
    long W = ccG(f, CI_W), H = ccG(f, CI_H);
    if (W > 0 && ccG(s, CI_W) > W) ccS(s, CI_W, W - 8);
    if (H > 0 && ccG(s, CI_H) > H) ccS(s, CI_H, H - 8);
    if (ccG(s, CI_X) < 0) ccS(s, CI_X, 0);
    if (ccG(s, CI_Y) < 0) ccS(s, CI_Y, 0);
    if (W > 0 && ccG(s, CI_X) + ccG(s, CI_W) > W)
        ccS(s, CI_X, W - ccG(s, CI_W));
    if (H > 0 && ccG(s, CI_Y) + ccG(s, CI_H) > H)
        ccS(s, CI_Y, H - ccG(s, CI_H));
}
static bool pisaHermano(Ser* s) {
    Ser* f = s->fol;
    if (!f) return false;
    long ax = ccG(s, CI_X), ay = ccG(s, CI_Y), aw = ccG(s, CI_W), ah = ccG(s, CI_H);
    if (aw <= 0 || ah <= 0) return false;
    for (long i = 0; i < f->nA; i++) {
        Ser* h = f->A[i];
        if (!h || h == s || ccG(h, CI_FLG) & 1) continue;
        long bx = ccG(h, CI_X), by = ccG(h, CI_Y);
        long bw = ccG(h, CI_W), bh = ccG(h, CI_H);
        long ox = (ax < bx ? ax : bx) + (ax + aw < bx + bw ? ax + aw : bx + bw)
                - (ax < bx ? bx : ax);
        long oy = (ay < by ? ay : by) + (ay + ah < by + bh ? ay + ah : by + bh)
                - (ay < by ? by : ay);
        if (ox > 0 && oy > 0) {
            long ac = ox * oy, hc = (aw < bw ? aw : bw) * (ah < bh ? ah : bh);
            if (ac * 10 > hc * 6) return true;      // pisa > 60%
        }
    }
    return false;
}
// ---- [L] tipografia 3x5 ------------------------------------------------------
static const char* fGly(char c) {
    switch (c >= 'a' && c <= 'z' ? (char)(c - 32) : c) {
    case 'A': return "25755"; case 'B': return "65656"; case 'C': return "34443";
    case 'D': return "65556"; case 'E': return "74647"; case 'F': return "74644";
    case 'G': return "34553"; case 'H': return "55755"; case 'I': return "72227";
    case 'J': return "11157"; case 'K': return "55655"; case 'L': return "44447";
    case 'M': return "57555"; case 'N': return "65555"; case 'O': return "25552";
    case 'P': return "65644"; case 'Q': return "25563"; case 'R': return "65655";
    case 'S': return "34216"; case 'T': return "72222"; case 'U': return "55557";
    case 'V': return "55552"; case 'W': return "55575"; case 'X': return "55255";
    case 'Y': return "55222"; case 'Z': return "71247";
    case '0': return "75557"; case '1': return "26227"; case '2': return "71747";
    case '3': return "71717"; case '4': return "55711"; case '5': return "74717";
    case '6': return "74757"; case '7': return "71122"; case '8': return "75757";
    case '9': return "75717";
    case '!': return "22202"; case '+': return "02720"; case '-': return "00700";
    case '.': return "00002"; case ',': return "00024"; case ':': return "02020";
    case '/': return "11244"; case '(': return "12221"; case ')': return "42224";
    case '=': return "07070"; case '<': return "12421"; case '>': return "42124";
    case '_': return "00007"; case '#': return "57575"; case '*': return "25000";
    }
    return "00000";
}

// ---- [R] raster: cache por zonas segun version ------------------------------
static long gW = 800, gH = 520;
static unsigned int* gImg = 0;         // ARGB empaquetado
static long gImgGen = -1;

static unsigned int pr(float r, float g, float b, float a) {
    if (r < 0) r = 0;
    if (r > 1) r = 1;
    if (g < 0) g = 0;
    if (g > 1) g = 1;
    if (b < 0) b = 0;
    if (b > 1) b = 1;
    if (a < 0) a = 0;
    if (a > 1) a = 1;
    unsigned int q = (unsigned int)(a * 255.0f + 0.5f);
    unsigned int rr = (unsigned int)(r * 255.0f + 0.5f);
    unsigned int gg = (unsigned int)(g * 255.0f + 0.5f);
    unsigned int bb = (unsigned int)(b * 255.0f + 0.5f);
    return (q << 24) | (rr << 16) | (gg << 8) | bb;
}
static unsigned int* gTgt = 0;   // destino actual (pantalla o cache de grupo)
static long gTW = 0, gTH = 0;
static long gPxC = 0;            // pixel encendido
static long gRq = 0;             // primitivas rasterizadas (medicion)

static void px(int x, int y, unsigned int c) {
    if (!gTgt || x < 0 || y < 0 || x >= gTW || y >= gTH) return;
    gPxC++;
    float a = (float)((c >> 24) & 255) / 255.0f;
    unsigned int* d = &gTgt[y * gTW + x];
    float dr = (float)(((*d >> 16) & 255));
    float dg = (float)(((*d >> 8) & 255));
    float db = (float)((*d & 255));
    float sr = (float)((c >> 16) & 255);
    float sg = (float)((c >> 8) & 255);
    float sb = (float)(c & 255);
    unsigned int nr = (unsigned int)(dr + (sr - dr) * a + 0.5f);
    unsigned int ng = (unsigned int)(dg + (sg - dg) * a + 0.5f);
    unsigned int nb = (unsigned int)(db + (sb - db) * a + 0.5f);
    *d = (c & 0xff000000u) | (nr << 16) | (ng << 8) | nb;
}
static void rectoQ(float x, float y, float w, float h, unsigned int c, float rad) {
    if (w <= 0 || h <= 0) return;
    gRq++;
    if (rad < 1) {
        for (int j = (int)y; j < (int)(y + h); j++)
            for (int i = (int)x; i < (int)(x + w); i++) px(i, j, c);
        return;
    }
    float cx = x + w * 0.5f, cy = y + h * 0.5f;
    float hw = w * 0.5f, hh = h * 0.5f;
    if (rad > hw) rad = hw;
    if (rad > hh) rad = hh;
    float bx = hw - rad, by = hh - rad;
    for (int j = (int)y; j < (int)(y + h); j++)
        for (int i = (int)x; i < (int)(x + w); i++) {
            float qx = (float)(i + 1) - cx;
            float qy = (float)(j + 1) - cy;
            float ax = (qx < 0 ? -qx : qx) - bx;
            float ay = (qy < 0 ? -qy : qy) - by;
            float ux = ax > 0 ? ax : 0;
            float uy = ay > 0 ? ay : 0;
            float mo = ax > ay ? ax : ay;
            float d = (float)sqrt(ux * ux + uy * uy) + (mo < 0 ? mo : 0) - rad;
            if (d <= 0) px(i, j, c);
            else if (d < 1) {
                unsigned int cc = pr(
                    (float)((c >> 16) & 255) / 255.0f,
                    (float)((c >> 8) & 255) / 255.0f,
                    (float)(c & 255) / 255.0f, 1.0f - d);
                px(i, j, cc);
            }
        }
}
static void texto(float x, float y, const char* t, unsigned int c) {
    long sc = gFS < 1 ? 1 : gFS;
    for (long k = 0; t[k]; k++) {
        const char* g = fGly(t[k]);
        for (long j = 0; j < 5; j++) {
            int fil = g[j] - '0';
            for (long i = 0; i < 3; i++) {
                if (((fil >> (2 - i)) & 1) == 0) continue;
                for (long b = 0; b < sc; b++)
                    for (long a = 0; a < sc; a++)
                        px((int)(x + k * 5 * sc + i * sc + a + 1),
                           (int)(y + j * sc + b + 1), c);
            }
        }
    }
}

// ---- [P] pintado del arbol (colado por orden A[], cache por gen) ------------
static Ser* gLin[1024];    // lista colada pintador (mismo orden sirve al pick)
static long gLinN = 0;

static void pintaNodo(Ser* s, float ox, float oy) {
    if (!s) return;
    long fl = ccG(s, CI_FLG);
    if (s->tc != 'r' && !(fl & 1)) return;
    float x = ox + (float)ccG(s, CI_X) / QZ;
    float y = oy + (float)ccG(s, CI_Y) / QZ;
    float w = (float)ccG(s, CI_W) / QZ;
    float h = (float)ccG(s, CI_H) / QZ;
    float r = (float)ccG(s, CI_R) / (float)QZ;
    float g = (float)ccG(s, CI_G) / (float)QZ;
    float b = (float)ccG(s, CI_B) / (float)QZ;
    float a = (float)ccG(s, CI_A) / (float)QZ;
    float red = (float)ccG(s, CI_RED);
    unsigned int col = pr(r, g, b, a);
    if (s->tc == 'b' || s->tc == 'e') {
        if (ccG(s, CI_EXT) > 0) col = pr(r * 1.4f, g * 1.4f, b * 1.4f, a);
        rectoQ(x, y, w, h, col, red);
        texto(x + 4, y + (h - 5 * gFS) * 0.5f, s->lb, pr(1, 1, 1, 1));
    } else if (s->tc == 'k') {
        rectoQ(x, y, w, h, col, red);
        if (fl & 2) rectoQ(x + 6, y + 6, w - 12 < 0 ? 0 : w - 12,
                           h - 12 < 0 ? 0 : h - 12, pr(0.1f, 0.9f, 0.2f, 1), 3);
    } else if (s->tc == 's') {
        rectoQ(x, y, w, h, pr(0.25f, 0.26f, 0.3f, a), red);
        float fr = (float)ccG(s, CI_VAL) / 1000.0f;
        if (fr < 0) fr = 0;
                if (fr > 1) fr = 1;
        rectoQ(x + 2, y + 2, (w - 4) * fr, h - 4, col, red);
        texto(x + 4, y + (h - 5 * gFS) * 0.5f, s->lb, pr(1, 1, 1, 1));
    } else if (s->tc == 'l') {
        texto(x, y, s->lb, col);
    } else if (s->tc == 'c') {
        float t = (float)ccG(s, CI_VAL) / (float)QZ;
        float rx = x + w * 0.5f, ry = y + h * 0.5f;
        float rr = w < h ? w : h; rr *= 0.4f;
        rectoQ(rx - rr, ry - rr, rr * 2, rr * 2, col, rr * (0.5f + 0.4f * t));
    } else {   // 'p' y 'r' y 'o': panel (recuadre redondeado de demo)
        rectoQ(x, y, w, h, col, red);
        rectoQ(x + 1, y + 1, w - 2 < 0 ? 0 : w - 2, 14 < h - 2 ? 14 : h - 2,
               pr(r * 1.35f, g * 1.35f, b * 1.35f, a > 0.5f ? 1 : a), red);
        texto(x + 4, y + 3, s->lb, pr(1, 1, 1, 1));
    }
    if (s->tc != 'r' && ccG(s, CI_SEL)) {
        rectoQ(x - 1, y - 1, w + 2, 1, pr(1, 1, 1, 1), 0);
        rectoQ(x - 1, y + h, w + 2, 1, pr(1, 1, 1, 1), 0);
        rectoQ(x - 1, y - 1, 1, h + 2, pr(1, 1, 1, 1), 0);
        rectoQ(x + w, y - 1, 1, h + 2, pr(1, 1, 1, 1), 0);
    }
    if (s->tc != 'p' || (ccG(s, CI_ABI) & 1)) {   // cascada: despliegue
        for (long i = 0; i < s->nA; i++) pintaNodo(s->A[i], x, y);
    }
}

static void listaRec(Ser* s) {           // lista colada: pintador y pick
    if (!s) return;
    if (s->tc == 'r') {
        for (long i = 0; i < s->nA; i++) listaRec(s->A[i]);
        return;
    }
    if (!(ccG(s, CI_FLG) & 1)) return;
    if (gLinN < 1024) gLin[gLinN++] = s;
    if (s->tc != 'p' || (ccG(s, CI_ABI) & 1))
        for (long i = 0; i < s->nA; i++) listaRec(s->A[i]);
}
static long gvUp(Ser* s) {               // version del grupo = suma de rama
    long v = (long)s->vr;
    for (long i = 0; i < s->nA; i++) {
        Ser* h = s->A[i];
        if (h) v += gvUp(h);             // suma: cualquier cambio cuenta
    }
    return v;
}
static long extW(Ser* s) {               // bloque alcanzado por la rama
    long w = ccG(s, CI_W) / QZ;
    long lt = 0; while (s->lb && s->lb[lt]) lt++;
    long tw = 14 + lt * 5;
    if (tw > w) w = tw;
    return w + 6;
}
static long extH(Ser* s) {
    long h = ccG(s, CI_H) / QZ;
    if (h < 16) h = 16;
    return h + 6;
}
static long long* cacheRow(Ser* s, long w, long h) {   // M fila 'g': [img,w,h,vv]
    long k = -1;
    for (long i = 0; i < s->nM; i++)
        if (s->TM[i] && s->TM[i][0] == 'g') { k = i; break; }
    if (k < 0) { k = s->nM; if (k >= s->cM) grC(s, k); s->nM = k + 1;
                 s->TM[k] = (char*)"g"; s->M[k] = 0; }
    long long* c4 = (long long*)s->M[k];
    if (!c4 || c4[1] < w || c4[2] < h) {
        long nw = (c4 && c4[1] > w) ? (long)c4[1] : w;
        long nh = (c4 && c4[2] > h) ? (long)c4[2] : h;
        if (c4) xMe((void*)c4[0]);
        unsigned int* im = (unsigned int*)xM((unsigned long)(nw * nh) * sizeof(unsigned int));
        memset(im, 0, (size_t)(nw * nh) * sizeof(unsigned int));
        c4 = (long long*)xM(4 * sizeof(long long));
        c4[0] = (long long)im; c4[1] = nw; c4[2] = nh; c4[3] = -1;
        s->M[k] = c4;
    }
    return c4;
}
static void limpia(int x, int y, int w, int h) {  // fondo en zona sucia
    unsigned int bg = pr(0.07f, 0.08f, 0.1f, 1);
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) px(i, j, bg);
}
static void blit(long long* c4, int dx, int dy) { // cache vigente: volcado
    unsigned int* im = (unsigned int*)c4[0];
    long w = (long)c4[1], h = (long)c4[2];
    for (long j = 0; j < h; j++)
        for (long i = 0; i < w; i++) {
            unsigned int c = im[j * w + i];
            if ((c >> 24) & 255) px(dx + (int)i, dy + (int)j, c);
        }
}
static void pintaTodo() {
    if (!gImg) return;
    if (gImgGen == gGen) return;                  // 0) nada cambio en TODO
    gTgt = gImg; gTW = gW; gTH = gH;
    gLinN = 0;
    listaRec(gRoot);                              // pick comparte el colado
    if (gRoot) {
        for (long i = 0; i < gRoot->nA; i++) {
            Ser* c = gRoot->A[i];
            if (!c) continue;
            long w = extW(c), h = extH(c);
            long long* c4 = cacheRow(c, w, h);
            long vis = ccG(c, CI_FLG) & 1;
            int ax = (int)(ccG(c, CI_X) - 3 * QZ) / QZ;
            int ay = (int)(ccG(c, CI_Y) - 3 * QZ) / QZ;
            long gv = gvUp(c);
            limpia(ax, ay, w, h);                 // zona -> fondo
            if (!vis) { c4[3] = -1; continue; }
            if (gv == c4[3]) { blit(c4, ax, ay); continue; }
            unsigned int* sub = (unsigned int*)c4[0];
            memset(sub, 0, (size_t)((long)c4[1] * (long)c4[2]) * sizeof(unsigned int));
            gTgt = sub; gTW = (long)c4[1]; gTH = (long)c4[2];
            pintaNodo(c, 3 - (float)(ccG(c, CI_X)) / QZ,   // siempre en (3,3)
                         3 - (float)(ccG(c, CI_Y)) / QZ);
            c4[3] = gv;
            gTgt = gImg; gTW = gW; gTH = gH;
            blit(c4, ax, ay);
        }
    }
    gImgGen = gGen;
}

// ---- [K] pick con el mismo orden de colado ----------------------------------
static Ser* elige(float mx, float my) {
    for (long i = gLinN - 1; i >= 0; i--) {
        Ser* s = gLin[i];
        Ser* f = s->fol;
        float ox = 0, oy = 0;
        for (Ser* p = f; p; p = p->fol) {
            ox += (float)ccG(p, CI_X) / QZ;
            oy += (float)ccG(p, CI_Y) / QZ;
        }
        float x = ox + (float)ccG(s, CI_X) / QZ;
        float y = oy + (float)ccG(s, CI_Y) / QZ;
        float w = (float)ccG(s, CI_W) / QZ;
        float h = (float)ccG(s, CI_H) / QZ;
        if (mx >= x && my >= y && mx <= x + w && my <= y + h) return s;
    }
    return 0;
}
static void mandaAlFrente(Ser* s) {
    Ser* f = s->fol;
    if (!f) return;
    long k = -1;
    for (long i = 0; i < f->nA; i++) if (f->A[i] == s) { k = i; break; }
    if (k >= 0 && k < f->nA - 1) {
        for (long i = k; i < f->nA - 1; i++) f->A[i] = f->A[i + 1];
        f->A[f->nA - 1] = s;
        gGen++;
    }
}

// ---- [E] escritor unico: ejecuta la cola ------------------------------------
static void escritor() {
    while (oTail != oHead) {
        Ord* o = &oRing[oTail & 255];
        if (o->k == 0) {                         // asignacion condicional
            Ser* t = o->t;
            if (t) {
                long cond = o->r[2];
                long op = o->r[3];
                if (op == 3) {                   // mover x/y con guardia de golpes
                    ccS(t, CI_X, o->r[1]);
                    ccS(t, CI_Y, o->r[2]);
                    zapFol(t);
                    for (int n = 0; n < 4 && pisaHermano(t); n++) {
                        ccS(t, CI_Y, ccG(t, CI_Y) + ccG(t, CI_H) + QZ);
                        zapFol(t);
                    }
                } else {
                    long ci = o->r[0], v = o->r[1];
                    long act = ccG(t, ci);
                    if (cond == 1) { if (act < v) ccS(t, ci, op); }
                    else if (cond == 2) { if (act > v) ccS(t, ci, op); }
                    else if (cond == 3) { if (act == v) ccS(t, ci, op); }
                    else ccS(t, ci, op);
                    if (o->r[5] == 7) exRows(t);   // reproducir filas del nodo
                }
            }
        } else if (o->k == 1) {                  // crear rama nueva
            char n2[3]; nvoN2(n2, o->ch);
            Ser* fol = o->t ? o->t : gRoot;
            const char* nomb = (o->ch == 'b') ? "Boton" : (o->ch == 'k') ? "Chek"
                             : (o->ch == 's') ? "Slid" : (o->ch == 'l') ? "Txt"
                             : (o->ch == 'p') ? "Pan" : (o->ch == 'c') ? "cbo"
                             : (o->ch == 'q') ? "Ses" : "Nvo";
            Ser* s = serCr(fol, o->ch, n2, nomb);
            long mx = fol == gRoot ? gW : ccG(fol, CI_W) / QZ;
            long mh = fol == gRoot ? gH : ccG(fol, CI_H) / QZ;
            ccS(s, CI_W, 90 * QZ); ccS(s, CI_H, 22 * QZ);
            ccS(s, CI_X, (gNvoX % (mx > 120 ? mx - 120 : 24)) * QZ);
            ccS(s, CI_Y, ((gNvoY + (gNvoX / (mx > 120 ? mx - 120 : 24)) * 30)
                          % (mh > 40 ? mh - 40 : 16)) * QZ);
            ccS(s, CI_FLG, 1 | 4);
            ccS(s, CI_R, 0.25f * QZ); ccS(s, CI_G, 0.45f * QZ);
            ccS(s, CI_B, 0.85f * QZ); ccS(s, CI_A, QZ);
            ccS(s, CI_RED, 6);
            gNvoX += 30; gNvoY += 14;
            f6Add(s, 2, (double)CI_EXT, 0.0, 2.0, (double)CI_FLG, 2.0);
        } else if (o->k == 2) {                  // eliminar
            if (o->t && o->t->tc != 'r') {
                Ser* f = o->t->fol;
                serAl(o->t);
                if (f) gGen++;
            }
        } else if (o->k == 3) {                  // mando al frente (z)
            if (o->t) mandaAlFrente(o->t);
        } else if (o->k == 4) {                  // guardar oroga.dbg
            FILE* f = fopen(gRuta, "wb");
            if (f) {
                long idx = 0;
                Ser* lin[1024]; long folto[1024];
                lin[idx] = gRoot; folto[idx] = -1; idx++;
                for (long p = 0; p < idx && p < 1024; p++) {
                    Ser* s = lin[p];
                    fprintf(f, "N %ld %ld %c %s %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld\n",
                        p, folto[p], s->tc, s->lb,
                        ccG(s, CI_X), ccG(s, CI_Y), ccG(s, CI_W), ccG(s, CI_H),
                        ccG(s, CI_R), ccG(s, CI_G), ccG(s, CI_B), ccG(s, CI_A),
                        ccG(s, CI_ABI), ccG(s, CI_FLG));
                    for (long m = 0; m < s->nM; m++) {
                        if (!s->M[m] || !s->TM[m] || s->TM[m][0] != 'f') continue;
                        double* r6 = (double*)s->M[m];
                        fprintf(f, "V %ld %ld %d %d %d %d %d %d\n", p,
                            (long)r6[0], (int)r6[1], (int)r6[2], (int)r6[3],
                            (int)r6[4], (int)r6[5], 0);
                    }
                    for (long i = 0; i < s->nA; i++)
                        if (s->A[i] && idx < 1024) {
                            lin[idx] = s->A[i]; folto[idx] = p; idx++;
                        }
                }
                fclose(f);
            }
        } else if (o->k == 5) {                  // accion del editor
            if (o->tx) { if (o->tx[0] == 'G') ordP(4, 0, 0,0,0,0,0,0); }
        }
        for (long z = 0; z < 256; z++) oRing[z].tx = 0;
        oTail = oTail + 1;
    }
}
// ---- [I] interaccion (mis para todos) ---------------------------------------
static Ser* gSel = 0; static Ser* gAnt = 0;   // seleccion actual y previa
static Ser* gArra = 0;               // nodo arrastrado
static float gDOffX, gDOffY;          // anclaje del arrastre
static int   gSlid = 0;               // arrastrando un slider

static __attribute__((unused)) void pisaRaton(float mx, float my) {
    Ser* s = elige(mx, my);
    if (!s) return;
    gAnt = gSel;
    gSel = s;
    mandaAlFrente(s);
    ordP(3, s, 0, 0, 0, 0, 0, 0);
    Ser* f = s->fol;
    float ox = 0, oy = 0;
    for (Ser* p = f; p; p = p->fol) {
        ox += (float)ccG(p, CI_X) / QZ;
        oy += (float)ccG(p, CI_Y) / QZ;
    }
    if (s->tc == 's') {
        gSlid = 1;
        Ser* ff = s->fol;
        float sx = 0;
        for (Ser* q = ff; q; q = q->fol) sx += (float)ccG(q, CI_X) / QZ;
        float x0 = sx + (float)ccG(s, CI_X) / QZ;
        float wi = (float)ccG(s, CI_W) / QZ;
        float fr2 = (mx - x0) / (wi > 0 ? wi : 1);
        if (fr2 < 0) fr2 = 0;
        if (fr2 > 1) fr2 = 1;
        ccS(s, CI_VAL, (long)(fr2 * 1000));
    } else {
        gArra = s;
        gDOffX = mx - ox - (float)ccG(s, CI_X) / QZ;
        gDOffY = my - oy - (float)ccG(s, CI_Y) / QZ;
    }
    if (s->tc == 'b') ccS(s, CI_EXT, 1);
    if (s->tc == 'k') ccS(s, CI_FLG, ccG(s, CI_FLG) ^ 2);
    if (s->tc == 'p') ccS(s, CI_ABI, ccG(s, CI_ABI) ^ 1);
    if (s->tc == 'e') {
        long v = ccG(s, CI_VAL);
        Ser* obj = (gAnt && gAnt->tc != 'e' && gAnt->tc != 'r') ? gAnt : 0;
        if (v == 0) ordCr('b', gRoot);
        else if (v == 1) ordCr('k', gRoot);
        else if (v == 2) ordCr('s', gRoot);
        else if (v == 3) { if (obj) ordP(2, obj, 0, 0, 0, 0, 0, 0); }
        else if (v == 4) ordP(4, 0, 0, 0, 0, 0, 0, 0);
        else if (v == 5) ordCr('q', gSes);
    }
    exRows(s);
}
static __attribute__((unused)) void sueltoRaton() { gArra = 0; gSlid = 0; }
static __attribute__((unused)) void tecla(char k) {
    if (k == 0) return;
    if (gSel) exRows(gSel);
}

// ---- [F] carga / escena modelos ----------------------------------------------
static long leeNum(const char* l, long idx, float* out) {
    long k = 0, w = 0;
    float acc = 0; int neg = 0;
    while (l[k] == ' ') k++;
    while (w < idx) {
        while (l[k] == ' ') k++;
        while (l[k] && l[k] != ' ') k++;
        w++;
    }
    while (l[k] == ' ') k++;
    if (l[k] == '-') { neg = 1; k++; }
    while (l[k] >= '0' && l[k] <= '9') { acc = acc * 10 + (l[k] - '0'); k++; }
    if (l[k] == '.') {
        k++; float m = 0.1f;
        while (l[k] >= '0' && l[k] <= '9') { acc += (l[k] - '0') * m; m *= 0.1f; k++; }
    }
    *out = neg ? -acc : acc;
    return 1;
}
static long campoTxt(const char* l, long idx, char* out, long mx) {
    long k = 0, w = 0;
    while (w < idx) {
        while (l[k] == ' ') k++;
        while (l[k] && l[k] != ' ') k++;
        w++;
    }
    while (l[k] == ' ') k++;
    long n = 0;
    while (l[k] && l[k] != '\n') { if (n < mx - 1) out[n++] = l[k]; k++; }
    out[n] = 0;
    return n;
}
static void desEscenaRoot();
static __attribute__((unused)) long desEscena(const char* ruta) {
    FILE* f = fopen(ruta, "rb");
    if (!f) return 0;
    if (!gRoot) desEscenaRoot();
    char lin[512];
    Ser** prev = (Ser**)xM(256 * sizeof(Ser*));
    for (long i = 0; i < 256; i++) prev[i] = 0;
    long np = 0;
    while (fgets(lin, sizeof(lin), f)) {
        if (lin[0] == 'N') {
            float fol = 0, tc = 0, x = 0, y = 0, w = 0, h = 0;
            float r = 0, g = 0, b = 0, a = 0, ab = 0, fl = 0;
            char nomb[256];
            leeNum(lin, 2, &fol);
            leeNum(lin, 3, &tc);
            campoTxt(lin, 4, nomb, sizeof(nomb));
            leeNum(lin, 5, &x);  leeNum(lin, 6, &y);
            leeNum(lin, 7, &w);  leeNum(lin, 8, &h);
            leeNum(lin, 9, &r);  leeNum(lin, 10, &g);
            leeNum(lin, 11, &b); leeNum(lin, 12, &a);
            leeNum(lin, 13, &ab); leeNum(lin, 14, &fl);
            char tco = (char)(long)tc;
            Ser* folp = (fol >= 0 && fol < np) ? prev[(long)fol] : gRoot;  // indice directo
            Ser* s = serCr(folp, tco, "nd", nomb);
            ccS(s, CI_X, (long)(x * QZ)); ccS(s, CI_Y, (long)(y * QZ));
            ccS(s, CI_W, (long)(w * QZ)); ccS(s, CI_H, (long)(h * QZ));
            ccS(s, CI_R, (long)(r * QZ)); ccS(s, CI_G, (long)(g * QZ));
            ccS(s, CI_B, (long)(b * QZ)); ccS(s, CI_A, (long)(a * QZ));
            ccS(s, CI_ABI, (long)ab); ccS(s, CI_FLG, (long)fl);
            ccS(s, CI_RED, 6);
            ccS(s, CI_VAL, (long)(h * 2));
            if (np < 256) prev[np++] = s;
        } else if (lin[0] == 'V') {
            float idx = 0, c0 = 0, c1 = 0, c2 = 0, c3 = 0, c4 = 0, c5 = 0;
            leeNum(lin, 1, &idx); leeNum(lin, 2, &c0); leeNum(lin, 3, &c1);
            leeNum(lin, 4, &c2);  leeNum(lin, 5, &c3); leeNum(lin, 6, &c4);
            leeNum(lin, 7, &c5);
            if (idx >= 0 && idx < np) 
                f6Add(prev[(long)idx], c0, c1, c2, c3, c4, c5);
        }
    }
    xMe(prev);
    fclose(f);
    gGen++;
    return np;
}
static void desEscenaRoot() {
    gRoot = serCr(0, 'r', "r0", "S");
    ccS(gRoot, CI_W, gW * QZ);
    ccS(gRoot, CI_H, gH * QZ);
    ccS(gRoot, CI_FLG, 0);
}
static void escModelo() {                   // escena de arranque
    desEscenaRoot();
    Ser* p = serCr(gRoot, 'p', "pA", "Dmo");
    ccS(p, CI_X, 16 * QZ); ccS(p, CI_Y, 16 * QZ);
    ccS(p, CI_W, 260 * QZ); ccS(p, CI_H, 210 * QZ);
    ccS(p, CI_R, 0.14f * QZ); ccS(p, CI_G, 0.16f * QZ); ccS(p, CI_B, 0.2f * QZ);
    ccS(p, CI_A, QZ); ccS(p, CI_FLG, 1); ccS(p, CI_ABI, 1); ccS(p, CI_RED, 10);
    Ser* b = serCr(p, 'b', "bH", "Hola");
    ccS(b, CI_X, 10 * QZ); ccS(b, CI_Y, 22 * QZ);
    ccS(b, CI_W, 90 * QZ); ccS(b, CI_H, 24 * QZ);
    ccS(b, CI_R, 0.2f * QZ); ccS(b, CI_G, 0.5f * QZ); ccS(b, CI_B, 0.9f * QZ);
    ccS(b, CI_A, QZ); ccS(b, CI_FLG, 1 | 4); ccS(b, CI_RED, 6);
    f6Add(b, 2, (double)CI_EXT, 0.0, 2.0, (double)CI_FLG, 2.0);
    Ser* k = serCr(p, 'k', "kA", "Mundo");
    ccS(k, CI_X, 10 * QZ); ccS(k, CI_Y, 54 * QZ);
    ccS(k, CI_W, 90 * QZ); ccS(k, CI_H, 20 * QZ);
    ccS(k, CI_R, 0.2f * QZ); ccS(k, CI_G, 0.3f * QZ); ccS(k, CI_B, 0.4f * QZ);
    ccS(k, CI_A, QZ); ccS(k, CI_FLG, 1 | 2); ccS(k, CI_RED, 4);
    Ser* s = serCr(p, 's', "sA", "Vvor");
    ccS(s, CI_X, 10 * QZ); ccS(s, CI_Y, 80 * QZ);
    ccS(s, CI_W, 200 * QZ); ccS(s, CI_H, 18 * QZ);
    ccS(s, CI_R, 0.9f * QZ); ccS(s, CI_G, 0.6f * QZ); ccS(s, CI_B, 0.1f * QZ);
    ccS(s, CI_A, QZ); ccS(s, CI_FLG, 1); ccS(s, CI_VAL, 400); ccS(s, CI_RED, 8);
    Ser* m = serCr(gRoot, 'l', "lT", "SM Todo. es condicion y asignacion");
    ccS(m, CI_X, 300 * QZ); ccS(m, CI_Y, 20 * QZ);
    ccS(m, CI_R, QZ); ccS(m, CI_G, QZ); ccS(m, CI_B, QZ); ccS(m, CI_A, QZ);
    ccS(m, CI_FLG, 1); ccS(m, CI_W, QZ); ccS(m, CI_H, QZ);
    Ser* cb = serCr(gRoot, 'c', "cA", "cbo");
    ccS(cb, CI_X, 300 * QZ); ccS(cb, CI_Y, 60 * QZ);
    ccS(cb, CI_W, 90 * QZ); ccS(cb, CI_H, 90 * QZ);
    ccS(cb, CI_R, 0.6f * QZ); ccS(cb, CI_G, 0.2f * QZ); ccS(cb, CI_B, 0.5f * QZ);
    ccS(cb, CI_A, QZ); ccS(cb, CI_FLG, 1); ccS(cb, CI_VAL, 128); ccS(cb, CI_RED, 0);
    f6Add(cb, 0, (double)CI_VAL, 0.0, 1.0, (double)CI_VAL, (double)QZ);
    Ser* rail = serCr(gRoot, 'o', "rE", "Rail");
    ccS(rail, CI_X, 600 * QZ); ccS(rail, CI_Y, 380 * QZ);
    ccS(rail, CI_W, 180 * QZ); ccS(rail, CI_H, 120 * QZ);
    ccS(rail, CI_A, 0.3f * QZ); ccS(rail, CI_FLG, 1); ccS(rail, CI_ABI, 1);
    ccS(rail, CI_RED, 12);
    const char* nomb[6] = { "Bot", "Chek", "Slid", "Borra", "Guardar", "Sesion" };
    char tcs[6] = { 'e', 'e', 'e', 'e', 'e', 'e' };
    for (long i = 0; i < 6; i++) {
        Ser* e = serCr(rail, tcs[i], "eS", nomb[i]);
        ccS(e, CI_X, (8 + (i % 2) * 86) * QZ);
        ccS(e, CI_Y, (20 + (i / 2) * 30) * QZ);
        ccS(e, CI_W, 80 * QZ); ccS(e, CI_H, 24 * QZ);
        ccS(e, CI_R, 0.15f * QZ); ccS(e, CI_G, 0.55f * QZ); ccS(e, CI_B, 0.4f * QZ);
        ccS(e, CI_A, QZ); ccS(e, CI_FLG, 1 | 4); ccS(e, CI_RED, 8);
        ccS(e, CI_VAL, (long)i);
    }
    gSes = serCr(gRoot, 'p', "pS", "Sesiones");
    ccS(gSes, CI_X, 300 * QZ); ccS(gSes, CI_Y, 170 * QZ);
    ccS(gSes, CI_W, 260 * QZ); ccS(gSes, CI_H, 170 * QZ);
    ccS(gSes, CI_R, 0.12f * QZ); ccS(gSes, CI_G, 0.2f * QZ); ccS(gSes, CI_B, 0.16f * QZ);
    ccS(gSes, CI_A, QZ); ccS(gSes, CI_FLG, 1); ccS(gSes, CI_ABI, 1); ccS(gSes, CI_RED, 8);
    Ser* se1 = serCr(gSes, 'p', "s1", "Ses.1");
    ccS(se1, CI_X, 8 * QZ); ccS(se1, CI_Y, 20 * QZ);
    ccS(se1, CI_W, 118 * QZ); ccS(se1, CI_H, 60 * QZ);
    ccS(se1, CI_R, 0.18f * QZ); ccS(se1, CI_G, 0.3f * QZ); ccS(se1, CI_B, 0.28f * QZ);
    ccS(se1, CI_A, QZ); ccS(se1, CI_FLG, 1); ccS(se1, CI_ABI, 1); ccS(se1, CI_RED, 6);
    Ser* b1 = serCr(se1, 'b', "b1", "ir");
    ccS(b1, CI_X, 8 * QZ); ccS(b1, CI_Y, 24 * QZ);
    ccS(b1, CI_W, 40 * QZ); ccS(b1, CI_H, 20 * QZ);
    ccS(b1, CI_R, 0.2f * QZ); ccS(b1, CI_G, 0.5f * QZ); ccS(b1, CI_B, 0.9f * QZ);
    ccS(b1, CI_A, QZ); ccS(b1, CI_FLG, 1); ccS(b1, CI_RED, 4);
    Ser* se2 = serCr(gSes, 'p', "s2", "Ses.2");
    ccS(se2, CI_X, 132 * QZ); ccS(se2, CI_Y, 20 * QZ);
    ccS(se2, CI_W, 118 * QZ); ccS(se2, CI_H, 60 * QZ);
    ccS(se2, CI_R, 0.18f * QZ); ccS(se2, CI_G, 0.3f * QZ); ccS(se2, CI_B, 0.28f * QZ);
    ccS(se2, CI_A, QZ); ccS(se2, CI_FLG, 1); ccS(se2, CI_ABI, 1); ccS(se2, CI_RED, 6);
    Ser* v2 = serCr(se2, 's', "v2", "v2");
    ccS(v2, CI_X, 8 * QZ); ccS(v2, CI_Y, 24 * QZ);
    ccS(v2, CI_W, 100 * QZ); ccS(v2, CI_H, 18 * QZ);
    ccS(v2, CI_R, 0.9f * QZ); ccS(v2, CI_G, 0.6f * QZ); ccS(v2, CI_B, 0.1f * QZ);
    ccS(v2, CI_A, QZ); ccS(v2, CI_FLG, 1); ccS(v2, CI_VAL, 300); ccS(v2, CI_RED, 6);
    Ser* pG = serCr(gRoot, 'p', "pG", "Generalidades");
    ccS(pG, CI_X, 300 * QZ); ccS(pG, CI_Y, 360 * QZ);
    ccS(pG, CI_W, 280 * QZ); ccS(pG, CI_H, 150 * QZ);
    ccS(pG, CI_R, 0.15f * QZ); ccS(pG, CI_G, 0.15f * QZ); ccS(pG, CI_B, 0.25f * QZ);
    ccS(pG, CI_A, QZ); ccS(pG, CI_FLG, 1); ccS(pG, CI_ABI, 1); ccS(pG, CI_RED, 8);
    Ser* gc = serCr(pG, 'k', "gc", "op");
    ccS(gc, CI_X, 8 * QZ); ccS(gc, CI_Y, 22 * QZ);
    ccS(gc, CI_W, 60 * QZ); ccS(gc, CI_H, 20 * QZ);
    ccS(gc, CI_R, 0.2f * QZ); ccS(gc, CI_G, 0.3f * QZ); ccS(gc, CI_B, 0.4f * QZ);
    ccS(gc, CI_A, QZ); ccS(gc, CI_FLG, 1 | 2); ccS(gc, CI_RED, 4);
    Ser* gl = serCr(pG, 'l', "gl", "comunes aqui");
    ccS(gl, CI_X, 80 * QZ); ccS(gl, CI_Y, 26 * QZ);
    ccS(gl, CI_R, QZ); ccS(gl, CI_G, QZ); ccS(gl, CI_B, QZ); ccS(gl, CI_A, QZ);
    ccS(gl, CI_FLG, 1);
}

// ---- [T] pruebas (sin ventana) ------------------------------------------------
static int guardado = 0;
static void pruebas() {
    gImg = (unsigned int*)xM((unsigned long)(gW * gH) * sizeof(unsigned int));
    escModelo();
    long antes = gVivos;
    ordCr('b', gRoot);
    escritor();
    printf("prueba crear control nuevo: %s (%ld -> %ld)\n",
           gVivos == antes + 1 ? "PASA" : "FALLA", antes, gVivos);
    pintaTodo();
    Ser* b = elige(30, 60);
    printf("pick sobre boton: %s\n", b && b->tc == 'b' ? "PASA" : "FALLA");
    Ser* sb = elige(320, 220);
    printf("pick sesion anidada en cascada: %s\n",
           sb && sb->tc == 'b' ? "PASA" : "FALLA");
    if (b) {
        long x0 = ccG(b, CI_X);
        ordP(0, b, 1, 120 * QZ, 0, 3, 0, 0);  // r=(ci,val,x,op3)
        escritor();
        printf("arranque guardia x: %s (%ld -> %ld)\n",
               ccG(b, CI_X) > x0 ? "PASA" : "FALLA", x0, ccG(b, CI_X));
    }
    if (b) {
        ccS(b, CI_EXT, 1);
        exRows(b);
        printf("fila SM pulso en Hola: %s (FLG=%ld)\n",
               (ccG(b, CI_FLG) & 2) ? "PASA" : "FALLA", ccG(b, CI_FLG));
        ccS(b, CI_EXT, 0);
    }
    ordP(4, 0, 0, 0, 0, 0, 0, 0);
    escritor();
    FILE* f = fopen(gRuta, "rb");
    if (f) { fclose(f); guardado = 1; }
    long vivos0 = gVivos;
    for (long i = 0; i < gRoot->nA; i++) {          // baseline: forzar full
        Ser* c = gRoot->A[i];
        if (c) for (long q = 0; q < c->nM; q++)
            if (c->TM[q] && c->TM[q][0] == 'g' && c->M[q])
                ((long long*)c->M[q])[3] = -1;
    }
    gImgGen = -1;
    gPxC = 0; gRq = 0;
    pintaTodo();
    long prims1 = gRq;
    printf("primitivas foto completa: %ld\n", prims1);
    gRq = 0;
    pintaTodo();
    printf("cache por version repinta 0 veces: %s\n", gRq == 0 ? "PASA" : "FALLA");
    printf("pick sobre lista colada sin repintar: %s\n",
           elige(30, 60) != 0 ? "PASA" : "FALLA");
    Ser* pn = gRoot->A[0];
    Ser* sld = pn->A[2];
    if (sld->tc == 's') {
        ccS(sld, CI_VAL, 500);
        gRq = 0; gPxC = 0;
        pintaTodo();
        printf("cache por agrupados (repinta solo su grupo): %s (%ld -> %ld prims, %ld%%)\n",
               (gRq > 0 && gRq < prims1) ? "PASA" : "FALLA", prims1, gRq,
               prims1 ? gRq * 100 / prims1 : 0);
    }
    printf("seres sin fuga en serccion: %s\n", guardado ? "PASA" : "FALLA");
    long nN = 0;
    FILE* q = fopen(gRuta, "rb");
    if (q) {
        char lb2[512];
        while (fgets(lb2, sizeof(lb2), q)) if (lb2[0] == 'N') nN++;
        fclose(q);
    }
    printf("vuelta al archivo (N = vivos): %s (%ld vs %ld)\n",
           nN == gVivos ? "PASA" : "FALLA", nN, gVivos);
    FILE* o = fopen("ser_check.ppm", "wb");
    if (o) {
        fprintf(o, "P6\n%ld %ld\n255\n", gW, gH);
        for (long i = 0; i < gW * gH; i++) {
            unsigned char r3 = (unsigned char)((gImg[i] >> 16) & 255);
            unsigned char g3 = (unsigned char)((gImg[i] >> 8) & 255);
            unsigned char b3 = (unsigned char)(gImg[i] & 255);
            fwrite(&r3, 1, 1, o); fwrite(&g3, 1, 1, o); fwrite(&b3, 1, 1, o);
        }
        fclose(o);
    }
    printf("seres: %ld vivos (%ld antes)\n", gVivos, vivos0);
    printf("pruebas listas: ser_check.ppm\n");
}

// ---- [W] frontend Win32 (mismo nucleo) ----------------------------------------
#if defined(_WIN32)
#include <windows.h>
static HWND gVen = 0;
static HDC  gHdc = 0;

static LRESULT CALLBACK wnProc(HWND v, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(v, &ps); EndPaint(v, &ps);   // valida: mata el flood de WM_PAINT
        return 0; }
    case WM_CLOSE: DestroyWindow(v); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_MOUSEMOVE: {
        if (gArra) {
            POINT p; p.x = (long)(short)(l & 65535);
            p.y = (long)(short)((l >> 16) & 65535);
            if (!gSlid) {
                long nx = (long)((p.x - gDOffX) * QZ);
                long ny = (long)((p.y - gDOffY) * QZ);
                ordP(0, gArra, 1, nx, ny, 3, 0, 0);
                escritor();
            } else if (gSel && gSel->tc == 's') {
                Ser* f = gSel->fol;
                float ox = 0;
                for (Ser* q = f; q; q = q->fol) ox += (float)ccG(q, CI_X) / QZ;
                float x = ox + (float)ccG(gSel, CI_X) / QZ;
                float wi = (float)ccG(gSel, CI_W) / QZ;
                float fr = x > 0 ? ((float)p.x - x) / (wi > 0 ? wi : 1) : 0;
                if (fr < 0) fr = 0;
                if (fr > 1) fr = 1;
                ccS(gSel, CI_VAL, (long)(fr * 1000));
            }
        }
        return 0; }
    case WM_LBUTTONDOWN: {
        POINT p; GetCursorPos(&p); ScreenToClient(gVen, &p);
        pisaRaton((float)p.x, (float)p.y);
        SetCapture(gVen);
        return 0; }
    case WM_LBUTTONUP: ReleaseCapture(); sueltoRaton(); return 0;
    case WM_KEYDOWN:
        if (w == VK_ESCAPE) DestroyWindow(gVen);
        if (w == VK_DELETE && gSel) { ordP(2, gSel, 0,0,0,0,0,0); escritor(); }
        tecla((char)w);
        return 0;
    }
    return DefWindowProcA(v, m, w, l);
}

int PASCAL WinMain(HINSTANCE i, HINSTANCE p, LPSTR c, int s) {
    (void)p; (void)c; (void)s;
    WNDCLASSA w;
    w.style = CS_OWNDC;
    w.lpfnWndProc = wnProc;
    w.cbClsExtra = 0; w.cbWndExtra = 0;
    w.hInstance = i;
    w.hIcon = 0; w.hCursor = LoadCursorA(0, IDC_ARROW);
    w.hbrBackground = 0; w.lpszMenuName = 0;
    w.lpszClassName = "ser";
    RegisterClassA(&w);
    RECT rc; rc.left = 0; rc.top = 0; rc.right = gW; rc.bottom = gH;
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, 0);   // marco: titulo + cerrar
    gVen = CreateWindowExA(0, "ser", "ser", WS_OVERLAPPEDWINDOW,
                           60, 60, rc.right - rc.left, rc.bottom - rc.top, 0, 0, i, 0);
    gHdc = GetDC(gVen);
    ShowWindow(gVen, 1);
    gImg = (unsigned int*)xM((unsigned long)(gW * gH) * sizeof(unsigned int));
    if (!desEscena(gRuta)) escModelo();
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = (long)gW;
    bi.bmiHeader.biHeight = (long)-gH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = 0;
    for (;;) {
        MSG m;
        while (PeekMessageA(&m, 0, 0, 0, 1)) {
            if (m.message == WM_QUIT) return 0;
            TranslateMessage(&m); DispatchMessageA(&m);
        }
        escritor();
        pintaTodo();
        SetDIBitsToDevice(gHdc, 0, 0, gW, gH, 0, 0, 0, gH, gImg, &bi, 0);
        Sleep(16);
    }
}
#else
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    pruebas();
    return 0;
}
#endif

