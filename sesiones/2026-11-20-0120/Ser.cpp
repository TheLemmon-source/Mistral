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
//             3=eq ; op 0=set 1=add 2=toggle 3=clip 4=mul
//             op 5=crb: crea rama hija de tipo (char)dst bajo el nodo script;
//             arg = tope de hijos (nA < arg) => cascada sin duplicados.
//  pin      : CI_FLG bit 8: primer plano (colado/pick al final) e inamovible
//             (sin mandaAlFrente, sin arrastre; slider sigue dando valor);
//             vive en espacio de PANTALLA: la camara no lo arrastra.
//  ayuda    : barra fijada al pie: atajos a la vista + linea SEL con el foco
//             vivo; el nodo con foco lleva marco ambar de 2 px.
//  ventana  : resize/maximizar ok: WM_SIZE recrea gImg y repinta todo.
//  camara   : nodo 'm' con fila TM 'x' [img,w,h,ver]; ordFeed (k=6) la carga;
//             sin fuente pinta CAM. Bezel Cam fijado contiene la camara.
//  cubo 3D  : la fuente del par ya proyecta un cubo girando (perspectiva +
//             pintor + luz): el 3D es la misma fila 'x' que produce pixels.
//  colador  : tercer paso (pase del impar cada 8 tics): bbox del mundo fit a
//             gBufCol 64x48; pesos volatiles gColAnti/gColImp/gColUrg (frescura
//             via s->ulti = tic del ultimo toque en ccS); alimenta la camara
//             Colado (gColNodo) por el mismo embudo gColNva.
//  escritor : unico (par obrero en Win; sicrono en pruebas). La GUI encola.
//  raster   : cache por version (solo repinta si cambia el gen), colado en
//             orden de A[] (el mismo orden sirve para pick, multiply z).
//  deshacer : log de asignaciones por grupos; U repone el ultimo grupo.
//  fTx      : filas f6/n9 generadas desde texto (conteo de numeros por linea).
//  camara G : gCamX/gCamY volatiles: pantalla = mundo - camara; op 8 del
//             escritor mueve la camara, C la vuelve al origen.
//  pick Z   : pick recursivo por zonas anidables: panel/sesion/cubo que no
//             contiene al punto cancela su sub-arbol (no recorre todo).
//  hilos    : par = vigilante (tic cronologico + fuente de camara); impar =
//             escritor + pintado (ventana). Embudo interlocked gCamNva y
//             seqlock gCamF en k==6: el impar nunca lee frame a medio armar.
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
    volatile long ulti;             // tic del ultimo toque: frescura del colador
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
static volatile long gTic = 0;               // tic cronologico del vigilante (par)

static Ser* serCr(Ser* fol, char tc, const char* n2, const char* lb) {
    Ser* s;
    if (serPoolU > 0) { s = serPool[--serPoolU]; s->vr++; }
    else { s = (Ser*)xM(sizeof(Ser));
           s->A = 0; s->cA = 0; s->nA = 0; s->M = 0; s->TM = 0; s->nM = 0;
           s->cM = 0; s->C = 0; s->cC = 0; s->nC = 0; s->vr = 0; }
    s->ulti = 0;
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
    if (!s) return;                      // huecos de A[] tras borrados
    while (s->nA > 0) { Ser* h = s->A[s->nA - 1]; s->nA--; serAl(h); }
    if (s->fol)
        for (long i = 0; i < s->fol->cA; i++)
            if (s->fol->A[i] == s) s->fol->A[i] = 0;
    s->fol = 0;
    if (s->lb) { xMe(s->lb); s->lb = 0; }
    for (long i = 0; i < s->nM; i++) {
        if (!s->M[i]) continue;
        if (s->TM[i] && (s->TM[i][0] == 'g' || s->TM[i][0] == 'x'))
            xMe((void*)((long long*)s->M[i])[0]);      // imagen dentro del cache
        xMe(s->M[i]); s->M[i] = 0;
    }
    s->nM = 0;
    s->tc = 0;                           // muerto: tc != 0 significa vivo
    if (serPoolU >= serPoolN) {          // pool: capacidad real, no declarada
        Ser** op = (Ser**)realloc(serPool, (serPoolN + 8) * sizeof(Ser*));
        if (!op) exit(1);
        serPool = op; serPoolN += 8;
    }
    serPool[serPoolU++] = s;
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
    s->ulti = gTic;                      // ultimo toque: frescura para el colador
    s->vr++;
    gGen++;
}

// ---- [U] deshacer: log de asignaciones por grupos ----------------------------
struct Lg { Ser* t; long g; long ci; long old; char* olb; };
static struct Lg gLg[2048];               // un arrastre gasta 2 por pixel
static long gLgN = 0;                    // entradas usadas
static long gLgG = 0;                    // grupo de acciones actual

static void lgEmp() { gLgG++; }          // accion nueva del usuario: grupo nuevo
static void lgE(Ser* t, long ci, long old, char* olb) {
    if (!t || t->tc == 0) { xMe(olb); return; }      // muerto: no se loguea
    if (gLgN >= 128) {                   // sin lugar: se pierde el fondo
        for (long i = 0; i < gLgN; i++) xMe(gLg[i].olb);
        gLgN = 0;
    }
    gLg[gLgN].t = t; gLg[gLgN].g = gLgG; gLg[gLgN].ci = ci;
    gLg[gLgN].old = old; gLg[gLgN].olb = olb;
    gLgN++;
}
static void borraApuntados(Ser* t);      // escritor limpia colgantes al borrar
static void serAl(Ser* s);               // Cementerio libera deshaciendo evict

// Cementerio: borrados con vuelta atras. El nodo se deshoja (queda fuera del
// arbol, no pinta, no pick, no archivo) pero sobrevive hasta 8 borrados.
static Ser* gCem[8];
static long gCemN = 0;
static long deshoja(Ser* s) {
    Ser* f = s->fol;
    if (!f) return 0;
    for (long i = 0; i < f->nA; i++)
        if (f->A[i] == s) { f->A[i] = 0; return 1; }
    return 0;
}
static void cemaSer(Ser* t) {            // eliminar: deshojar y aparcar
    deshoja(t);
    gVivos--;
    lgE(t, -4, 0, 0);                    // deshacer: resucita aca
    if (gCemN >= 8) {                    // evict del mas viejo: ya no vuelve
        Ser* v = gCem[0];
        for (long i = 0; i < gLgN; i++) if (gLg[i].t == v) gLg[i].t = 0;
        gVivos++;                        // ya conto el deshoje al aparcar
        serAl(v);
        for (long i = 1; i < gCemN; i++) gCem[i - 1] = gCem[i];
        gCemN--;
    }
    gCem[gCemN++] = t;
}
static void resucita(Ser* t) {           // volver del cementerio al mismo fol
    Ser* f = t->fol;
    if (!f || f->tc == 0) { serAl(t); return; }
    long k = -1;
    for (long i = 0; i < f->cA; i++) if (!f->A[i]) { k = i; break; }
    if (k < 0) { k = f->nA; if (k >= f->cA) grC(f, k); f->nA = k + 1; }
    f->A[k] = t;
    gVivos++; gGen++;
}
static void deshacer() {                 // repone el ultimo grupo completo
    if (gLgN <= 0) return;
    long g = gLg[gLgN - 1].g;
    long i = gLgN - 1;
    while (i > 0 && gLg[i - 1].g == g) i--;
    for (long j = gLgN - 1; j >= i; j--) {           // en reversa
        Ser* t = gLg[j].t;
        long ci = gLg[j].ci;
        if (!t) { xMe(gLg[j].olb); continue; }
        if (ci == -3) {                              // nodo creado: se quita
            if (t->tc != 0) {
                borraApuntados(t); t->vr++; gGen++; serAl(t);
            }
        } else if (ci == -4) {                       // borrado: se resucita
            if (t->tc != 0) resucita(t);
        } else {
            if (t->tc == 0) { xMe(gLg[j].olb); continue; }
            if (ci == -2) {                          // etiqueta: se repone
                if (gLg[j].olb) { xMe(t->lb); t->lb = gLg[j].olb; gLg[j].olb = 0; }
            } else ccS(t, ci, gLg[j].old);
            t->vr++; gGen++;
        }
    }
    gLgN = i;
}

// ---- [V] SM: filas de condicion y asignacion --------------------------------
static void ordCr(char tc, Ser* fol);      // encola creacion de rama ([Q])
static void ordFeed(Ser* t, unsigned int* im, long w, long h);  // [W] camara
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
// n9: contador IRR de 9 slots [dst, delta, min, max, resetMin, resetMax,
//      compMin, compMax, activo]; valor vive en C[dst]. comp: 0=clip,
//      1=reset, 2=desactivar fila tras el tope.
static double* n9Add(Ser* s, double dst, double delta, double mn, double mx,
                     double rmn, double rmx, double cmn, double cmx, double act) {
    long k = -1;
    for (long i = 0; i < s->nM; i++) if (!s->M[i] || !s->TM[i]) { k = i; break; }
    if (k < 0) { k = s->nM; if (k >= s->cM) grC(s, k); s->nM = k + 1; }
    if (s->M[k]) xMe(s->M[k]);
    s->M[k] = xM(9 * sizeof(double));
    s->TM[k] = (char*)"n9";
    double* n = (double*)s->M[k];
    n[0] = dst; n[1] = delta; n[2] = mn; n[3] = mx;
    n[4] = rmn; n[5] = rmx; n[6] = cmn; n[7] = cmx; n[8] = act;
    return n;
}
static void exN9(Ser* s) {                // contadores: delta + topes + reset
    for (long i = 0; i < s->nM; i++) {
        if (!s->M[i] || !s->TM[i]) continue;
        if (s->TM[i][0] != 'n' || s->TM[i][1] != '9') continue;
        double* n = (double*)s->M[i];
        long dst = (long)n[0];
        if (dst < 0 || dst >= CI_MAX) continue;
        if ((long)n[8] == 0) continue;
        long v = ccG(s, dst) + (long)n[1];
        long mn = (long)n[2], mx = (long)n[3];
        if (mx > mn) {
            if (v > mx) { v = (long)n[5]; if ((long)n[7] == 2) n[8] = 0; }
            else if (v < mn) { v = (long)n[4]; if ((long)n[6] == 2) n[8] = 0; }
        }
        ccS(s, dst, v);
    }
}
static void exRows(Ser* s) {              // ejecucion de filas SM del nodo
    exN9(s);
    for (long i = 0; i < s->nM; i++) {
        if (!s->M[i] || !s->TM[i] || s->TM[i][0] != 'f') continue;
        double* f = (double*)s->M[i];
        long cond = (long)f[0], ci = (long)f[1], cmp = (long)f[2];
        long op = (long)f[3], dst = (long)f[4], arg = (long)f[5];
        long v = ccG(s, ci);
        if (cond == 1) { if (!(v < cmp)) continue; }
        else if (cond == 2) { if (!(v > cmp)) continue; }
        else if (cond == 3) { if (v != cmp) continue; }
        if (op == 5) {                   // meta-UI: crb crear rama hija en cascada
            char tco = (char)dst;
            long cap = arg;
            if (cap < 1) cap = 1;
            if (tco && s->nA < cap) ordCr(tco, s);
            s->vr++; gGen++;
            continue;
        }
        volatile long* d = 0;
        if (dst >= 0 && dst < s->nC) d = &s->C[dst];
        else if (dst < 0 && s->fol) {       // arco al fol: -1-k = canal k del padre
            long dk = -dst - 1;
            if (dk >= 0 && dk < s->fol->nC) d = &s->fol->C[dk];
        }
        if (!d) continue;
        if (op == 0) *d = arg;                      // asignacion
        else if (op == 1) *d += arg;
        else if (op == 2) *d ^= arg;
        else if (op == 3) { if (*d > arg) *d = arg; if (*d < cmp) *d = cmp; }
        else if (op == 4) *d *= arg;
        s->vr++; gGen++;
        if (dst < 0 && s->fol) s->fol->vr++;        // el fol tambien cambia
    }
}
static void tickN9(Ser* s) {              // contadores vivos: recorre el arbol
    if (!s) return;
    exN9(s);
    for (long i = 0; i < s->nA; i++) tickN9(s->A[i]);
}
static long fTx(Ser* s, const char* dtx) { // filas desde texto: conteo por linea
    if (!s || !dtx) return 0;
    long hechas = 0;
    const char* p = dtx;                   // lineas por \n o ';': solo numeros
    while (*p) {
        double v[9]; long k = 0;
        while (*p && *p != '\n' && *p != ';') {
            while (*p == ' ' || *p == ',' || *p == '\t') p++;
            if (!*p || *p == '\n' || *p == ';') break;
            char* q;
            double d = strtod(p, &q);
            if (q == p) { p++; continue; }
            if (k < 9) v[k] = d;
            k++;
            p = q;
        }
        if (k == 9) {
            n9Add(s, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8]);
            hechas++;
        } else if (k >= 6) {
            f6Add(s, v[0], v[1], v[2], v[3], v[4], v[5]);
            hechas++;
        }
        while (*p && *p != '\n' && *p != ';') p++;
        if (*p) p++;
    }
    return hechas;
}

// ---- [Q] cola de ordenes (GUI -> escritor unico) ----------------------------
struct Ord { long k; Ser* t; long r[6]; char ch; char* tx; };
static Ord  oRing[256];
static volatile long oHead = 0, oTail = 0;
static char  gRuta[512] = "oroga.dbg";

static void lleno() {
    fprintf(stderr, "ser: cola llena, orden perdida\n");
}

static void ordP(long k, Ser* t, long a, long b, long c, long d, long e, long f) {
    long h = oHead;
    if (h - oTail >= 256) { lleno(); return; }
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
    if (h - oTail >= 256) { lleno(); return; }
    oRing[h & 255].k = 1;
    oRing[h & 255].t = fol;
    oRing[h & 255].ch = tc;
    oRing[h & 255].tx = 0;
    oHead = h + 1;
}
static void ordFeed(Ser* t, unsigned int* im, long w, long h) {  // [[6] camara
    long hh = oHead;
    if (hh - oTail >= 256) { lleno(); return; }
    oRing[hh & 255].k = 6;
    oRing[hh & 255].t = t;
    oRing[hh & 255].r[0] = w; oRing[hh & 255].r[1] = h;
    oRing[hh & 255].r[2] = 0; oRing[hh & 255].r[3] = 0;
    oRing[hh & 255].r[4] = 0; oRing[hh & 255].r[5] = 0;
    oRing[hh & 255].ch = 0;
    oRing[hh & 255].tx = (char*)im;
    oHead = hh + 1;
}

static Ser* gRoot = 0;
static Ser* gSes  = 0;                  // contenedor de sesiones (ramas q)
static Ser* gForm = 0;                  // nodo script meta-UI (filas op 5)
static Ser* gCamNodo = 0;               // camara 'm' alimentada en ventana
static Ser* gObj = 0;                   // nodo inspeccionado
static Ser* gInsp = 0;                  // panel inspector (fijado)
static Ser* gInspEd = 0;                // check modo etiqueta
static Ser* gInspVis = 0;               // check visible
static Ser* gInspPn = 0;                // check pin
static Ser* gInspR = 0; static Ser* gInspG = 0;
static Ser* gInspB = 0; static Ser* gInspRad = 0;
static Ser* gAyTxt = 0;                 // estado vivo: foco + teclado
static long gEdito = 0;                 // escribiendo etiqueta del objetivo
static volatile long gCamX = 0, gCamY = 0;  // camara global: pantalla = mundo - cam
static volatile long gCamVer = 0;       // version de camara: invalida el volcado
static long gCamVisto = -1;
__attribute__((unused)) static long gCamF = 0;                  // fase de la fuente
__attribute__((unused)) static unsigned int gBufCam[32 * 24];   // marco sintetico
static volatile long gFS = 2;           // escala de texto: celdas por glifo
static long gNvoX = 24, gNvoY = 24;
static void nvoN2(char* out, char t) {           // 2 chars unicos
    const char* ab[10] = { "b1","k1","s1","l1","p1","e1","c1","q1","r1","m1" };
    long i = (t == 'b') ? 0 : (t == 'k') ? 1 : (t == 's') ? 2 : (t == 'l') ? 3
           : (t == 'p') ? 4 : (t == 'e') ? 5 : (t == 'q') ? 7 : (t == 'm') ? 9 : 6;
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

// ---- [X] camara: fila 'x' = [img,w,h,ver] -----------------------------------
static long long* camFil(Ser* s) {       // frame vivo si existe
    for (long i = 0; i < s->nM; i++)
        if (s->TM[i] && s->TM[i][0] == 'x' && s->M[i]) return (long long*)s->M[i];
    return 0;
}
static long long* camRow(Ser* s, long w, long h) {   // crea/ajusta fila 'x'
    long k = -1;
    for (long i = 0; i < s->nM; i++)
        if (s->TM[i] && s->TM[i][0] == 'x') { k = i; break; }
    if (k < 0) { k = s->nM; if (k >= s->cM) grC(s, k); s->nM = k + 1;
                 s->TM[k] = (char*)"x"; s->M[k] = 0; }
    long long* c4 = (long long*)s->M[k];
    if (w < 1) w = 1;         // LLP64: producto en long de 32 bits
    if (w > 2048) w = 2048;   // desborda a negativo: tope, no crash
    if (h < 1) h = 1;
    if (h > 2048) h = 2048;
    if (!c4 || c4[1] != w || c4[2] != h) {
        if (c4) xMe((void*)c4[0]);
        size_t nt = (size_t)w * (size_t)h;
        unsigned int* im = (unsigned int*)xM(nt * sizeof(unsigned int));
        memset(im, 0, nt * sizeof(unsigned int));
        if (!c4) c4 = (long long*)xM(4 * sizeof(long long));
        c4[0] = (long long)im; c4[1] = w; c4[2] = h; c4[3] = 0;
        s->M[k] = c4;
    }
    return c4;
}

// ---- [H] hilos por pares: par = vigilante, impar = escritor (interlocked) ---
static volatile long gCamNva = 0;       // embudo: hay frame nuevo para el impar
static volatile long gParar = 0;        // apaga los hilos (fin de sesion)
#if defined(_WIN32)
#include <windows.h>
static void dormir(long ms) { Sleep(ms); }
#else
#include <unistd.h>
#include <pthread.h>
static void dormir(long ms) { usleep((useconds_t)ms * 1000); }
#endif
static void triLleno(unsigned int* im, long w, long h,
                     float ax, float ay, float bx, float by,
                     float cx, float cy, unsigned int col) {
    float tx;
    if (ay > by) { tx = ax; ax = bx; bx = tx; tx = ay; ay = by; by = tx; }
    if (ay > cy) { tx = ax; ax = cx; cx = tx; tx = ay; ay = cy; cy = tx; }
    if (by > cy) { tx = bx; bx = cx; cx = tx; tx = by; by = cy; cy = tx; }
    if (cy - ay < 0.5f) return;
    for (long y = 0; y < h; y++) {
        float py = (float)y + 0.5f;
        if (py < ay || py >= cy) continue;
        float t1 = (py - ay) / (cy - ay);
        float xl = ax + t1 * (cx - ax);
        float xr;
        if (py < by) {
            float t2 = (by - ay > 0.0001f) ? (py - ay) / (by - ay) : 0.5f;
            xr = ax + t2 * (bx - ax);
        } else {
            float t2 = (cy - by > 0.0001f) ? (py - by) / (cy - by) : 0.5f;
            xr = bx + t2 * (cx - bx);
        }
        if (xl > xr) { float sw = xl; xl = xr; xr = sw; }
        long x0 = (long)(xl + 0.5f), x1 = (long)(xr + 0.5f);
        if (x0 < 0) x0 = 0;
        if (x1 > w - 1) x1 = w - 1;
        for (long x = x0; x <= x1; x++) im[y * w + x] = col;
    }
}
static void camFuente() {               // el par proyecta el cubo 3D
    if (!gCamNodo) return;
    static const float VX[8][3] = {
        {-1,-1,-1},{ 1,-1,-1},{ 1, 1,-1},{-1, 1,-1},
        {-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1}
    };
    static const long FG[6][4] = {       // caras: top, bottom, left, right, frent, fond
        {2,3,7,6},{0,1,5,4},{0,3,7,4},{1,2,6,5},{0,1,2,3},{4,5,6,7}
    };
    static const float CN[6][3] = {
        {0,1,0},{0,-1,0},{-1,0,0},{1,0,0},{0,0,-1},{0,0,1}
    };
    static const float CB[6][3] = {
        {0.9f,0.3f,0.2f},{0.4f,0.4f,0.5f},{0.5f,0.8f,0.3f},
        {0.9f,0.75f,0.2f},{0.8f,0.4f,0.8f},{0.3f,0.8f,0.8f}
    };
    float aY = gCamF * 0.021f, aX = 0.6f + gCamF * 0.009f;
    float cY = (float)cos((double)aY), sY = (float)sin((double)aY);
    float cX = (float)cos((double)aX), sX = (float)sin((double)aX);
    float px[8], py[8], pz[8], nr[6][3];
    for (long i = 0; i < 8; i++) {
        float x = VX[i][0], y = VX[i][1], z = VX[i][2];
        float x2 = x * cY + z * sY;
        float z2 = -x * sY + z * cY;
        float y2 = y * cX - z2 * sX;
        float z3 = y * sX + z2 * cX;
        float inv = 1.0f / (z3 + 3.2f);       // perspectiva
        px[i] = 16.0f + x2 * inv * 30.0f;
        py[i] = 12.0f - y2 * inv * 24.0f;
        pz[i] = z3;
    }
    float az[6];
    for (long f = 0; f < 6; f++) {
        az[f] = (pz[FG[f][0]] + pz[FG[f][1]] + pz[FG[f][2]] + pz[FG[f][3]]) * 0.25f;
        float nx = CN[f][0], ny = CN[f][1], nz = CN[f][2];
        nr[f][0] = nx * cY + nz * sY;
        float nz2 = -nx * sY + nz * cY;
        nr[f][1] = ny * cX - nz2 * sX;
        nr[f][2] = ny * sX + nz2 * cX;
    }
    long ord[6] = { 0,1,2,3,4,5 };            // pintor: lejos primero
    for (long a = 0; a < 5; a++)
        for (long b3 = a + 1; b3 < 6; b3++)
            if (az[ord[b3]] > az[ord[a]]) { long sw = ord[a]; ord[a] = ord[b3]; ord[b3] = sw; }
    for (long oi = 0; oi < 6; oi++) {
        long f = ord[oi];
        float dx = 0.4f - nr[f][2] * 0.7f;    // luz fija desde arriba-frente
        float dy = -nr[f][1] * -0.6f + nr[f][2] * 0.4f;
        float sh = 0.35f + 0.65f * (dx + dy);
        if (sh < 0.3f) sh = 0.3f;
        if (sh > 1.0f) sh = 1.0f;
        unsigned int col = pr(CB[f][0] * sh, CB[f][1] * sh, CB[f][2] * sh, 1.0f);
        long a = FG[f][0], b3 = FG[f][1], c = FG[f][2], d = FG[f][3];
        triLleno(gBufCam, 32, 24, px[a], py[a], px[b3], py[b3], px[c], py[c], col);
        triLleno(gBufCam, 32, 24, px[a], py[a], px[c], py[c], px[d], py[d], col);
    }
    gCamF += 3;
    __sync_lock_test_and_set(&gCamNva, 1);   // interlocked: hay frame nuevo
}

// ---- [C] colador: tercer proceso, escena -> imagen con pesos volatiles -------
static volatile long gColAnti = 400, gColImp = 300, gColUrg = 300;   // pesos 0..1000
static Ser* gColNodo = 0;               // camara 'm' que muestra lo colado
static volatile long gColNva = 0;       // embudo: colado nuevo
static unsigned int gBufCol[64 * 48];   // imagen colada (fit del bbox del mundo)
static void colador() {
    if (!gRoot) return;
    for (long i = 0; i < 64 * 48; i++) gBufCol[i] = 0;
    long bx0 = 1 << 30, by0 = 1 << 30, bx1 = -(1 << 30), by1 = -(1 << 30);
    for (long i = 0; i < gRoot->nA; i++) {
        Ser* s = gRoot->A[i];
        if (!s || s->tc == 0) continue;
        long fl = ccG(s, CI_FLG);
        if (s->tc != 'r' && !(fl & 1)) continue;
        long w = ccG(s, CI_W), h = ccG(s, CI_H);
        if (w <= 0 || h <= 0) continue;
        long x = ccG(s, CI_X), y = ccG(s, CI_Y);
        if (x < bx0) bx0 = x;
        if (y < by0) by0 = y;
        if (x + w > bx1) bx1 = x + w;
        if (y + h > by1) by1 = y + h;
    }
    if (bx1 <= bx0 || by1 <= by0) return;
    float fs = 60.0f / (float)(bx1 - bx0);
    float fy = 44.0f / (float)(by1 - by0);
    if (fy < fs) fs = fy;
    for (long i = 0; i < gRoot->nA; i++) {
        Ser* s = gRoot->A[i];
        if (!s || s->tc == 0) continue;
        long fl = ccG(s, CI_FLG);
        if (s->tc != 'r' && !(fl & 1)) continue;
        long w = ccG(s, CI_W), h = ccG(s, CI_H);
        if (w <= 0 || h <= 0) continue;
        long impL = (fl & 8) ? 900 : (ccG(s, CI_ABI) ? 500 : ((fl & 1) ? 300 : 0));
        long dc = gTic - s->ulti;
        float fres = 1000.0f - (dc > 16000 ? 1000.0f : (float)dc / 16.0f);
        if (fres < 0) fres = 0;
        float urg = s->nM > 0 ? 600.0f : 100.0f;
        float sc = ((float)gColAnti * fres + (float)gColImp * (float)impL
                    + (float)gColUrg * urg) / 1000.0f;
        if (sc < 1.0f) continue;         // el colador lo deja pasar de largo
        float ax = 2.0f + ((float)ccG(s, CI_X) - (float)bx0) * fs;
        float ay = 2.0f + ((float)ccG(s, CI_Y) - (float)by0) * fs;
        float aw = (float)w * fs, ah = (float)h * fs;
        if (aw < 1.0f) aw = 1.0f;
        if (ah < 1.0f) ah = 1.0f;
        unsigned int col = pr((float)ccG(s, CI_R) / QZ, (float)ccG(s, CI_G) / QZ,
                              (float)ccG(s, CI_B) / QZ, (float)ccG(s, CI_A) / QZ);
        triLleno(gBufCol, 64, 48, ax, ay, ax + aw, ay, ax, ay + ah, col);
        triLleno(gBufCol, 64, 48, ax + aw, ay, ax, ay + ah, ax + aw, ay + ah, col);
    }
    __sync_lock_test_and_set(&gColNva, 1);
}
#if defined(_WIN32)
static DWORD __stdcall alVigilante(void* p) {
    (void)p;
    while (!gParar) {
        dormir(16);
        __sync_fetch_and_add(&gTic, 1);
        camFuente();
    }
    return 0;
}
static void parArranca() {
    HANDLE h = CreateThread(0, 0, alVigilante, 0, 0, 0);
    if (h) CloseHandle(h);
}
static void parParar() { gParar = 1; }
#else
static pthread_t gHiloVig;
static void* alVigilante(void* p) {
    (void)p;
    while (!gParar) {
        dormir(16);
        __sync_fetch_and_add(&gTic, 1);
        camFuente();
    }
    return 0;
}
static void parArranca() { pthread_create(&gHiloVig, 0, alVigilante, 0); }
static void parParar() { gParar = 1; pthread_join(gHiloVig, 0); }
#endif

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
    } else if (s->tc == 'm') {   // camara: frame vivo o placeholder
        long long* fc = camFil(s);
        if (fc && fc[0] && fc[1] > 0 && fc[2] > 0) {
            unsigned int* im = (unsigned int*)fc[0];
            for (long j = 0; j < (long)h; j++)
                for (long i = 0; i < (long)w; i++) {
                    long si = (long)((float)i * (float)fc[1] / w);
                    long sj = (long)((float)j * (float)fc[2] / h);
                    px((int)(x + i), (int)(y + j), im[sj * (long)fc[1] + si]);
                }
        } else {
            rectoQ(x, y, w, h, pr(0.08f, 0.1f, 0.14f, a), red);
            texto(x + 4, y + (h - 5 * gFS) * 0.5f, "CAM", pr(0.7f, 0.8f, 0.9f, 1));
        }
    } else {   // 'p' y 'r' y 'o': panel (recuadre redondeado de demo)
        rectoQ(x, y, w, h, col, red);
        rectoQ(x + 1, y + 1, w - 2 < 0 ? 0 : w - 2, 14 < h - 2 ? 14 : h - 2,
               pr(r * 1.35f, g * 1.35f, b * 1.35f, a > 0.5f ? 1 : a), red);
        texto(x + 4, y + 3, s->lb, pr(1, 1, 1, 1));
    }
    if (s->tc != 'r' && ccG(s, CI_SEL)) {   // foco: marco ambar de 2 px
        unsigned int ac = pr(1, 0.75f, 0.15f, 1);
        rectoQ(x - 2, y - 2, w + 4, 2, ac, 0);
        rectoQ(x - 2, y + h, w + 4, 2, ac, 0);
        rectoQ(x - 2, y, 2, h, ac, 0);
        rectoQ(x + w, y, 2, h, ac, 0);
    }
    if (s->tc != 'p' || (ccG(s, CI_ABI) & 1)) {   // cascada: despliegue
        for (long pase = 0; pase < 2; pase++)     // pin CI_FLG&8 al final
            for (long i = 0; i < s->nA; i++) {
                Ser* h = s->A[i];
                if (!h || (((ccG(h, CI_FLG) & 8) != 0) != (pase == 1))) continue;
                pintaNodo(h, x, y);
            }
    }
}

static void listaRec(Ser* s) {           // lista colada: pintador y pick
    if (!s) return;
    if (s->tc == 'r') {
        for (long pase = 0; pase < 2; pase++)   // pin CI_FLG&8 al final
            for (long i = 0; i < s->nA; i++) {
                Ser* h = s->A[i];
                if (!h || (((ccG(h, CI_FLG) & 8) != 0) != (pase == 1))) continue;
                listaRec(h);
            }
        return;
    }
    if (!(ccG(s, CI_FLG) & 1)) return;
    if (gLinN < 1024) gLin[gLinN++] = s;
    else fprintf(stderr, "ser: colado de 1024 alcanzado: nodo sin pick\n");
    if (s->tc != 'p' || (ccG(s, CI_ABI) & 1))
        for (long pase = 0; pase < 2; pase++)   // pin CI_FLG&8 al final
            for (long i = 0; i < s->nA; i++) {
                Ser* h = s->A[i];
                if (!h || (((ccG(h, CI_FLG) & 8) != 0) != (pase == 1))) continue;
                listaRec(h);
            }
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
    if (w < 1) w = 1;                    // datos corruptos: tope, no crash
    if (w > 2048) w = 2048;
    return w + 6;
}
static long extH(Ser* s) {
    long h = ccG(s, CI_H) / QZ;
    if (h < 16) h = 16;
    if (h > 2048) h = 2048;
    return h + 6;
}
static long long* cacheRow(Ser* s, long w, long h) {   // M fila 'g': [img,w,h,vv]
    long k = -1;
    for (long i = 0; i < s->nM; i++)
        if (s->TM[i] && s->TM[i][0] == 'g') { k = i; break; }
    if (k < 0) { k = s->nM; if (k >= s->cM) grC(s, k); s->nM = k + 1;
                 s->TM[k] = (char*)"g"; s->M[k] = 0; }
    long long* c4 = (long long*)s->M[k];
    if (w < 1) w = 1;             // LLP64: el producto en long de
    if (w > 2048) w = 2048;       // 32 bits desborda a negativo
    if (h < 1) h = 1;
    if (h > 2048) h = 2048;
    if (!c4 || c4[1] < w || c4[2] < h) {
        long nw = (c4 && c4[1] > w) ? (long)c4[1] : w;
        long nh = (c4 && c4[2] > h) ? (long)c4[2] : h;
        if (c4) xMe((void*)c4[0]);
        size_t nt = (size_t)nw * (size_t)nh;
        unsigned int* im = (unsigned int*)xM(nt * sizeof(unsigned int));
        memset(im, 0, nt * sizeof(unsigned int));
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
// ---- [3D] modo 3D: la escena como tableros en profundidad, misma fila 'x' ----
// Cada nodo visible es una placa (x,y = su caja; z = -1.1 * nivel): el padre
// queda detras y los hijos adelante. Camara orbital: arrastre o flechas giran,
// +/- acerca; un clic elige el tablero (mismo Ser, mismo pick, mismo undo).
static long g3D = 0;                     // modo 3D activo
static float g3Az = 0.7f, g3El = 0.55f, g3Zm = 1.0f;   // orbita y zoom
static void pr3(float x, float y, float z,
                float* sx, float* sy, float* w3) {   // proyecta un punto mundo
    float cY = (float)cos((double)g3Az), sY = (float)sin((double)g3Az);
    float cX = (float)cos((double)g3El), sX = (float)sin((double)g3El);
    float x2 = x * cY + z * sY;
    float z2 = -x * sY + z * cY;
    float y2 = y * cX - z2 * sX;
    float q3 = y * sX + z2 * cX;
    if (w3) *w3 = q3;
    float inv = 1.0f / (q3 + 6.0f);
    float esc = 180.0f * g3Zm;           // 1:1 en el centro (mundo = px/30)
    *sx = (float)gW * 0.5f + x2 * inv * esc;
    *sy = (float)gH * 0.5f - y2 * inv * esc;
}
#define G3MAX 256
static Ser* g3Se[G3MAX];                 // pick: nodo -> centro proyectado
static float g3Px[G3MAX], g3Py[G3MAX];
static long g3Nv[G3MAX];
static long g3N3 = 0;
static void reco3D(Ser* s, long nivel) {
    if (!s || s->tc == 0 || nivel > 2 || g3N3 >= G3MAX) return;
    long fl = ccG(s, CI_FLG);
    if (s->tc != 'r') {
        if (!(fl & 1) || ccG(s, CI_W) <= 0) return;
        g3Se[g3N3] = s;
        g3Nv[g3N3] = nivel;
        g3N3++;
    }
    for (long i = 0; i < s->nA; i++) reco3D(s->A[i], nivel + 1);
}
static Ser* elige3D(float mx, float my) {
    Ser* r = 0;
    float bd = 18.0f;
    for (long i = 0; i < g3N3; i++) {
        float dx = g3Px[i] - mx, dy = g3Py[i] - my;
        float d = (float)sqrt((double)(dx * dx + dy * dy));
        if (d < bd) { bd = d; r = g3Se[i]; }
    }
    return r;
}
static void pinta3D() {
    if (!gImg || !gRoot) return;
    long gFS0 = gFS;
    gTgt = gImg; gTW = gW; gTH = gH;
    unsigned int bg = pr(0.05f, 0.06f, 0.09f, 1.0f);
    for (unsigned long i = 0; i < (unsigned long)gW * (unsigned long)gH; i++)
        gImg[i] = bg;
    static const long FF[6][4] = { {0,1,3,2},{4,5,7,6},{0,1,5,4},
                                   {2,3,7,6},{0,2,6,4},{1,3,7,5} };
    static const float FN[6][3] = { {0,0,-1},{0,0,1},{0,-1,0},
                                    {0,1,0},{-1,0,0},{1,0,0} };
    g3N3 = 0;
    reco3D(gRoot, 0);
    long bx0 = 1 << 30, by0 = 1 << 30, bx1 = -(1 << 30), by1 = -(1 << 30);
    for (long i = 0; i < g3N3; i++) {
        Ser* t = g3Se[i];
        long x = ccG(t, CI_X) / QZ, y = ccG(t, CI_Y) / QZ;
        long w = ccG(t, CI_W) / QZ, h = ccG(t, CI_H) / QZ;
        if (x < bx0) bx0 = x;
        if (y < by0) by0 = y;
        if (x + w > bx1) bx1 = x + w;
        if (y + h > by1) by1 = y + h;
    }
    if (bx1 > bx0 && by1 > by0) {
        float ccx = (float)(bx0 + bx1) * 0.5f / 30.0f;
        float ccy = (float)(by0 + by1) * 0.5f / 30.0f;
        long opos[256];
        float zz[256];
        for (long i = 0; i < g3N3; i++) {
            opos[i] = i;
            Ser* t = g3Se[i];
            float wx = ((float)(ccG(t, CI_X) + ccG(t, CI_W) / 2))
                       / QZ / 30.0f - ccx;
            float wy = ((float)(ccG(t, CI_Y) + ccG(t, CI_H) / 2))
                       / QZ / 30.0f - ccy;
            float qz = 0, sx = 0, sy = 0;
            pr3(wx, wy, -1.1f * (float)g3Nv[i], &sx, &sy, &qz);
            zz[i] = qz;
            g3Px[i] = sx; g3Py[i] = sy;
        }
        for (long a = 0; a < g3N3; a++)          // pintor: placas lejos primero
            for (long b2 = a + 1; b2 < g3N3; b2++)
                if (zz[opos[b2]] > zz[opos[a]]) {
                    long sw = opos[a]; opos[a] = opos[b2]; opos[b2] = sw;
                }
        for (long oi = 0; oi < g3N3; oi++) {
            Ser* t = g3Se[opos[oi]];
            long niv = g3Nv[opos[oi]];
            float w = (float)ccG(t, CI_W) / QZ;
            float h = (float)ccG(t, CI_H) / QZ;
            if (w < 1.0f) w = 1.0f;
            if (h < 1.0f) h = 1.0f;
            if (w > 1200.0f) w = 1200.0f;        // placas gigantes: tope
            if (h > 1200.0f) h = 1200.0f;
            float hw = w / 60.0f, hh = h / 60.0f, dz = 0.35f;
            float wx = (float)ccG(t, CI_X) / QZ + hw - ccx;
            float wy = (float)ccG(t, CI_Y) / QZ + hh - ccy;
            float wz = -1.1f * (float)niv;
            float pxc[8], pyc[8], pzc[8];
            for (long c = 0; c < 8; c++)
                pr3(wx + ((c & 1) ? hw : -hw), wy + ((c & 2) ? hh : -hh),
                    wz + ((c & 4) ? dz : -dz),
                    &pxc[c], &pyc[c], &pzc[c]);
            float fr = (float)ccG(t, CI_R) / QZ, fg2 = (float)ccG(t, CI_G) / QZ;
            float fb = (float)ccG(t, CI_B) / QZ;
            long fo[6] = { 0,1,2,3,4,5 };
            for (long a = 0; a < 5; a++)          // caras lejos -> cerca
                for (long b2 = a + 1; b2 < 6; b2++) {
                    float za = 0, zb = 0;
                    for (long v = 0; v < 4; v++) {
                        za += pzc[FF[fo[a]][v]];
                        zb += pzc[FF[fo[b2]][v]];
                    }
                    if (zb > za) { long sw = fo[a]; fo[a] = fo[b2]; fo[b2] = sw; }
                }
            for (long fi = 0; fi < 6; fi++) {
                long f = fo[fi];
                float nx = FN[f][0], ny = FN[f][1], nz = FN[f][2];
                float cY = (float)cos((double)g3Az), sY = (float)sin((double)g3Az);
                float cX = (float)cos((double)g3El), sX = (float)sin((double)g3El);
                float z2 = -nx * sY + nz * cY;
                float rz = ny * sX + z2 * cX;
                float ry = ny * cX - z2 * sX;
                float sh = 0.35f + 0.65f * (0.4f - rz * 0.7f + 0.6f * ry
                                            + 0.4f * rz);
                if (sh < 0.2f) sh = 0.2f;
                if (sh > 1.0f) sh = 1.0f;
                unsigned int col = pr(fr * sh, fg2 * sh, fb * sh, 1.0f);
                long a0 = FF[f][0], a1 = FF[f][1], a2 = FF[f][2], a3 = FF[f][3];
                triLleno(gImg, gW, gH, pxc[a0], pyc[a0], pxc[a1], pyc[a1],
                         pxc[a2], pyc[a2], col);
                triLleno(gImg, gW, gH, pxc[a0], pyc[a0], pxc[a2], pyc[a2],
                         pxc[a3], pyc[a3], col);
            }
        }
    }
    gFS = 2;                             // etiquetas: planas, siempre legibles
    for (long i = 0; i < g3N3; i++) {
        Ser* t = g3Se[i];
        char lbm[16];
        lbm[0] = 0;
        if (t->lb) {
            long m2 = 0;
            while (t->lb[m2] && m2 < 10) { lbm[m2] = t->lb[m2]; m2++; }
            lbm[m2] = 0;
        }
        unsigned int col = (t == gObj) ? pr(1.0f, 0.72f, 0.2f, 1.0f)
                                       : pr(0.8f, 0.85f, 0.9f, 1.0f);
        texto(g3Px[i] - 12.0f, g3Py[i] - 4.0f, lbm, col);
    }
    texto(8, (float)gH - 12.0f,
          "3D  G:VUELVE  FLEC:GIRA  +/-:ZOOM  CLIC:ELIGE", pr(0.5f, 0.55f, 0.6f, 1.0f));
    gFS = gFS0;
    gImgGen = -1;                        // en 3D se repinta cada frame
}
static void pintaTodo() {
    if (!gImg) return;
    if (g3D) { pinta3D(); return; }      // 3D: vista orbital de la escena
    if (gImgGen == gGen && gCamVisto == gCamVer) return;   // nada cambio en TODO
    gTgt = gImg; gTW = gW; gTH = gH;
    gLinN = 0;
    listaRec(gRoot);                              // pick comparte el colado
    if (gRoot) {
        for (long pase = 0; pase < 2; pase++)         // pin CI_FLG&8 al final
        for (long i = 0; i < gRoot->nA; i++) {
            Ser* c = gRoot->A[i];
            if (!c || (((ccG(c, CI_FLG) & 8) != 0) != (pase == 1))) continue;
            long w = extW(c), h = extH(c);
            long long* c4 = cacheRow(c, w, h);
            long vis = ccG(c, CI_FLG) & 1;
            long pin = ccG(c, CI_FLG) & 8;
            int ax = (int)(ccG(c, CI_X) - (pin ? 0 : gCamX) - 3 * QZ) / QZ;
            int ay = (int)(ccG(c, CI_Y) - (pin ? 0 : gCamY) - 3 * QZ) / QZ;
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
    gCamVisto = gCamVer;
}

// ---- [K] pick por zonas anidables (mismo orden de colado) ------------------
static Ser* eligeZ(Ser* s, float mx, float my) {  // mx,my: coords del fol de s
    if (!s) return 0;
    long fl = ccG(s, CI_FLG);
    if (s->tc != 'r' && !(fl & 1)) return 0;
    float x = 0, y = 0, w = 0, h = 0;
    if (s->tc != 'r') {
        x = (float)ccG(s, CI_X) / QZ;
        y = (float)ccG(s, CI_Y) / QZ;
        w = (float)ccG(s, CI_W) / QZ;
        h = (float)ccG(s, CI_H) / QZ;
    }
    char en = (mx >= x && my >= y && mx <= x + w && my <= y + h);
    if (s->tc != 'r' && !en
        && (s->tc == 'p' || s->tc == 'q' || s->tc == 'c'))
        return 0;                            // zona cancelante: corta la rama
    Ser* hit = 0;
    if (s->tc != 'p' || (ccG(s, CI_ABI) & 1))
        for (long pase = 0; pase < 2; pase++)         // pin al final: mismo orden
            for (long i = 0; i < s->nA; i++) {
                Ser* hn = s->A[i];
                if (!hn || (((ccG(hn, CI_FLG) & 8) != 0) != (pase == 1))) continue;
                float cmx = 0, cmy = 0;
                if (s == gRoot && !(ccG(hn, CI_FLG) & 8)) {
                    cmx = (float)gCamX / QZ;
                    cmy = (float)gCamY / QZ;          // pin: espacio de pantalla
                }
                Ser* r2 = eligeZ(hn, mx - x + cmx, my - y + cmy);
                if (r2) hit = r2;             // los ultimos pintados ganan
            }
    if (!hit && s->tc != 'r' && en) hit = s;
    return hit;
}
static Ser* elige(float mx, float my) {       // camara por hijo: pin fijo
    return eligeZ(gRoot, mx, my);
}
static void ratonMundo(Ser* s, float mx, float my, float* ox, float* oy) {
    if (ox) *ox = mx;
    if (oy) *oy = my;
    Ser* r = s;
    while (r && r->fol && r->fol != gRoot) r = r->fol;   // hijo tope de la rama
    if (!r || (r->fol == gRoot && (ccG(r, CI_FLG) & 8))) return;   // pin: pantalla
    if (ox) *ox = mx + (float)gCamX / QZ;
    if (oy) *oy = my + (float)gCamY / QZ;
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
                    lgE(t, CI_X, ccG(t, CI_X), 0);
                    ccS(t, CI_X, o->r[1]);
                    lgE(t, CI_Y, ccG(t, CI_Y), 0);
                    ccS(t, CI_Y, o->r[2]);
                    zapFol(t);
                    for (int n = 0; n < 4 && pisaHermano(t); n++) {
                        ccS(t, CI_Y, ccG(t, CI_Y) + ccG(t, CI_H) + QZ);
                        zapFol(t);
                    }
                } else {
                    long ci = o->r[0], v = o->r[1];
                    long act = ccG(t, ci);
                    lgE(t, ci, act, 0);
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
                             : (o->ch == 'q') ? "Ses" : (o->ch == 'm') ? "Cam"
                             : "Nvo";
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
            lgE(s, -3, 0, 0);                    // crear = deshacer lo quita
            f6Add(s, 2, (double)CI_EXT, 0.0, 2.0, (double)CI_FLG, 2.0);
        } else if (o->k == 2) {                  // eliminar
            if (o->t && o->t->tc != 'r') {
                cemaSer(o->t);                  // cementerio: U lo resucita
                borraApuntados(o->t);           // sin nodos fantasma (crash editar)
                gGen++;
            }
        } else if (o->k == 3) {                  // mando al frente (z)
            if (o->t) mandaAlFrente(o->t);
        } else if (o->k == 4) {                  // guardar oroga.dbg
            FILE* f = fopen(gRuta, "wb");
            if (f) {
                long idx = 0; long cort = 0;
                Ser* lin[1024]; long folto[1024];
                lin[idx] = gRoot; folto[idx] = -1; idx++;
                for (long p = 0; p < idx && p < 1024; p++) {
                    Ser* s = lin[p];
                    fprintf(f, "N %ld %ld %c %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %s\n",
                        p, folto[p], s->tc,
                        ccG(s, CI_X), ccG(s, CI_Y), ccG(s, CI_W), ccG(s, CI_H),
                        ccG(s, CI_R), ccG(s, CI_G), ccG(s, CI_B), ccG(s, CI_A),
                        ccG(s, CI_ABI), ccG(s, CI_FLG), ccG(s, CI_VAL),
                        s->lb ? s->lb : "");   // etiqueta AL FINAL: espacios seguros
                    for (long m = 0; m < s->nM; m++) {
                        if (!s->M[m] || !s->TM[m] || s->TM[m][0] != 'f') continue;
                        double* r6 = (double*)s->M[m];
                        fprintf(f, "V %ld %g %g %g %g %g %g %g\n", p,
                            r6[0], r6[1], r6[2], r6[3], r6[4], r6[5], 0.0);
                    }
                    for (long m = 0; m < s->nM; m++) {
                        if (!s->M[m] || !s->TM[m] || s->TM[m][0] != 'n') continue;
                        double* r9 = (double*)s->M[m];
                        fprintf(f, "W %ld", p);
                        for (long c = 0; c < 9; c++) fprintf(f, " %g", r9[c]);
                        fprintf(f, "\n");
                    }
                    for (long i = 0; i < s->nA; i++) {
                        if (!s->A[i]) continue;
                        if (idx < 1024) { lin[idx] = s->A[i]; folto[idx] = p; idx++; }
                        else cort++;
                    }
                }
                fclose(f);
                if (cort) fprintf(stderr,
                    "ser: arbol de mas de 1024 nodos: %ld quedaron sin guardar\n",
                    cort);
            }
        } else if (o->k == 5) {                  // accion del editor
            if (o->tx) { if (o->tx[0] == 'G') ordP(4, 0, 0,0,0,0,0,0); }
        } else if (o->k == 7) {                  // z: 0 al fondo, 1/2 un paso
            Ser* s = o->t; Ser* f = s ? s->fol : 0;
            long k = -1;
            for (long i = 0; f && i < f->nA; i++) if (f->A[i] == s) { k = i; break; }
            if (k >= 0) {
                if (o->r[0] == 0 && k > 0) {
                    for (long i = k; i > 0; i--) f->A[i] = f->A[i - 1];
                    f->A[0] = s; gGen++;
                } else if (o->r[0] == 1 && k < f->nA - 1) {
                    f->A[k] = f->A[k + 1]; f->A[k + 1] = s; gGen++;
                } else if (o->r[0] == 2 && k > 0) {
                    f->A[k] = f->A[k - 1]; f->A[k - 1] = s; gGen++;
                }
            }
        } else if (o->k == 6) {                  // alimenta camara 'm'
            if (o->t && o->tx && o->r[0] > 0 && o->r[1] > 0) {
                long long* c4 = camRow(o->t, o->r[0], o->r[1]);
                for (long vi = 0; vi < 4; vi++) {    // embudo: sin leer sucio
                    long fv = gCamF;
                    memcpy((void*)c4[0], o->tx,
                           (size_t)(o->r[0] * o->r[1]) * sizeof(unsigned int));
                    if (gCamF == fv) break;      // el par no escribio mientras
                }
                c4[3]++;
                o->t->vr++; gGen++;
            }
        } else if (o->k == 8) {                  // camara global: mueve la vista
            gCamX += o->r[0];
            gCamY += o->r[1];
            gCamVer++;
        }
        oTail = oTail + 1;
    }
    for (long z = 0; z < 256; z++) oRing[z].tx = 0;   // tras drenar todo
}
// ---- [I] interaccion (mis para todos) ---------------------------------------
static Ser* gSel = 0; static Ser* gAnt = 0;   // seleccion actual y previa
static Ser* gArra = 0;               // nodo arrastrado
static float gDOffX, gDOffY;          // anclaje del arrastre
static int   gSlid = 0;               // arrastrando un slider

static long esDe(Ser* ap, Ser* t) {      // ap vive dentro de la rama t
    for (; ap; ap = ap->fol) if (ap == t) return 1;
    return 0;
}
static void borraApuntados(Ser* t) {     // ser colgado en la rama borrada
    Ser** ap[16] = { &gObj, &gSel, &gAnt, &gInsp, &gSes, &gForm, &gCamNodo,
                     &gColNodo, &gInspEd, &gInspVis, &gInspPn, &gInspR,
                     &gInspG, &gInspB, &gInspRad, &gAyTxt };
    for (long i = 0; i < 16; i++) if (esDe(*ap[i], t)) *ap[i] = 0;
    if (!gObj) gEdito = 0;
}

static void inspAplica(Ser* sl) {        // slider ligado -> objetivo elegido
    if (!gObj || !sl) return;            // cualquier slider con CI_EXT actua igual
    long ch = ccG(sl, CI_EXT);
    long vr = ccG(sl, CI_VAL);
    if (ch == CI_RED) {
        lgE(gObj, CI_RED, ccG(gObj, CI_RED), 0);
        ccS(gObj, CI_RED, vr * 30 / 1000);
    } else if (ch >= CI_R && ch <= CI_B) {
        lgE(gObj, ch, ccG(gObj, ch), 0);
        ccS(gObj, ch, vr * QZ / 1000);
    }
}

static void syncAyuda() {                // barra fija: foco y teclado vistos
    if (!gAyTxt || !gAyTxt->lb || (gEdito && gObj == gAyTxt)) return;
    static char sc[192];
    char* nom = (gObj && gObj->lb && gObj->lb[0]) ? gObj->lb : (char*)"-";
    snprintf(sc, sizeof(sc), "SEL %s [%c]%s%s",
        nom, gObj ? gObj->tc : '?',
        (gObj && !(ccG(gObj, CI_FLG) & 1)) ? " OCULTO" : "",
        (gObj && (ccG(gObj, CI_FLG) & 8)) ? " PIN" : "");
    if (strcmp(sc, gAyTxt->lb) == 0) return;
    snprintf(gAyTxt->lb, 195, "%s", sc);
    gAyTxt->vr++; gGen++;
}
static void syncInsp() {                 // inspector refleja al objetivo
    if (!gObj || !gInsp) return;
    ccS(gObj, CI_SEL, 1);
    if (gInspR) ccS(gInspR, CI_VAL, ccG(gObj, CI_R) * 1000 / QZ);
    if (gInspG) ccS(gInspG, CI_VAL, ccG(gObj, CI_G) * 1000 / QZ);
    if (gInspB) ccS(gInspB, CI_VAL, ccG(gObj, CI_B) * 1000 / QZ);
    if (gInspRad) ccS(gInspRad, CI_VAL, ccG(gObj, CI_RED) * 1000 / 30);
    if (gInspVis) {
        long fl = ccG(gInspVis, CI_FLG);
        ccS(gInspVis, CI_FLG, (ccG(gObj, CI_FLG) & 1) ? (fl | 2) : (fl & ~2));
    }
    if (gInspPn) {
        long fl = ccG(gInspPn, CI_FLG);
        ccS(gInspPn, CI_FLG, (ccG(gObj, CI_FLG) & 8) ? (fl | 2) : (fl & ~2));
    }
}

static __attribute__((unused)) void pisaRaton(float mx, float my) {
    if (g3D) {                           // 3D: clic elige el tablero
        lgEmp();
        Ser* s = elige3D(mx, my);
        if (s) {
            gAnt = gSel;
            gSel = s;
            if (gObj && gObj != s) ccS(gObj, CI_SEL, 0);
            gObj = s;
            syncInsp();
        }
        return;
    }
    Ser* s = elige(mx, my);
    if (!s) return;
    ratonMundo(s, mx, my, &mx, &my);         // pin: pantalla / resto: mundo
    lgEmp();                                 // un clic = un grupo de deshacer
    gAnt = gSel;
    gSel = s;
    long enI = 0;
    for (Ser* p = s; p; p = p->fol) if (p == gInsp) { enI = 1; break; }
    if (!enI) {                              // fuera del inspector: nuevo objetivo
        if (gObj && gObj != s) ccS(gObj, CI_SEL, 0);
        gObj = s;
        syncInsp();
    }
    if (!(ccG(s, CI_FLG) & 8)) {            // pin: sin frente ni z propio
        mandaAlFrente(s);
        ordP(3, s, 0, 0, 0, 0, 0, 0);
    }
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
    } else if (!(ccG(s, CI_FLG) & 8)) {     // pin: inamovible
        gArra = s;
        gDOffX = mx - ox - (float)ccG(s, CI_X) / QZ;
        gDOffY = my - oy - (float)ccG(s, CI_Y) / QZ;
    }
    if (s->tc == 'b') ccS(s, CI_EXT, 1);
    if (s->tc == 'k') {
        lgE(s, CI_FLG, ccG(s, CI_FLG), 0);
        ccS(s, CI_FLG, ccG(s, CI_FLG) ^ 2);
    }
    if (s == gInspVis && gObj) {           // inspector escribe al objetivo
        lgE(gObj, CI_FLG, ccG(gObj, CI_FLG), 0);
        ccS(gObj, CI_FLG, (ccG(s, CI_FLG) & 2) ? (ccG(gObj, CI_FLG) | 1)
                                               : (ccG(gObj, CI_FLG) & ~1));
    }
    if (s == gInspPn && gObj) {
        lgE(gObj, CI_FLG, ccG(gObj, CI_FLG), 0);
        ccS(gObj, CI_FLG, (ccG(s, CI_FLG) & 2) ? (ccG(gObj, CI_FLG) | 8)
                                               : (ccG(gObj, CI_FLG) & ~8));
    }
    if (s == gInspEd) gEdito = (ccG(s, CI_FLG) & 2) ? 1 : 0;
    if (s->tc == 'p') {
        lgE(s, CI_ABI, ccG(s, CI_ABI), 0);
        ccS(s, CI_ABI, ccG(s, CI_ABI) ^ 1);
    }
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
    if (gEdito && gObj) {                    // edicion de etiqueta
        lgEmp();                             // tecla = grupo propio
        long n = 0; if (gObj->lb) while (gObj->lb[n]) n++;
        if (k == 8 && n > 0) {
            lgE(gObj, -2, 0, gObj->lb ? xSt(gObj->lb) : 0);
            gObj->lb[n - 1] = 0; gObj->vr++; gGen++;
        }
        else if (k >= 32 && k < 127 && n < 60) {
            if (k >= 'a' && k <= 'z') k = (char)(k - 32);   // glifos 3x5
            lgE(gObj, -2, 0, gObj->lb ? xSt(gObj->lb) : 0);
            char* nv = (char*)xM((unsigned long)(n + 2));
            for (long i = 0; i <= n; i++) nv[i] = gObj->lb[i];
            xMe(gObj->lb);
            nv[n] = k; nv[n + 1] = 0;
            gObj->lb = nv;
            gObj->vr++; gGen++;
        }
        return;
    }
    if (k == 'L') {                          // modo etiqueta
        gEdito = !gEdito;
        if (gInspEd && gEdito == ((ccG(gInspEd, CI_FLG) & 2) ? 0 : 1))
            ccS(gInspEd, CI_FLG, ccG(gInspEd, CI_FLG) ^ 2);
    }
    if (k == 'G') { g3D = !g3D; gImgGen = -1; return; }   // G: modo 3D
    if (g3D) {
        if (k == '+') { g3Zm *= 1.15f; if (g3Zm > 3.0f) g3Zm = 3.0f; return; }
        if (k == '-') { g3Zm *= 0.87f; if (g3Zm < 0.3f) g3Zm = 0.3f; return; }
    }
    if (k == 'U') { deshacer(); return; }          // deshacer: ultimo grupo
    if (k == 'C') ordP(8, 0, -gCamX, -gCamY, 0, 0, 0, 0);   // camara a origen
    lgEmp();                                 // una tecla = un grupo
    if (gObj) {
        long flO = ccG(gObj, CI_FLG);
        if (k == 'V') { lgE(gObj, CI_FLG, flO, 0); ccS(gObj, CI_FLG, flO ^ 1); }
        if (k == 'P') { lgE(gObj, CI_FLG, flO, 0); ccS(gObj, CI_FLG, flO ^ 8); }
        if (k == 'F') ordP(3, gObj, 0, 0, 0, 0, 0, 0);
        if (k == 'B') ordP(7, gObj, 0, 0, 0, 0, 0, 0);
        if (k == 'Z') ordP(7, gObj, 1, 0, 0, 0, 0, 0);
        if (k == 'X') ordP(7, gObj, 2, 0, 0, 0, 0, 0);
        if (k == 'D') {
            ordP(2, gObj, 0, 0, 0, 0, 0, 0);
            if (gObj->fol == gInsp) gObj = 0;
        }
        exRows(gObj);
    }
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
static long leeNumV(const char* l, long idx, float* out) {
    long k = 0, w = 0;
    while (w < idx) {
        while (l[k] == ' ') k++;
        while (l[k] && l[k] != ' ') k++;
        w++;
    }
    while (l[k] == ' ') k++;
    if (!(l[k] >= '0' && l[k] <= '9') && l[k] != '-' && l[k] != '.') return 0;
    return leeNum(l, idx, out);
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
    Ser** prev = (Ser**)xM(1024 * sizeof(Ser*));
    for (long i = 0; i < 1024; i++) prev[i] = 0;
    long np = 0;
    while (fgets(lin, sizeof(lin), f)) {
        if (lin[0] == 'N') {
            float fol = 0, x = 0, y = 0, w = 0, h = 0;
            float r = 0, g = 0, b = 0, a = 0, ab = 0, fl = 0, vv = 0;
            char nomb[256], tct[16];
            campoTxt(lin, 3, tct, sizeof(tct));    // tc es letra: no pasa por leeNum
            char tcc = tct[0];
            if (tcc < 'A' || tcc > 'z') tcc = 0;   // archivo sin tc reaparece muerto? no: neutro
            leeNum(lin, 2, &fol);
            long nuevo = leeNumV(lin, 4, &x);
            if (nuevo) {
                leeNum(lin, 5, &y);
                leeNum(lin, 6, &w);  leeNum(lin, 7, &h);
                leeNum(lin, 8, &r);  leeNum(lin, 9, &g);
                leeNum(lin, 10, &b); leeNum(lin, 11, &a);
                leeNum(lin, 12, &ab); leeNum(lin, 13, &fl);
                leeNum(lin, 14, &vv);                   // CI_VAL persistido
                campoTxt(lin, 15, nomb, sizeof(nomb));  // etiqueta al final: con espacios
            } else {
                campoTxt(lin, 4, nomb, sizeof(nomb));   // formato viejo: etiqueta en campo 4
                for (long k2 = 0; nomb[k2]; k2++)       // era un solo token: corto en espacio
                    if (nomb[k2] == ' ') { nomb[k2] = 0; break; }
                leeNum(lin, 5, &x);  leeNum(lin, 6, &y);
                leeNum(lin, 7, &w);  leeNum(lin, 8, &h);
                leeNum(lin, 9, &r);  leeNum(lin, 10, &g);
                leeNum(lin, 11, &b); leeNum(lin, 12, &a);
                leeNum(lin, 13, &ab); leeNum(lin, 14, &fl);
                leeNum(lin, 15, &vv);                   // val al final en la ultima vieja
            }
            Ser* s;
            if (fol < 0) {                       // N 0 comandos fol -1: la raiz
                s = gRoot;                       // ya existe: no duplicar
                s->tc = tcc ? tcc : s->tc;
                if (s->lb) { xMe(s->lb); }
                s->lb = xSt(nomb);
            } else {
                Ser* folp = (fol < np) ? prev[(long)fol] : 0;
                s = serCr(folp ? folp : gRoot, tcc, "nd", nomb);
            }
            ccS(s, CI_X, (long)x); ccS(s, CI_Y, (long)y);
            ccS(s, CI_W, (long)w); ccS(s, CI_H, (long)h);   // el archivo ya es punto fijo: sin *QZ
            ccS(s, CI_R, (long)r); ccS(s, CI_G, (long)g);
            ccS(s, CI_B, (long)b); ccS(s, CI_A, (long)a);
            ccS(s, CI_ABI, (long)ab); ccS(s, CI_FLG, (long)fl);
            ccS(s, CI_RED, 6);
            ccS(s, CI_VAL, (long)vv);
            if (np < 1024) prev[np++] = s;
        } else if (lin[0] == 'V') {
            float idx = 0, c0 = 0, c1 = 0, c2 = 0, c3 = 0, c4 = 0, c5 = 0;
            leeNum(lin, 1, &idx); leeNum(lin, 2, &c0); leeNum(lin, 3, &c1);
            leeNum(lin, 4, &c2);  leeNum(lin, 5, &c3); leeNum(lin, 6, &c4);
            leeNum(lin, 7, &c5);
            if (idx >= 0 && idx < np) 
                f6Add(prev[(long)idx], c0, c1, c2, c3, c4, c5);
        } else if (lin[0] == 'W') {              // contador n9 persistido
            float idx = 0;
            double n[9];
            leeNum(lin, 1, &idx);
            for (long c = 0; c < 9; c++) { float nv = 0; leeNum(lin, 2 + c, &nv); n[c] = nv; }
            if (idx >= 0 && idx < np)
                n9Add(prev[(long)idx], n[0], n[1], n[2], n[3], n[4], n[5],
                      n[6], n[7], n[8]);
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
    Ser* p = serCr(gRoot, 'p', "pA", "Demo");
    ccS(p, CI_X, 16 * QZ); ccS(p, CI_Y, 16 * QZ);
    ccS(p, CI_W, 260 * QZ); ccS(p, CI_H, 210 * QZ);
    ccS(p, CI_R, 0.14f * QZ); ccS(p, CI_G, 0.16f * QZ); ccS(p, CI_B, 0.2f * QZ);
    ccS(p, CI_A, QZ); ccS(p, CI_FLG, 1); ccS(p, CI_ABI, 1); ccS(p, CI_RED, 10);
    Ser* b = serCr(p, 'b', "bH", "Rojo");       // fila con arco al fol:
    ccS(b, CI_X, 10 * QZ); ccS(b, CI_Y, 22 * QZ);   // dst -1-K = canal K del padre
    ccS(b, CI_W, 75 * QZ); ccS(b, CI_H, 24 * QZ);
    ccS(b, CI_R, 0.7f * QZ); ccS(b, CI_G, 0.15f * QZ); ccS(b, CI_B, 0.1f * QZ);
    ccS(b, CI_A, QZ); ccS(b, CI_FLG, 1 | 4); ccS(b, CI_RED, 6);
    f6Add(b, 0, 0.0, 0.0, 0.0, -1.0 - (double)CI_R, (double)QZ);
    Ser* b2 = serCr(p, 'b', "bV", "Verde");
    ccS(b2, CI_X, 92 * QZ); ccS(b2, CI_Y, 22 * QZ);
    ccS(b2, CI_W, 75 * QZ); ccS(b2, CI_H, 24 * QZ);
    ccS(b2, CI_R, 0.1f * QZ); ccS(b2, CI_G, 0.6f * QZ); ccS(b2, CI_B, 0.15f * QZ);
    ccS(b2, CI_A, QZ); ccS(b2, CI_FLG, 1 | 4); ccS(b2, CI_RED, 6);
    f6Add(b2, 0, 0.0, 0.0, 0.0, -1.0 - (double)CI_G, (double)QZ);
    Ser* b3 = serCr(p, 'b', "bA", "Azul");
    ccS(b3, CI_X, 174 * QZ); ccS(b3, CI_Y, 22 * QZ);
    ccS(b3, CI_W, 75 * QZ); ccS(b3, CI_H, 24 * QZ);
    ccS(b3, CI_R, 0.1f * QZ); ccS(b3, CI_G, 0.25f * QZ); ccS(b3, CI_B, 0.7f * QZ);
    ccS(b3, CI_A, QZ); ccS(b3, CI_FLG, 1 | 4); ccS(b3, CI_RED, 6);
    f6Add(b3, 0, 0.0, 0.0, 0.0, -1.0 - (double)CI_B, (double)QZ);
    Ser* k = serCr(p, 'k', "kA", "Fijar");       // click: pin del panel
    f6Add(k, 0, 0.0, 0.0, 2.0, -1.0 - (double)CI_FLG, 8.0);
    ccS(k, CI_X, 10 * QZ); ccS(k, CI_Y, 54 * QZ);
    ccS(k, CI_W, 90 * QZ); ccS(k, CI_H, 20 * QZ);
    ccS(k, CI_R, 0.2f * QZ); ccS(k, CI_G, 0.3f * QZ); ccS(k, CI_B, 0.4f * QZ);
    ccS(k, CI_A, QZ); ccS(k, CI_FLG, 1 | 2); ccS(k, CI_RED, 4);
    Ser* s = serCr(p, 's', "sA", "R obj");      // al canal R del objeto ELEGIDO
    ccS(s, CI_X, 10 * QZ); ccS(s, CI_Y, 80 * QZ);
    ccS(s, CI_W, 200 * QZ); ccS(s, CI_H, 18 * QZ);
    ccS(s, CI_R, 0.9f * QZ); ccS(s, CI_G, 0.6f * QZ); ccS(s, CI_B, 0.1f * QZ);
    ccS(s, CI_A, QZ); ccS(s, CI_FLG, 1); ccS(s, CI_VAL, 400); ccS(s, CI_RED, 8);
    ccS(s, CI_EXT, CI_R);
    Ser* ay = serCr(p, 'l', "lA", "Rojo/Verde/Azul pintan panel; Fijar lo clava; R obj al nodo elegido");
    ccS(ay, CI_X, 10 * QZ); ccS(ay, CI_Y, 104 * QZ);
    ccS(ay, CI_R, 0.8f * QZ); ccS(ay, CI_G, 0.8f * QZ); ccS(ay, CI_B, 0.8f * QZ);
    ccS(ay, CI_A, QZ); ccS(ay, CI_FLG, 1);
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
    n9Add(cb, (double)CI_VAL, 12.0, 0.0, 256.0, 0.0, 0.0, 0.0, 1.0, 1.0);
                                  // contadores: pulsa con topes y reset
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
    gForm = serCr(gRoot, 'p', "pF", "Gen");   // meta-UI: forma que se forma
    ccS(gForm, CI_X, 570 * QZ); ccS(gForm, CI_Y, 230 * QZ);
    ccS(gForm, CI_W, 200 * QZ); ccS(gForm, CI_H, 120 * QZ);
    ccS(gForm, CI_R, 0.16f * QZ); ccS(gForm, CI_G, 0.22f * QZ); ccS(gForm, CI_B, 0.3f * QZ);
    ccS(gForm, CI_A, QZ); ccS(gForm, CI_FLG, 1); ccS(gForm, CI_ABI, 0); ccS(gForm, CI_RED, 8);
    f6Add(gForm, 3, (double)CI_ABI, 1.0, 5.0, (double)'b', 3.0);   // abrir -> 1 boton
    f6Add(gForm, 3, (double)CI_ABI, 1.0, 5.0, (double)'k', 3.0);   // + 1 check
    f6Add(gForm, 3, (double)CI_ABI, 1.0, 5.0, (double)'s', 3.0);   // + 1 slider
    Ser* pm = serCr(gRoot, 'p', "pM", "Cam");     // bezel fijado (primer plano)
    ccS(pm, CI_X, 640 * QZ); ccS(pm, CI_Y, 20 * QZ);
    ccS(pm, CI_W, 150 * QZ); ccS(pm, CI_H, 110 * QZ);
    ccS(pm, CI_R, 0.1f * QZ); ccS(pm, CI_G, 0.12f * QZ); ccS(pm, CI_B, 0.16f * QZ);
    ccS(pm, CI_A, QZ); ccS(pm, CI_FLG, 1 | 8); ccS(pm, CI_ABI, 1); ccS(pm, CI_RED, 8);
    Ser* cmi = serCr(pm, 'm', "mC", "Cam");       // camara viva (fila 'x')
    ccS(cmi, CI_X, 6 * QZ); ccS(cmi, CI_Y, 18 * QZ);
    ccS(cmi, CI_W, 138 * QZ); ccS(cmi, CI_H, 86 * QZ);
    ccS(cmi, CI_A, QZ); ccS(cmi, CI_FLG, 1);
    gCamNodo = cmi;
    Ser* pc2 = serCr(gRoot, 'p', "pC2", "Colado");  // tercer proceso: lo colado
    ccS(pc2, CI_X, 640 * QZ); ccS(pc2, CI_Y, 140 * QZ);
    ccS(pc2, CI_W, 150 * QZ); ccS(pc2, CI_H, 110 * QZ);
    ccS(pc2, CI_R, 0.1f * QZ); ccS(pc2, CI_G, 0.12f * QZ); ccS(pc2, CI_B, 0.16f * QZ);
    ccS(pc2, CI_A, QZ); ccS(pc2, CI_FLG, 1 | 8); ccS(pc2, CI_ABI, 1); ccS(pc2, CI_RED, 8);
    Ser* mCol = serCr(pc2, 'm', "mC2", "Col");
    ccS(mCol, CI_X, 6 * QZ); ccS(mCol, CI_Y, 18 * QZ);
    ccS(mCol, CI_W, 138 * QZ); ccS(mCol, CI_H, 86 * QZ);
    ccS(mCol, CI_A, QZ); ccS(mCol, CI_FLG, 1);
    gColNodo = mCol;
    gInsp = serCr(gRoot, 'p', "iN", "Inspect");   // inspector fijado
    ccS(gInsp, CI_X, 16 * QZ); ccS(gInsp, CI_Y, 244 * QZ);
    ccS(gInsp, CI_W, 260 * QZ); ccS(gInsp, CI_H, 116 * QZ);
    ccS(gInsp, CI_R, 0.13f * QZ); ccS(gInsp, CI_G, 0.11f * QZ);
    ccS(gInsp, CI_B, 0.19f * QZ);
    ccS(gInsp, CI_A, QZ); ccS(gInsp, CI_FLG, 1 | 8); ccS(gInsp, CI_ABI, 1);
    ccS(gInsp, CI_RED, 8);
    gInspEd = serCr(gInsp, 'k', "eD", "Ed");
    ccS(gInspEd, CI_X, 8 * QZ); ccS(gInspEd, CI_Y, 20 * QZ);
    ccS(gInspEd, CI_W, 36 * QZ); ccS(gInspEd, CI_H, 18 * QZ);
    ccS(gInspEd, CI_R, 0.3f * QZ); ccS(gInspEd, CI_G, 0.3f * QZ);
    ccS(gInspEd, CI_B, 0.4f * QZ);
    ccS(gInspEd, CI_A, QZ); ccS(gInspEd, CI_FLG, 1); ccS(gInspEd, CI_RED, 4);
    gInspVis = serCr(gInsp, 'k', "vI", "Vi");
    ccS(gInspVis, CI_X, 52 * QZ); ccS(gInspVis, CI_Y, 20 * QZ);
    ccS(gInspVis, CI_W, 36 * QZ); ccS(gInspVis, CI_H, 18 * QZ);
    ccS(gInspVis, CI_R, 0.3f * QZ); ccS(gInspVis, CI_G, 0.3f * QZ);
    ccS(gInspVis, CI_B, 0.4f * QZ);
    ccS(gInspVis, CI_A, QZ); ccS(gInspVis, CI_FLG, 1); ccS(gInspVis, CI_RED, 4);
    gInspPn = serCr(gInsp, 'k', "pI2", "Pn");
    ccS(gInspPn, CI_X, 96 * QZ); ccS(gInspPn, CI_Y, 20 * QZ);
    ccS(gInspPn, CI_W, 36 * QZ); ccS(gInspPn, CI_H, 18 * QZ);
    ccS(gInspPn, CI_R, 0.3f * QZ); ccS(gInspPn, CI_G, 0.3f * QZ);
    ccS(gInspPn, CI_B, 0.4f * QZ);
    ccS(gInspPn, CI_A, QZ); ccS(gInspPn, CI_FLG, 1); ccS(gInspPn, CI_RED, 4);
    gInspRad = serCr(gInsp, 's', "aI2", "Rad");
    ccS(gInspRad, CI_X, 140 * QZ); ccS(gInspRad, CI_Y, 21 * QZ);
    ccS(gInspRad, CI_W, 110 * QZ); ccS(gInspRad, CI_H, 16 * QZ);
    ccS(gInspRad, CI_R, 0.6f * QZ); ccS(gInspRad, CI_G, 0.4f * QZ);
    ccS(gInspRad, CI_B, 0.1f * QZ);
    ccS(gInspRad, CI_A, QZ); ccS(gInspRad, CI_FLG, 1|4);
    ccS(gInspRad, CI_EXT, CI_RED); ccS(gInspRad, CI_RED, 6);
    gInspR = serCr(gInsp, 's', "rI", "R");
    ccS(gInspR, CI_X, 8 * QZ); ccS(gInspR, CI_Y, 46 * QZ);
    ccS(gInspR, CI_W, 110 * QZ); ccS(gInspR, CI_H, 16 * QZ);
    ccS(gInspR, CI_R, 0.8f * QZ); ccS(gInspR, CI_G, 0.2f * QZ);
    ccS(gInspR, CI_B, 0.2f * QZ);
    ccS(gInspR, CI_A, QZ); ccS(gInspR, CI_FLG, 1|4);
    ccS(gInspR, CI_EXT, CI_R); ccS(gInspR, CI_RED, 6);
    gInspG = serCr(gInsp, 's', "gI", "G");
    ccS(gInspG, CI_X, 8 * QZ); ccS(gInspG, CI_Y, 68 * QZ);
    ccS(gInspG, CI_W, 110 * QZ); ccS(gInspG, CI_H, 16 * QZ);
    ccS(gInspG, CI_R, 0.2f * QZ); ccS(gInspG, CI_G, 0.8f * QZ);
    ccS(gInspG, CI_B, 0.2f * QZ);
    ccS(gInspG, CI_A, QZ); ccS(gInspG, CI_FLG, 1|4);
    ccS(gInspG, CI_EXT, CI_G); ccS(gInspG, CI_RED, 6);
    gInspB = serCr(gInsp, 's', "bI", "B");
    ccS(gInspB, CI_X, 8 * QZ); ccS(gInspB, CI_Y, 90 * QZ);
    ccS(gInspB, CI_W, 110 * QZ); ccS(gInspB, CI_H, 16 * QZ);
    ccS(gInspB, CI_R, 0.2f * QZ); ccS(gInspB, CI_G, 0.3f * QZ);
    ccS(gInspB, CI_B, 0.9f * QZ);
    ccS(gInspB, CI_A, QZ); ccS(gInspB, CI_FLG, 1|4);
    ccS(gInspB, CI_EXT, CI_B); ccS(gInspB, CI_RED, 6);
    Ser* im = serCr(gInsp, 'l', "mI", "L: edita  V/P Z/X B/F");
    ccS(im, CI_X, 140 * QZ); ccS(im, CI_Y, 48 * QZ);
    ccS(im, CI_R, QZ); ccS(im, CI_G, QZ); ccS(im, CI_B, QZ); ccS(im, CI_A, QZ);
    ccS(im, CI_FLG, 1);
    Ser* pA = serCr(gRoot, 'p', "pY", "Ayuda");   // barra fijada: teclado visto
    ccS(pA, CI_X, 16 * QZ); ccS(pA, CI_Y, 486 * QZ);
    ccS(pA, CI_W, 660 * QZ); ccS(pA, CI_H, 32 * QZ);
    ccS(pA, CI_R, 0.1f * QZ); ccS(pA, CI_G, 0.13f * QZ); ccS(pA, CI_B, 0.18f * QZ);
    ccS(pA, CI_A, QZ); ccS(pA, CI_FLG, 1 | 8); ccS(pA, CI_ABI, 1); ccS(pA, CI_RED, 6);
    gAyTxt = serCr(pA, 'l', "yT", "");            // estado del foco (vivo)
    xMe(gAyTxt->lb);
    gAyTxt->lb = (char*)xM(200); gAyTxt->lb[0] = 0;
    ccS(gAyTxt, CI_X, 6 * QZ); ccS(gAyTxt, CI_Y, 4 * QZ);
    ccS(gAyTxt, CI_R, QZ); ccS(gAyTxt, CI_G, 0.75f * QZ); ccS(gAyTxt, CI_B, 0.15f * QZ);
    ccS(gAyTxt, CI_A, QZ); ccS(gAyTxt, CI_FLG, 1);
    Ser* ay2 = serCr(pA, 'l', "yK",
        "FLEC:camara  C:camara 0  U:deshacer todo  L:etiqueta  V:visible  "
        "P:pin  D:borrar(vuelve con U)  G:3D  F/B/Z/X:orden z");
    ccS(ay2, CI_X, 6 * QZ); ccS(ay2, CI_Y, 19 * QZ);
    ccS(ay2, CI_R, 0.9f * QZ); ccS(ay2, CI_G, 0.9f * QZ); ccS(ay2, CI_B, 0.95f * QZ);
    ccS(ay2, CI_A, QZ); ccS(ay2, CI_FLG, 1);
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
        printf("fila SM pinta al panel (arco al fol): %s (R=%ld)\n",
               ccG(b->fol, CI_R) == QZ ? "PASA" : "FALLA", ccG(b->fol, CI_R));
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
    Ser* gf = gForm;
    if (gf && gf->tc == 'p') {
        long n0 = gf->nA;
        ccS(gf, CI_ABI, 1); exRows(gf); escritor();
        long n1 = gf->nA;
        printf("meta-UI despliegue en cascada: %s (%ld -> %ld hijos)\n",
               n1 == n0 + 3 ? "PASA" : "FALLA", n0, n1);
        ccS(gf, CI_ABI, 0); exRows(gf); escritor();
        ccS(gf, CI_ABI, 1); exRows(gf); escritor();
        printf("meta-UI cascada sin duplicados: %s (%ld hijos)\n",
               gf->nA == n1 ? "PASA" : "FALLA", gf->nA);
        long tipos = 0;
        for (long i = 0; i < gf->nA; i++) {
            if (!gf->A[i]) continue;
            if (gf->A[i]->tc == 'b' || gf->A[i]->tc == 'k'
                || gf->A[i]->tc == 's') tipos++;
        }
        printf("meta-UI tipos creados (b,k,s): %s\n", tipos == 3 ? "PASA" : "FALLA");
        ordP(4, 0, 0, 0, 0, 0, 0, 0);       // re-volcado con los hijos meta
        escritor();
    }
    Ser* pk = serCr(gRoot, 'k', "pk", "pin");   // pin: gana pick estando debajo
    ccS(pk, CI_X, 8 * QZ); ccS(pk, CI_Y, 8 * QZ);
    ccS(pk, CI_W, 60 * QZ); ccS(pk, CI_H, 18 * QZ);
    ccS(pk, CI_FLG, 1 | 8); ccS(pk, CI_RED, 4);
    Ser* nb = serCr(gRoot, 'b', "nb", "nor");   // normal creado despues (lo tapa)
    ccS(nb, CI_X, 8 * QZ); ccS(nb, CI_Y, 8 * QZ);
    ccS(nb, CI_W, 60 * QZ); ccS(nb, CI_H, 18 * QZ);
    ccS(nb, CI_FLG, 1); ccS(nb, CI_RED, 6);
    gLinN = 0; listaRec(gRoot);
    Ser* pp = elige(20, 12);
    printf("pin gana el pick: %s\n", pp == pk ? "PASA" : "FALLA");
    pisaRaton(20, 12);
    printf("pin inamovible sin frente: %s\n",
           (gArra == 0 && gRoot->A[gRoot->nA - 1] == nb) ? "PASA" : "FALLA");
    syncAyuda();                             // barra de estado sigue al foco
    printf("ayuda refleja foco: %s (lb=%s)\n",
           (gAyTxt && gAyTxt->lb && strncmp(gAyTxt->lb, "SEL ", 4) == 0
            && strstr(gAyTxt->lb, "PIN")) ? "PASA" : "FALLA",
           gAyTxt && gAyTxt->lb ? gAyTxt->lb : "-");
    gObj = 0; ccS(pk, CI_SEL, 0); syncAyuda();
    Ser* cam = serCr(gRoot, 'm', "cT", "Cam");   // camara de prueba (zona libre:
    ccS(cam, CI_X, 40 * QZ); ccS(cam, CI_Y, 460 * QZ);   // el inspector pin tapa)
    ccS(cam, CI_W, 40 * QZ); ccS(cam, CI_H, 20 * QZ);
    ccS(cam, CI_FLG, 1);
    unsigned int fr[8];                        // frame 4x2: azul arriba rojo abajo
    for (long i = 0; i < 4; i++) fr[i] = 0xff0000ffu;
    for (long i = 4; i < 8; i++) fr[i] = 0xffff0000u;
    ordFeed(cam, fr, 4, 2); escritor();
    pintaTodo();
    unsigned int cv = gImg[470 * gW + 60];
    printf("camara muestra frame: %s\n",
           ((cv >> 16) & 255) == 255 ? "PASA" : "FALLA");
    gRq = 0; pintaTodo();
    printf("cache camara sin cambios: %s\n", gRq == 0 ? "PASA" : "FALLA");
    for (long i = 0; i < 8; i++) fr[i] = 0xff00ff00u;   // todo verde
    ordFeed(cam, fr, 4, 2); escritor(); pintaTodo();
    unsigned int cv2 = gImg[470 * gW + 60];
    printf("camara fuente viva: %s\n",
           ((cv2 >> 8) & 255) == 255 && !((cv2 >> 16) & 255) ? "PASA" : "FALLA");
    Ser* z1 = serCr(gRoot, 'b', "z1", "z1");
    Ser* z2 = serCr(gRoot, 'b', "z2", "z2");
    Ser* z3 = serCr(gRoot, 'b', "z3", "z3");
    long nz = gRoot->nA;
    long iz1 = -1, iz2 = -1, iz3 = -1;       //.indices reales: A[] puede tener
    for (long i = 0; i < nz; i++) {           //huecos reciclados, no contiguos
        if (gRoot->A[i] == z1) iz1 = i;
        else if (gRoot->A[i] == z2) iz2 = i;
        else if (gRoot->A[i] == z3) iz3 = i;
    }
    ordP(7, z3, 0, 0, 0, 0, 0, 0); escritor();   // z3 al fondo
    printf("z: orden inicial (A[]): %s (%ld,%ld,%ld de %ld)\n",
           (iz1 >= 0 && iz2 > iz1 && iz3 > iz2) ? "PASA" : "FALLA",
           iz1, iz2, iz3, nz);
    ordP(7, z3, 0, 0, 0, 0, 0, 0); escritor();   // z3 al fondo (indice 0)
    printf("z: al fondo: %s\n",
           (gRoot->A[0] == z3) ? "PASA" : "FALLA");
    ordP(7, z3, 1, 0, 0, 0, 0, 0); escritor();   // un paso adelante
    printf("z: un paso adelante: %s\n",
           (gRoot->A[1] == z3) ? "PASA" : "FALLA");
    ordP(7, z3, 2, 0, 0, 0, 0, 0); escritor();   // un paso atras
    printf("z: un paso atras: %s\n",
           (gRoot->A[0] == z3) ? "PASA" : "FALLA");
    if (gInsp && gInspR) {                       // inspector: objetivo y teclas
        Ser* tb = serCr(gRoot, 'b', "tI", "Tgt");
        ccS(tb, CI_X, 700 * QZ); ccS(tb, CI_Y, 460 * QZ);
        ccS(tb, CI_W, 80 * QZ); ccS(tb, CI_H, 24 * QZ);
        ccS(tb, CI_R, QZ / 2); ccS(tb, CI_FLG, 1); ccS(tb, CI_RED, 8);
        gObj = tb; syncInsp();
        printf("inspector sigue al objetivo: %s (SEL=%ld R=%ld)\n",
               (ccG(tb, CI_SEL) == 1 && ccG(gInspR, CI_VAL) == 500)
               ? "PASA" : "FALLA", ccG(tb, CI_SEL), ccG(gInspR, CI_VAL));
        ccS(gInspG, CI_VAL, 500);
        inspAplica(gInspG);
        printf("inspector color por slider: %s (G=%ld)\n",
               ccG(tb, CI_G) == 128 ? "PASA" : "FALLA", ccG(tb, CI_G));
        tecla('P');
        printf("inspector pin por tecla: %s (FLG=%ld)\n",
               (ccG(tb, CI_FLG) & 8) ? "PASA" : "FALLA", ccG(tb, CI_FLG));
        pisaRaton(68, 264);                 // clic real sobre check Vi
        printf("inspector visible por check: %s (FLG=%ld)\n",
               (ccG(tb, CI_FLG) & 1) ? "FALLA" : "PASA", ccG(tb, CI_FLG));
    }
    Ser* c9 = serCr(gRoot, 's', "c9", "c9");   // contador n9 con tope y reset
    ccS(c9, CI_X, 700 * QZ); ccS(c9, CI_Y, 490 * QZ);
    ccS(c9, CI_W, 80 * QZ); ccS(c9, CI_H, 16 * QZ);
    ccS(c9, CI_FLG, 1); ccS(c9, CI_VAL, 0);
    n9Add(c9, (double)CI_VAL, 100.0, 0.0, 700.0, 0.0, 0.0, 0.0, 1.0, 1.0);
    exN9(c9); exN9(c9); exN9(c9); exN9(c9); exN9(c9);
    long v9a = ccG(c9, CI_VAL);
    for (long i = 0; i < 3; i++) exN9(c9);
    long v9b = ccG(c9, CI_VAL);
    printf("contador n9 delta y tope: %s (500 -> %ld)\n",
           v9a == 500 && v9b == 0 ? "PASA" : "FALLA", v9b);
    Ser* e1 = serCr(gRoot, 'b', "e1", "");
    ccS(e1, CI_FLG, 1); ccS(e1, CI_X, QZ); ccS(e1, CI_Y, 495 * QZ);
    gObj = e1; gEdito = 1;
    tecla('Q'); tecla('W'); tecla(8);
    printf("edicion de etiqueta: %s (lb=%s)\n",
           e1->lb && e1->lb[0] == 'Q' && e1->lb[1] == 0 ? "PASA" : "FALLA",
           e1->lb ? e1->lb : "-");
    gEdito = 0;
    deshacer();                              // repone el borrado: lb = "QW"
    printf("deshacer borrado de etiqueta: %s (lb=%s)\n",
           e1->lb && e1->lb[1] == 'W' ? "PASA" : "FALLA",
           e1->lb ? e1->lb : "-");
    deshacer();                              // repone el append W: lb = "Q"
    printf("deshacer tecla de etiqueta: %s (lb=%s)\n",
           e1->lb && e1->lb[0] == 'Q' && e1->lb[1] == 0 ? "PASA" : "FALLA",
           e1->lb ? e1->lb : "-");
    Ser* fu = serCr(gRoot, 'l', "fU", "fTx");     // parser de filas desde texto
    long nf = fTx(fu, "1 0 0 0 3 4.25\n0.5 1 0 1 3 2");
    double* fuf = (double*)fu->M[0];
    printf("parser f6 desde texto: %s (%ld filas, arg=%g)\n",
           (nf == 2 && fu->nM == 2 && fuf && fuf[0] == 1 && fuf[5] == 4.25)
           ? "PASA" : "FALLA", nf, fuf ? fuf[5] : -1.0);
    double* fu2 = fu->nM > 1 ? (double*)fu->M[1] : 0;
    printf("parser cond fraccionaria: %s (%g)\n",
           fu2 && fu2[0] == 0.5 ? "PASA" : "FALLA", fu2 ? fu2[0] : -1.0);
    long nn = fTx(fu, "3 0 0 255 0 0 0 1 1");      // 9 tokens: contador n9
    printf("parser n9 desde texto: %s (TM=%c)\n",
           (nn == 1 && fu->nM == 3 && fu->TM[2] && fu->TM[2][1] == '9')
           ? "PASA" : "FALLA", fu->TM[2] ? fu->TM[2][1] : '?');
    Ser* pz = serCr(gRoot, 'p', "pZ", "Zona");    // pick por zonas anidables
    ccS(pz, CI_X, 500 * QZ); ccS(pz, CI_Y, 440 * QZ);
    ccS(pz, CI_W, 60 * QZ); ccS(pz, CI_H, 30 * QZ);
    ccS(pz, CI_R, 0.1f * QZ); ccS(pz, CI_G, 0.2f * QZ); ccS(pz, CI_B, 0.3f * QZ);
    ccS(pz, CI_FLG, 1); ccS(pz, CI_A, QZ); ccS(pz, CI_RED, 6);
    ccS(pz, CI_ABI, 1);
    Ser* cz = serCr(pz, 'b', "bZ", "bZ");
    ccS(cz, CI_X, 10 * QZ); ccS(cz, CI_Y, 10 * QZ);
    ccS(cz, CI_W, 20 * QZ); ccS(cz, CI_H, 10 * QZ);
    ccS(cz, CI_FLG, 1); ccS(cz, CI_A, QZ); ccS(cz, CI_RED, 6);
    ccS(cz, CI_R, 0.2f * QZ); ccS(cz, CI_G, 0.5f * QZ); ccS(cz, CI_B, 0.9f * QZ);
    pintaTodo();
    printf("pick dentro de zona: %s\n", elige(515, 455) == cz ? "PASA" : "FALLA");
    ccS(cz, CI_X, 70 * QZ);                      // hijo fuera del panel
    printf("pick fuera de zona cancela rama: %s\n",
           elige(575, 455) != cz ? "PASA" : "FALLA");
    ccS(cz, CI_X, 10 * QZ);                      // de vuelta: dentro de zona
    ordP(8, 0, 100 * QZ, 0, 0, 0, 0, 0);         // camara global +100 px
    escritor(); pintaTodo();
    unsigned int cvG = gImg[455 * gW + 415];
    printf("camara global mueve la imagen: %s (B=%ld)\n",
           ((cvG & 255) > 90 && ((cvG >> 8) & 255) > 90) ? "PASA" : "FALLA",
           (long)(cvG & 255));
    printf("camara global mueve el pick: %s\n",
           elige(415, 455) == cz ? "PASA" : "FALLA");
    ordP(8, 0, -100 * QZ, 0, 0, 0, 0, 0);        // camara a origen
    escritor(); pintaTodo();
    unsigned int pv = gImg[495 * gW + 40];       // panel Ayuda fijado
    unsigned int dv = gImg[100 * gW + 100];      // panel Dmo: en el mundo
    ordP(8, 0, 60 * QZ, 40 * QZ, 0, 0, 0, 0);
    escritor(); pintaTodo();
    printf("pin quieto con camara: %s\n",
           gImg[495 * gW + 40] == pv ? "PASA" : "FALLA");
    printf("no fijado sigue a camara: %s\n",
           gImg[100 * gW + 100] != dv ? "PASA" : "FALLA");
    ordP(8, 0, -60 * QZ, -40 * QZ, 0, 0, 0, 0);  // camara a origen de nuevo
    escritor(); pintaTodo();
    parArranca();                          // par: tic cronologico + fuente cam
    long t0 = gTic;
    dormir(120);                                 // el par avanza solo
    printf("hilo par tic cronologico: %s (%ld -> %ld)\n",
           gTic > t0 ? "PASA" : "FALLA", t0, gTic);
    long long* cx = camFil(gCamNodo);
    long v0x = cx ? (long)cx[3] : 0;
    for (long vu = 0; vu < 8 && !(cx && (long)cx[3] > v0x); vu++) {
        long nvo = __sync_lock_test_and_set(&gCamNva, 0);
        if (nvo) ordFeed(gCamNodo, gBufCam, 32, 24);   // rol del impar (WinMain)
        else dormir(25);
        while (oTail != oHead) escritor();       // volcado impar: ejecuta k=6
        cx = camFil(gCamNodo);
    }
    printf("hilo par alimenta la camara: %s (ver %ld -> %ld)\n",
           (cx && (long)cx[3] > v0x) ? "PASA" : "FALLA",
           v0x, cx ? (long)cx[3] : -1);
    parParar();                            // apaga el hilo: sesiones limpias
    {                                      // cubo 3D: el par ya rasteriza
        long nz1 = 0;
        for (long i = 0; i < 768; i++) if (gBufCam[i]) nz1++;
        for (long k = 0; k < 20; k++) camFuente();
        long nz2 = 0;
        for (long i = 0; i < 768; i++) if (gBufCam[i]) nz2++;
        printf("cubo 3D girando en la camara: %s (%ld -> %ld pixeles)\n",
               (nz1 > 10 && nz1 < 768 && nz2 > 10 && nz1 != nz2) ? "PASA" : "FALLA",
               nz1, nz2);
    }
    {                                      // colador: escena -> imagen con fit
        Ser* rv = gRoot; gRoot = 0;
        Ser* rr = serCr(0, 'r', "cR", "ColadoTmp");
        gRoot = rr;
        Ser* r1 = serCr(rr, 'r', "cA", "col A");
        ccS(r1, CI_X, 20 * QZ); ccS(r1, CI_Y, 40 * QZ);
        ccS(r1, CI_W, 100 * QZ); ccS(r1, CI_H, 60 * QZ);
        ccS(r1, CI_R, QZ); ccS(r1, CI_A, QZ); ccS(r1, CI_FLG, 1);
        Ser* r2 = serCr(rr, 'r', "cB", "col B");   // encima de r1 en A[]
        ccS(r2, CI_X, 70 * QZ); ccS(r2, CI_Y, 40 * QZ);
        ccS(r2, CI_W, 100 * QZ); ccS(r2, CI_H, 60 * QZ);
        ccS(r2, CI_G, QZ); ccS(r2, CI_A, QZ); ccS(r2, CI_FLG, 1);
        Ser* m3 = serCr(rr, 'm', "m9", "colCam");   // invisible: no entra al bbox
        ccS(m3, CI_W, 30 * QZ); ccS(m3, CI_H, 20 * QZ);
        gColNodo = m3;
        colador();
        long q1 = __sync_lock_test_and_set(&gColNva, 0);
        if (q1) ordFeed(m3, gBufCol, 64, 48);    // rol del impar (WinMain)
        escritor();
        long long* cf = camFil(m3);
        float fs = 60.0f / 150.0f;               // fit: bbox 150x60 px de mundo
        long px = (long)(2.0f + (120.0f - 20.0f) * fs);   // centro de col B
        long py = (long)(2.0f + (70.0f - 40.0f) * fs);
        long pz = (long)(2.0f + (30.0f - 20.0f) * fs);    // solo col A tapa ahi
        long pz2 = (long)(2.0f + (45.0f - 40.0f) * fs);
        printf("colador fit a imagen: %s (A=%08x B=%08x ver=%ld)\n",
               (gBufCol[py * 64 + px] == pr(0, 1, 0, 1)
                && gBufCol[pz2 * 64 + pz] == pr(1, 0, 0, 1)
                && cf && (long)cf[3] == 1) ? "PASA" : "FALLA",
               gBufCol[pz2 * 64 + pz], gBufCol[py * 64 + px],
               cf ? (long)cf[3] : -1);
        gColNodo = 0;
        while (rr->nA > 0) { Ser* h2 = rr->A[rr->nA - 1]; rr->nA--; serAl(h2); }
        serAl(rr);
        gRoot = rv;
    }
    {                                      // borrar limpia apuntados: crash editar
        Ser* db = serCr(gRoot, 'b', "dB", "temp");
        gObj = db; gEdito = 1; gSel = db; gAnt = db;
        ordP(2, db, 0, 0, 0, 0, 0, 0); escritor();
        printf("borrar limpia apuntados: %s\n",
               (gObj == 0 && gSel == 0 && gAnt == 0 && gEdito == 0) ? "PASA" : "FALLA");
    }
    {                                      // deshacer creacion: la quita
        long v0 = gVivos;
        lgEmp();
        ordCr('b', gRoot); escritor();
        Ser* nb = 0;
        for (long i = gRoot->nA - 1; i >= 0 && !nb; i--)
            if (gRoot->A[i] && gRoot->A[i]->tc == 'b') nb = gRoot->A[i];
        long vino = nb ? 1 : 0;
        deshacer();
        printf("deshacer creacion quita el nodo: %s\n",
               (nb && vino && nb->tc == 0 && gVivos == v0) ? "PASA" : "FALLA");
    }
    {                                      // deshacer borrado: resucita
        Ser* dz = serCr(gRoot, 'b', "dZ", "Desh");
        long v0 = gVivos;
        lgEmp();
        ordP(2, dz, 0, 0, 0, 0, 0, 0); escritor();
        long fuera = 1;
        for (long i = 0; i < gRoot->nA; i++) if (gRoot->A[i] == dz) fuera = 0;
        long pa = (fuera && gVivos == v0 - 1 && dz->tc != 0) ? 1 : 0;
        deshacer();
        long dentro = 0;
        for (long i = 0; i < gRoot->nA; i++) if (gRoot->A[i] == dz) dentro = 1;
        printf("deshacer borrado resucita: %s (%ld vivos)\n",
               (pa && dentro && gVivos == v0 && dz->tc != 0) ? "PASA" : "FALLA",
               gVivos);
        lgEmp();                           // queda en el cementerio: sin ensuciar
        ordP(2, dz, 0, 0, 0, 0, 0, 0); escritor();
    }
    {                                      // modo 3D: placas + pick orbital
        g3D = 1; gImgGen = -1;
        pintaTodo();
        long nz = 0;
        unsigned int fnd = pr(0.05f, 0.06f, 0.09f, 1.0f);
        for (unsigned long i = 0; i < (unsigned long)gW * (unsigned long)gH; i++)
            if (gImg[i] != fnd) nz++;
        Ser* blanco = (g3N3 > 1) ? g3Se[1] : 0;
        Ser* pk = blanco ? elige3D(g3Px[1], g3Py[1]) : 0;
        printf("modo 3D pinta la escena: %s (%ld placas, %ld px)\n",
               (g3N3 > 1 && nz > 500 && pk == blanco) ? "PASA" : "FALLA",
               g3N3, nz);
        g3D = 0; gImgGen = -1;
        pintaTodo();
    }
    Ser* uk = 0;                         // check demo: por tipo, no por puesto
    for (long i = 0; gRoot && !uk && i < gRoot->nA; i++) {
        Ser* pA0 = gRoot->A[i];
        if (!pA0) continue;
        if (pA0->tc == 'k') uk = pA0;
        for (long j = 0; !uk && pA0->nA && j < pA0->nA; j++)
            if (pA0->A[j] && pA0->A[j]->tc == 'k') uk = pA0->A[j];
    }
    if (uk && uk->tc == 'k') {
        long fl0 = ccG(uk, CI_FLG);
        pisaRaton(40, 80);                       // clic real: grupo de deshacer
        long fl1 = ccG(uk, CI_FLG);
        deshacer();
        printf("deshacer clic en check: %s (%ld -> %ld)\n",
               (fl1 != fl0 && ccG(uk, CI_FLG) == fl0) ? "PASA" : "FALLA",
               fl0, ccG(uk, CI_FLG));
        long va0 = ccG(uk, CI_VAL);
        lgEmp();
        ordP(0, uk, (long)CI_VAL, 0, 0, 700, 0, 0); escritor();
        long va1 = ccG(uk, CI_VAL);
        deshacer();
        printf("deshacer asignacion: %s (%ld -> %ld -> %ld)\n",
               (va1 == 700 && ccG(uk, CI_VAL) == va0) ? "PASA" : "FALLA",
               va0, va1, ccG(uk, CI_VAL));
    }
    ordP(4, 0, 0, 0, 0, 0, 0, 0);           // re-volcado con pin y camara
    escritor();
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
    Ser* sld0 = 0;                       // slider demo: por tipo, no por puesto
    for (long i = 0; gRoot && !sld0 && i < gRoot->nA; i++) {
        Ser* pA0 = gRoot->A[i];
        if (!pA0) continue;
        if (pA0->tc == 's') sld0 = pA0;
        for (long j = 0; !sld0 && pA0->nA && j < pA0->nA; j++)
            if (pA0->A[j] && pA0->A[j]->tc == 's') sld0 = pA0->A[j];
    }
    if (sld0) {
        ccS(sld0, CI_VAL, 555);
        f6Add(sld0, 0.5, 0.0, 0.0, 0.0, (double)CI_VAL, 6.0);  // cond decimal
        ordP(4, 0, 0, 0, 0, 0, 0, 0); escritor();
        long v0 = gVivos;
        while (gRoot->nA > 0) {
            Ser* h = gRoot->A[gRoot->nA - 1];
            gRoot->nA--;
            serAl(h);
        }
        serAl(gRoot); gRoot = 0;
        gSes = 0; gForm = 0; gCamNodo = 0; gColNodo = 0; gSel = 0; gAnt = 0; gArra = 0;
        gInsp = 0; gInspEd = 0; gInspVis = 0; gInspPn = 0;
        gInspR = 0; gInspG = 0; gInspB = 0; gInspRad = 0;
        gAyTxt = 0; gObj = 0;                        // sin nodos fantasma al recargar
        long rr = desEscena(gRuta);
        Ser* sld1 = 0;                       // slider al recargar: por tipo
        for (long i = 0; gRoot && !sld1 && i < gRoot->nA; i++) {
            Ser* q0 = gRoot->A[i];
            if (!q0) continue;
            if (q0->tc == 's') sld1 = q0;
            for (long j = 0; !sld1 && q0->nA && j < q0->nA; j++)
                if (q0->A[j] && q0->A[j]->tc == 's') sld1 = q0->A[j];
        }
        printf("carga de escena completa: %s (%ld nodos, %ld vivos)\n",
               rr == v0 && sld1 ? "PASA" : "FALLA", rr, gVivos);
        printf("guardado conserva valor: %s (%ld)\n",
               sld1 && ccG(sld1, CI_VAL) == 555 ? "PASA" : "FALLA",
               sld1 ? ccG(sld1, CI_VAL) : -1);
        double* fila = (sld1 && sld1->nM > 0) ? (double*)sld1->M[0] : 0;
        printf("cond fraccionaria sobrevive: %s (%g)\n",
               fila && fila[0] == 0.5 ? "PASA" : "FALLA", fila ? fila[0] : -1.0);
    }
    ordP(4, 0, 0, 0, 0, 0, 0, 0); escritor();       // etiqueta con espacios:
    Ser* sp = serCr(gRoot, 'l', "sP", "SEL pin [k] PIN");   // antes corrompia
    ccS(sp, CI_W, 660 * QZ); ccS(sp, CI_H, 32 * QZ);
    ccS(sp, CI_X, 16 * QZ); ccS(sp, CI_Y, 486 * QZ);
    ordP(4, 0, 0, 0, 0, 0, 0, 0); escritor();
    while (gRoot->nA > 0) {
        Ser* h = gRoot->A[gRoot->nA - 1];
        gRoot->nA--;
        serAl(h);
    }
    serAl(gRoot); gRoot = 0;
    gSes = 0; gForm = 0; gCamNodo = 0; gColNodo = 0; gSel = 0; gAnt = 0; gArra = 0;
    gInsp = 0; gInspEd = 0; gInspVis = 0; gInspPn = 0;
    gInspR = 0; gInspG = 0; gInspB = 0; gInspRad = 0;
    gAyTxt = 0; gObj = 0;
    desEscena(gRuta);
    Ser* sp2 = 0;
    for (long i = 0; gRoot && i < gRoot->nA; i++) {
        Ser* q2 = gRoot->A[i];
        if (q2 && q2->lb && strcmp(q2->lb, "SEL pin [k] PIN") == 0) sp2 = q2;
    }
    printf("etiqueta con espacios sobrevive: %s (w=%ld h=%ld x=%ld)\n",
           (sp2 && ccG(sp2, CI_W) == 660 * QZ && ccG(sp2, CI_H) == 32 * QZ
            && ccG(sp2, CI_X) == 16 * QZ) ? "PASA" : "FALLA",
           sp2 ? ccG(sp2, CI_W) / QZ : -1, sp2 ? ccG(sp2, CI_H) / QZ : -1,
           sp2 ? ccG(sp2, CI_X) / QZ : -1);
    gImgGen = -1; pintaTodo();                   // el camino que crasheaba en Win
    printf("render tras recarga: %s\n", gImg && gRoot ? "PASA" : "FALLA");
    {                                            // migracion: renglon formato viejo
        FILE* v = fopen(gRuta, "wb");
        if (v) {
            fprintf(v, "N 0 -1 p Dmo 4096 4096 66560 53760 36 41 51 256 1 9 555\n");
            fclose(v);
        }
        while (gRoot->nA > 0) {
            Ser* h = gRoot->A[gRoot->nA - 1];
            gRoot->nA--;
            serAl(h);
        }
        serAl(gRoot); gRoot = 0;
        gSes = 0; gForm = 0; gCamNodo = 0; gColNodo = 0; gSel = 0; gAnt = 0; gArra = 0;
        gInsp = 0; gInspEd = 0; gInspVis = 0; gInspPn = 0;
        gInspR = 0; gInspG = 0; gInspB = 0; gInspRad = 0;
        gAyTxt = 0; gObj = 0;
        desEscena(gRuta);
        printf("migracion de formato viejo: %s (x=%ld w=%ld val=%ld lb=%s)\n",
               (gRoot && ccG(gRoot, CI_X) == 4096 && ccG(gRoot, CI_W) == 66560
                && ccG(gRoot, CI_VAL) == 555
                && gRoot->lb && strcmp(gRoot->lb, "Dmo") == 0) ? "PASA" : "FALLA",
               gRoot ? ccG(gRoot, CI_X) : -1, gRoot ? ccG(gRoot, CI_W) : -1,
               gRoot ? ccG(gRoot, CI_VAL) : -1,
               gRoot && gRoot->lb ? gRoot->lb : "(nil)");
    }
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
    gVivos += gCemN;                     // serAl vuelve a contarlos
    for (long i = 0; i < gCemN; i++) serAl(gCem[i]);   // cementerio fuera
    gCemN = 0;
    printf("seres: %ld vivos (%ld antes)\n", gVivos, vivos0);
    xMe((void*)gImg); gImg = 0;      // sin fuga al salir
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
    case WM_DESTROY:                    // autosave: la sesion sobrevive la X
        ordP(4, 0,0,0,0,0,0,0);
        escritor();
        PostQuitMessage(0); return 0;
    case WM_MOUSEMOVE: {
        if (g3D) {                       // orbita: arrastrar = girar la camara
            static long u3x = -1, u3y = -1;
            long mx2 = (long)(short)(l & 65535), my2 = (long)(short)((l >> 16) & 65535);
            if (u3x >= 0) {
                g3Az += (float)(mx2 - u3x) * 0.012f;
                g3El += (float)(my2 - u3y) * 0.012f;
                if (g3El < 0.05f) g3El = 0.05f;
                if (g3El > 1.45f) g3El = 1.45f;
                if (g3Az > 6.2832f) g3Az -= 6.2832f;
                if (g3Az < 0.0f) g3Az += 6.2832f;
            }
            u3x = mx2; u3y = my2;
            return 0; }
        if (gArra) {
            POINT p; p.x = (long)(short)(l & 65535);
            p.y = (long)(short)((l >> 16) & 65535);
            float wx = 0, wy = 0;
            ratonMundo(gSlid ? gSel : gArra, (float)p.x, (float)p.y, &wx, &wy);
            if (!gSlid) {
                long nx = (long)((wx - gDOffX) * QZ);
                long ny = (long)((wy - gDOffY) * QZ);
                ordP(0, gArra, 1, nx, ny, 3, 0, 0);
                escritor();
            } else if (gSel && gSel->tc == 's') {
                Ser* f = gSel->fol;
                float ox = 0;
                for (Ser* q = f; q; q = q->fol) ox += (float)ccG(q, CI_X) / QZ;
                float x = ox + (float)ccG(gSel, CI_X) / QZ;
                float wi = (float)ccG(gSel, CI_W) / QZ;
                float fr = x > 0 ? (wx - x) / (wi > 0 ? wi : 1) : 0;
                if (fr < 0) fr = 0;
                if (fr > 1) fr = 1;
                ccS(gSel, CI_VAL, (long)(fr * 1000));
                inspAplica(gSel);
            }
        }
        return 0; }
    case WM_LBUTTONDOWN: {
        POINT p; GetCursorPos(&p); ScreenToClient(gVen, &p);
        pisaRaton((float)p.x, (float)p.y);
        SetCapture(gVen);
        return 0; }
    case WM_LBUTTONUP: ReleaseCapture(); sueltoRaton(); return 0;
    case WM_SIZE: {                      // la ventana manda su tamano: rehacer
        long nw = (long)(l & 65535), nh = (long)((l >> 16) & 65535);
        if (nw > 40 && nh > 40 && (nw != gW || nh != gH) && gImg) {
            xMe((void*)gImg);
            gW = nw; gH = nh;
            gImg = (unsigned int*)xM((unsigned long)(gW * gH) * sizeof(unsigned int));
            unsigned int bgf = pr(0.07f, 0.08f, 0.1f, 1);
            for (unsigned long i = 0; i < (unsigned long)(gW * gH); i++) gImg[i] = bgf;
            gImgGen = -1;                // todo el frame se repinta
            if (gRoot) {
                ccS(gRoot, CI_W, gW * QZ);
                ccS(gRoot, CI_H, gH * QZ);
            }
        }
        return 0; }
    case WM_KEYDOWN:
        if (w == VK_ESCAPE) DestroyWindow(gVen);
        if (w == VK_DELETE && gSel) {
            lgEmp();                     // borrar = su grupo: U lo resucita
            ordP(2, gSel, 0,0,0,0,0,0); escritor(); return 0;
        }
        if (g3D) {                       // flechas: giran la camara 3D
            if (w == VK_LEFT)  { g3Az -= 0.15f; return 0; }
            if (w == VK_RIGHT) { g3Az += 0.15f; return 0; }
            if (w == VK_UP)    { g3El += 0.12f; if (g3El > 1.45f) g3El = 1.45f; return 0; }
            if (w == VK_DOWN)  { g3El -= 0.12f; if (g3El < 0.05f) g3El = 0.05f; return 0; }
        }
        if (w == VK_UP)    { ordP(8, 0, 0, -32 * QZ, 0, 0, 0, 0); return 0; }
        if (w == VK_DOWN)  { ordP(8, 0, 0,  32 * QZ, 0, 0, 0, 0); return 0; }
        if (w == VK_LEFT)  { ordP(8, 0, -32 * QZ, 0, 0, 0, 0, 0); return 0; }
        if (w == VK_RIGHT) { ordP(8, 0,  32 * QZ, 0, 0, 0, 0, 0); return 0; }
        if (w != 8 && (w < 32 || w > 126)) return 0;   // VK codigos: no son teclas
        tecla((char)w);
        escritor();
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
    long estilo = WS_OVERLAPPEDWINDOW;   // maximizar/resize ok: WM_SIZE rehace
    AdjustWindowRect(&rc, (LONG)estilo, 0);
    gVen = CreateWindowExA(0, "ser", "ser", (DWORD)estilo,
                           60, 60, rc.right - rc.left, rc.bottom - rc.top, 0, 0, i, 0);
    gHdc = GetDC(gVen);
    gImg = (unsigned int*)xM((unsigned long)(gW * gH) * sizeof(unsigned int));
    unsigned int bgf = pr(0.07f, 0.08f, 0.1f, 1);
    for (unsigned long i = 0; i < (unsigned long)(gW * gH); i++) gImg[i] = bgf;
    ShowWindow(gVen, 1);                 // el WM_SIZE inicial no cambia nada
    if (!desEscena(gRuta)) escModelo();
    parArranca();                        // par vigilante: tic + fuente de camara
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = 0;
    for (;;) {
        MSG m;
        while (PeekMessageA(&m, 0, 0, 0, 1)) {
            if (m.message == WM_QUIT) return 0;
            TranslateMessage(&m); DispatchMessageA(&m);
        }
        if (gCamNodo && __sync_lock_test_and_set(&gCamNva, 0))
            ordFeed(gCamNodo, gBufCam, 32, 24);   // embudo: el par aviso frame
        if ((gTic & 7) == 0) colador();           // pase del impar sobre el arbol
        if (gColNodo && __sync_lock_test_and_set(&gColNva, 0))
            ordFeed(gColNodo, gBufCol, 64, 48);   // embudo del colado
        escritor();
        tickN9(gRoot);                       // contadores vivos por cuadro
        syncAyuda();                         // barra fija sigue al foco
        pintaTodo();
        bi.bmiHeader.biWidth = (long)gW;  // resize: el encabezado sigue al buffer
        bi.bmiHeader.biHeight = (long)-gH;
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

