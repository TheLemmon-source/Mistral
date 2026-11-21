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
//  colador  : tercer paso (pase del impar cada 8 tics): escena con pintor real
//             (widgets, titulos, cache por version) fit al tamano pedido;
//             pesos volatiles gColAnti/gColImp/gColUrg (frescura via s->ulti =
//             tic del ultimo toque en ccS); base 64x48 en gBufCol y, nida
//             con camara, la camara Colado (gColNodo) pide su resolucion
//             propria; embudo gColNva en ambos.
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
//  oroga.dbg: G = contexto grafico, N = nodo, V = fila SM, W = n9,
//             P = polar/estado; carga si existe y guarda el arbol vivo.
// ============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

// ---- [D] datos junto al exe: <dir del exe>/<exe base>/ ----------------------
// Los datos del programa (layout, tipografias, exportes) viven en una carpeta
// con el nombre del exe, ramificando desde donde el exe vive. La raiz es una
// sola vez al arrancar (idempotente); todo acceso a datos pasa por fopenD/
// removeD, que agregan la raiz a cada nombre relativo: arbol de datos propio
// con ramas nuevas (subcarpetas) sin reconstruir lecturas por directorios.
static char gDirDatos[1204] = { 0 };     // con separador final; vacio = cwd

static void dirDatosArma(void) {
    if (gDirDatos[0]) return;            // ya armada: nada de rehacer
    char ex[1204];
    long n = 0;
#if defined(_WIN32)
    n = (long)GetModuleFileNameA(0, ex, (DWORD)sizeof(ex));
    if (n > 0 && n < (long)sizeof(ex)) ex[n] = 0; else n = 0;
#else
    n = (long)readlink("/proc/self/exe", ex, sizeof(ex) - 1);
    if (n > 0) ex[n] = 0; else n = 0;
#endif
    if (n <= 0) return;                  // sin ruta: cwd, todo sigue como era
    long sep = -1;
    for (long i = 0; i < n; i++)
        if (ex[i] == '/' || ex[i] == '\\') sep = i;
    if (sep < 0) return;                 // exe relativo al cwd: cwd mismo
    long dot = -1;
    for (long i = n - 1; i > sep; i--) {
        if (ex[i] == '.') { dot = i; break; }
        if (ex[i] == '/' || ex[i] == '\\') break;
    }
    long ext = dot > sep;                // extension despues del ultimo tramo
    long base1 = ext ? dot : n;
    long b = 0;
    for (long i = 0; i <= sep; i++)
        if (b + 1 < (long)sizeof(gDirDatos)) gDirDatos[b++] = ex[i];
    for (long i = sep + 1; i < base1; i++)
        if (b + 1 < (long)sizeof(gDirDatos)) gDirDatos[b++] = ex[i];
    if (!ext) {                          // sin extension: el nombre chocaria
        if (b + 3 < (long)sizeof(gDirDatos)) {   // con el propio exe
            gDirDatos[b++] = '_';
            gDirDatos[b++] = 'd';
        }
    }
    if (b + 1 < (long)sizeof(gDirDatos)) gDirDatos[b++] = ex[sep];
    gDirDatos[b] = 0;                    // separador final: listo para ramas
#if defined(_WIN32)
    CreateDirectoryA(gDirDatos, 0);      // ya existe: se ignora
#else
    mkdir(gDirDatos, 0755);
#endif
}
static FILE* fopenD(const char* nom, const char* mod) {  // dato = raiz + nom
    char p[1300];
    if (!gDirDatos[0] || nom[0] == '/' || nom[0] == '\\'
        || (nom[0] && nom[1] == ':')) return fopen(nom, mod);
    snprintf(p, sizeof(p), "%s%s", gDirDatos, nom);
    return fopen(p, mod);
}
static long removeD(const char* nom) {
    char p[1300];
    if (!gDirDatos[0] || nom[0] == '/' || nom[0] == '\\'
        || (nom[0] && nom[1] == ':')) return remove(nom) == 0;
    snprintf(p, sizeof(p), "%s%s", gDirDatos, nom);
    return remove(p) == 0;
}

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
#define CI_PON 14                      // [R] pool de reservas: cantidad de slots
#define CI_POE 15                      // [R] pool: epoch = liberaciones de slots confirmados
#define CI_POS 16                      // [R] pool: proximo slot a escanear (rotacion)
#define CI_TOP 17                      // [R] pool: C[CI_TOP+i] es el slot i
#define CI_AGL 96                      // [P] polar: angulo en grados * QZ
#define CI_RAD 97                      // [P] polar: radio en QZ (pulsos de 256)
#define CI_ARC 98                      // [P] semiarco: barrido en grados * QZ
#define CI_EST 99                      // [F] fsm: estado actual (indice del hijo)

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

// ---- [CA] contadores por sistema: telemetria atomica del sistema vivo --------
#define CA_SER   0                              // seres creados
#define CA_MUER  1                              // seres liberados
#define CA_MAT   2                              // matrices con datos pedidas
#define CA_FIL   3                              // filas SM ejecutadas
#define CA_ORD   4                              // ordenes drenadas por escritor
#define CA_RES   5                              // reservas CAS tomadas
#define CA_EXP   6                              // exportes (PPM/PNG/DOC)
#define CA_VIDA  7                              // snapshots de vida tomados
#define CA_N     8
static volatile long gCa[CA_N];                 // contadores Ca[] del sistema
static void caSuma(long i) { __sync_fetch_and_add(&gCa[i], 1); }

// ---- [V] vidas: snapshots de estado por nodo y bitacora ----------------------
#define VID_TOPE 10                            // snapshots por nodo
#define VID_N    24                            // bitacora global de nodos
static Ser* gVid[VID_N];                       // sinhrono: el mas reciente al final
static long gVidN = 0;
static long gVidSec = 0;                       // secuencia de snapshots (orden)
static void vidaOlvida(Ser* s) {         // el nodo muerto sale de la bitacora
    long n = gVidN;
    for (long i = 0; i < n; )
        if (gVid[i] == s) {
            for (long j = i + 1; j < n; j++) gVid[j - 1] = gVid[j];
            gVid[n - 1] = 0; n--;
        }
        else i++;
    gVidN = n;
}

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
    caSuma(CA_SER);
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
    caSuma(CA_MUER);
    vidaOlvida(s);                       // bitacora de vidas limpiamente
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

// ---- [R] pool de reservas CAS: slots en C[CI_TOP..CI_TOP+CI_PON) -------------
//        slot en 0 (libre), 1 (reservado) o 2 (confirmado/en uso);
//        toma, confirma y suelta son atomicas. 2 fases: quien reservo confirma
//        antes de usar; el epoch cuenta liberaciones de slots confirmados.
static void resFija(Ser* s, long n) {    // arma o redimensiona el pool
    if (n < 0) n = 0;
    if (n > 64) n = 64;                  // tope sanitario
    ccS(s, CI_PON, n);
    ccS(s, CI_POE, 0);
    ccS(s, CI_POS, 0);
    for (long i = 0; i < n; i++) ccS(s, CI_TOP + i, 0);
}
static long resToma(Ser* s) {            // 1er slot libre via CAS, escaneo rotativo
    long n = ccG(s, CI_PON);
    if (n < 1) return -1;
    long st = ccG(s, CI_POS) % n;        // arranca donde quedo la ultima toma
    for (long k = 0; k < n; k++) {
        long i = st + k; if (i >= n) i -= n;
        if (__sync_bool_compare_and_swap(&s->C[CI_TOP + i], 0, 1)) {
            ccS(s, CI_POS, (i + 1) % n); // la proxima toma escanea desde i+1
            caSuma(CA_RES);
            return i;
        }
    }
    return -1;
}
static long resConfirma(Ser* s, long i) { // reserva -> en uso via CAS (1 -> 2)
    if (i < 0 || i >= ccG(s, CI_PON)) return 0;
    return __sync_bool_compare_and_swap(&s->C[CI_TOP + i], 1, 2);
}
static long resEpoch(Ser* s) {           // liberaciones de slots confirmados
    return ccG(s, CI_POE);
}
static void resSuelta(Ser* s, long i) {  // libera slot reservado o confirmado
    if (i < 0 || i >= ccG(s, CI_PON)) return;
    if (__sync_bool_compare_and_swap(&s->C[CI_TOP + i], 2, 0))
        __sync_fetch_and_add(&s->C[CI_POE], 1);      // epoch avanza en cada liberacion
    else
        __sync_bool_compare_and_swap(&s->C[CI_TOP + i], 1, 0);
}
static long resVivos(Ser* s) {           // slots tomados ahora
    long n = ccG(s, CI_PON), v = 0;
    for (long i = 0; i < n; i++) if (ccG(s, CI_TOP + i)) v++;
    return v;
}

// ---- [P] polar: posicion efectiva = declarada + radio * (cos, sin) ----------
//        CI_RAD en QZ, CI_AGL en grados * QZ (90*QZ = 90 grados). Un solo
//        resolutor: pintar, pickear, 3D y colador preguntan ACA; radio 0
//        = como siempre (x, y absolutos), asi nada cambia de golpe y el
//        arrastre sigue moviendo el pivote declarado.
static long pRed(double v) {             // redondeo con signo
    return (long)(v + (v >= 0 ? 0.5 : -0.5));
}
static void posEf(Ser* s, long* ox, long* oy) {
    long x = ccG(s, CI_X), y = ccG(s, CI_Y);
    long rad = ccG(s, CI_RAD);
    if (rad != 0) {
        double a = (double)ccG(s, CI_AGL)
                   * (2.0 * 3.14159265358979 / (360.0 * QZ));
        x += pRed((double)rad * cos(a));
        y += pRed((double)rad * sin(a));
    }
    if (ox) *ox = x;
    if (oy) *oy = y;
}

// ---- [V] vidas: snapshots de C[] del nodo; bitacora global con tope ----------
//        una vida es una matriz "vd": [nC, secuencia, C[0..nC)); cola con tope
//        VID_TOPE por nodo; la bitacora recuerda VID_N nodos, asi H deshace
//        el ultimo efecto global (reforma segura: se snapshottea antes).
static long vidaN(Ser* s) {              // snapshots del nodo ahora
    long v = 0;
    for (long i = 0; i < s->nM; i++)
        if (s->TM[i] && s->TM[i][0] == 'v' && s->TM[i][1] == 'd') v++;
    return v;
}
static void vidaAnota(Ser* s) {          // bitacora: el nodo queda en la punta
    long n = gVidN;
    for (long i = 0; i < n; i++)         // ya anotado? se re-sube a la punta
        if (gVid[i] == s) {
            for (long j = i + 1; j < n; j++) gVid[j - 1] = gVid[j];
            n--; break;
        }
    if (n >= VID_N) {                    // cola llena: el mas viejo fuera
        for (long j = 1; j < VID_N; j++) gVid[j - 1] = gVid[j];
        gVid[VID_N - 1] = 0;
        n = VID_N - 1;
    }
    gVid[n++] = s;
    gVidN = n;
}
static long vidaToma(Ser* s) {           // snapshot del estado: push con tope
    long n = s->nC;
    double* m = (double*)xM((unsigned long)(n + 2) * sizeof(double));
    m[0] = (double)n;
    m[1] = (double)(++gVidSec);          // secuencia: orden real de creacion
    for (long i = 0; i < n; i++) m[2 + i] = (double)s->C[i];
    if (vidaN(s) >= VID_TOPE) {          // cola: el de menor secuencia sale
        double* vm = 0; long vi = -1; double vs = 0;
        for (long i = 0; i < s->nM; i++) {
            if (s->TM[i] && s->TM[i][0] == 'v' && s->TM[i][1] == 'd') {
                double* m2 = (double*)s->M[i];
                if (!vm || m2[1] < vs) { vm = m2; vi = i; vs = m2[1]; }
            }
        }
        if (vm) { xMe(s->M[vi]); s->M[vi] = 0; s->TM[vi] = 0; }
    }
    long k = -1;
    for (long i = 0; i < s->nM; i++) if (!s->M[i] || !s->TM[i]) { k = i; break; }
    if (k < 0) { k = s->nM; if (k >= s->cM) grC(s, k); s->nM = k + 1; }
    s->M[k] = m;
    s->TM[k] = (char*)"vd";
    caSuma(CA_MAT);
    caSuma(CA_VIDA);
    vidaAnota(s);
    s->vr++; gGen++;
    return vidaN(s);
}
static long vidaRest(Ser* s, long k, long pop) {   // k: 0 = mas nuevo
    long ns = vidaN(s);
    if (k < 0) k = 0;
    if (ns < 1 || k >= ns) return 0;
    double* ms[VID_TOPE]; double sd[VID_TOPE];       // orden de menor secuencia
    long q = 0;
    for (long i = 0; i < s->nM && q < VID_TOPE; i++) {
        if (s->TM[i] && s->TM[i][0] == 'v' && s->TM[i][1] == 'd') {
            double* m2 = (double*)s->M[i];
            long j2 = q;
            while (j2 > 0 && sd[j2 - 1] > m2[1]) {   // insercion: viejo -> nuevo
                ms[j2] = ms[j2 - 1]; sd[j2] = sd[j2 - 1]; j2--;
            }
            ms[j2] = m2; sd[j2] = m2[1]; q++;
        }
    }
    double* m = ms[ns - 1 - k];
    if (!m) return 0;
    long n = (long)m[0];
    if (n > (long)s->cC) grC(s, n);
    for (long i = 0; i < n; i++) s->C[i] = (long)m[2 + i];
    if (s->nC < n) s->nC = n;
    s->ulti = gTic; s->vr++; gGen++;
    if (pop)                             // se consume: su hueco queda libre
        for (long i = 0; i < s->nM; i++)
            if (s->M[i] == (void*)m) { xMe(m); s->M[i] = 0; s->TM[i] = 0; break; }
    return 1;
}
static long vidaDeshacer(void) {         // bitacora: el mas reciente vuelve
    if (gVidN < 1) return 0;
    Ser* s = gVid[gVidN - 1];
    gVidN--; gVid[gVidN] = 0;
    if (!s || s->tc == 0) return 0;
    return vidaRest(s, 0, 1);            // rest + consume la vida
}

// ---- [U] deshacer: log de asignaciones por grupos ----------------------------
struct Lg { Ser* t; long g; long ci; long old; char* olb; double row[6]; };
static struct Lg gLg[2048];               // un arrastre gasta 2 por pixel
static long gLgN = 0;                    // entradas usadas
static long gLgG = 0;                    // grupo de acciones actual

static void lgEmp() { gLgG++; }          // accion nueva del usuario: grupo nuevo
static void lgSuelta(long i) {           // libera olb solo si es texto propio
    struct Lg* e = &gLg[i];
    if (e->ci == -2) { xMe(e->olb); }
    e->olb = 0;
}
static void lgE(Ser* t, long ci, long old, char* olb) {
    if (!t || t->tc == 0) { if (ci == -2) xMe(olb); return; }  // muerto: nada
    if (gLgN >= 2048) {                  // sin lugar: se pierde el fondo
        for (long i = 0; i < gLgN; i++) lgSuelta(i);
        gLgN = 0;
    }
    gLg[gLgN].t = t; gLg[gLgN].g = gLgG; gLg[gLgN].ci = ci;
    gLg[gLgN].old = old; gLg[gLgN].olb = olb;
    gLgN++;
}
static void borraApuntados(Ser* t);      // escritor limpia colgantes al borrar
static void serAl(Ser* s);               // Cementerio libera deshaciendo evict
static void syncInsp();                  // escritor op 9 marca el clon
static Ser* elige(float mx, float my);   // menu contextual: elige debajo
static Ser* clonaSer(Ser* t, Ser* fol);  // op 9: copia profunda con filas

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
        if (!t) { lgSuelta(j); continue; }
        if (ci == -3) {                              // nodo creado: se quita
            if (t->tc != 0) {
                borraApuntados(t); t->vr++; gGen++; serAl(t);
            }
        } else if (ci == -4) {                       // borrado: se resucita
            if (t->tc != 0) resucita(t);
        } else if (ci == -5) {                       // fila SM: se repone entera
            if (t->tc == 0) { lgSuelta(j); continue; }
            double* f = (double*)gLg[j].olb;         // puntero a la fila (64)
            if (f) {
                long viva = 0;
                for (long m = 0; m < t->nM; m++)
                    if ((double*)t->M[m] == f) { viva = 1; break; }
                if (viva) {
                    for (long w2 = 0; w2 < 6; w2++) f[w2] = gLg[j].row[w2];
                    t->vr++; gGen++;
                }
                gLg[j].olb = 0;
            }
        } else {
            if (t->tc == 0) { lgSuelta(j); continue; }
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
static long long* cachePinta(Ser* c, long pintar);   // cache fresco ([N])
static void blitEsc(long long* c4, float dx, float dy,
                    float dw, float dh);        // cache a escala ([N])
static double* f6Add(Ser* s, double cond, double ci, double cmp,
                     double op, double dst, double arg) {
    long k = -1;
    for (long i = 0; i < s->nM; i++) if (!s->M[i] || !s->TM[i]) { k = i; break; }
    if (k < 0) { k = s->nM; if (k >= s->cM) grC(s, k); s->nM = k + 1; }
    if (s->M[k]) xMe(s->M[k]);
    s->M[k] = xM(6 * sizeof(double));
    s->TM[k] = (char*)"f6";
    caSuma(CA_MAT);
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
    caSuma(CA_MAT);
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
        if (op == 11) {                  // fsm: la maquina (fol) salta a dst (verd)
                                         // o a arg (falso); -1 o igual = se queda
            Ser* m = s->fol;
            long rv = 1;
            if (cond == 1) rv = (v < cmp);
            else if (cond == 2) rv = (v > cmp);
            else if (cond == 3) rv = (v == cmp);
            long nx = rv ? dst : arg;
            if (m && nx >= 0 && nx != ccG(m, CI_EST)) {
                ccS(m, CI_EST, nx); caSuma(CA_FIL); s->vr++; m->vr++; gGen++;
            }
            continue;
        }
        if (cond == 1) { if (!(v < cmp)) continue; }
        else if (cond == 2) { if (!(v > cmp)) continue; }
        else if (cond == 3) { if (v != cmp) continue; }
        if (op == 5) {                   // meta-UI: crb crear rama hija en cascada
            char tco = (char)dst;
            long cap = arg;
            if (cap < 1) cap = 1;
            if (tco && s->nA < cap) ordCr(tco, s);
            caSuma(CA_FIL);
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
        if (op == 6) {                   // reserva CAS: dst = slot tomado + 1
            *d = resToma(s) + 1;
            caSuma(CA_FIL);
            s->vr++; gGen++;
            continue;
        }
        if (op == 7) {                   // soltar el slot arg; dst queda en 0
            resSuelta(s, arg);
            *d = 0;
            caSuma(CA_FIL);
            s->vr++; gGen++;
            continue;
        }
        if (op == 8) {                   // confirmar el slot arg (reserva -> en uso)
            *d = resConfirma(s, arg);
            caSuma(CA_FIL);
            s->vr++; gGen++;
            continue;
        }
        if (op == 9 || op == 10) {       // vida: snapshot (9) / rest (10); el dst
                                         // se re-resuelve: C[] pudo crecer justo arriba
            long res = (op == 9) ? vidaToma(s) : vidaRest(s, arg, 0);
            d = 0;
            if (dst >= 0 && dst < s->nC) d = &s->C[dst];
            else if (dst < 0 && s->fol) {
                long dk = -dst - 1;
                if (dk >= 0 && dk < s->fol->nC) d = &s->fol->C[dk];
            }
            if (d) *d = res;
            caSuma(CA_FIL);
            s->vr++; gGen++;
            continue;
        }
        if (op == 0) *d = arg;                      // asignacion
        else if (op == 1) *d += arg;
        else if (op == 2) *d ^= arg;
        else if (op == 3) { if (*d > arg) *d = arg; if (*d < cmp) *d = cmp; }
        else if (op == 4) *d *= arg;
        caSuma(CA_FIL);
        s->vr++; gGen++;
        if (dst < 0 && s->fol) s->fol->vr++;        // el fol tambien cambia
    }
}
static long fsmPaso(Ser* m) {             // un paso de la maquina 'f': filas del estado vivo
    if (!m || m->nA == 0) return -1;
    long e = ccG(m, CI_EST);
    if (e < 0 || e >= m->nA) { ccS(m, CI_EST, 0); e = 0; }
    Ser* es = m->A[e];
    if (!es) return e;
    exRows(es);                           // las filas del estado corren (op 11 salta)
    long ne = ccG(m, CI_EST);
    if (ne < 0 || ne >= m->nA) { ne = e; ccS(m, CI_EST, ne); }   // salto invalido: se queda
    return ne;
}
static void tickN9(Ser* s) {              // contadores vivos: recorre el arbol
    if (!s) return;
    if (s->tc == 'f') fsmPaso(s);         // las maquinas avanzan por cuadro
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
static Ser* gMenu = 0;                  // menu contextual (clic derecho)
static Ser* gInsp2 = 0;                 // panel filas SM (fijado)
static Ser* gFilTxt = 0;                // texto vivo de la fila elegida
static long gFilI = 0;                  // fila SM elegida del objetivo
static Ser* gFilLast = 0;               // objetivo anterior: resetea gFilI
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
static const char* fGly(char c) {     // sistema de FUGA: minimo vital para
    switch (c >= 'a' && c <= 'z' ? (char)(c - 32) : c) {   // referencias cuando
                                           // no hay fuente prevista. Basico:
                                           // mayusculas, digitos, simbolos.
                                           // Sin minusculas ni acentos ni ene:
                                           // el case real va por fuentes de
                                           // datos (Glifo/Fuente), extendibles
                                           // a 6x10 o 10x50 segun demanda.
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

// ---- [L2] tipografias de datos: glifo = conectores entre puntos -------------
// Nada hardcodeado: el glifo es DATO (puntos del em-cuadro que se conectan con
// lineas, o bitmap en variable). Cada glifu gasta lo minimo: la i angosta,
// la m ancha, con su propio alto. El nativo 5x3 de arriba queda de socorro
// cuando el contexto no tiene glifos (por la causa que fuere).
struct Glifo {
    long cod;                // el byte 0..255 que dibuja (letras, digitos, simbolos)
    long an, alt;            // em-cuadro propio del glifo (diferente por letra)
    long nP;                 // conectores: polyline de puntos, escala libre
    long* pts;               // pares x,y (y=0 arriba), unidades del em-cuadro
    long nR;                 // bitmap en variable: nR filas x an bytes (0/1)
    unsigned char* bmp;
    long vr;                 // version del glifo (cachés/cosmetica)
};
struct Fuente {              // una tipografia por layout/contexto
    const char* ctx;         // "ui", "3d", el que venga
    const char* arch;        // persiste a disco por contexto, un archivo por uso
    struct Glifo* gl;
    long nG, cG;             // catalogo dinamico: agrega/quita segun demanda
    long* kr;                // espaciado por consecutividad: (a,b,dx) por par
    long nK, cK;
    long sucio;              // algo cambio: hay que guardar a disco
};
static struct Fuente gFuUI = { "ui", "fuente-ui.txt", 0, 0, 0, 0, 0, 0, 0 };
static struct Fuente gFu3D = { "3d", "fuente-3d.txt", 0, 0, 0, 0, 0, 0, 0 };
static struct Fuente* gFuAct = 0;        // el texto() pinta por este contexto
static void textoFu(struct Fuente* F, float x, float y, const char* t,
                    unsigned int c, float esc);

// ---- [R] raster: cache por zonas segun version ------------------------------
static long gW = 800, gH = 520;
static volatile long gPPP = 96;        // pixels por pulgada: la ventana lo mide
static volatile long gPXcm = 38;       // pixels por cm: ppp / 2.54
static long fsDePpp(long ppp) {        // escala de texto derivada del contexto
    long f = (ppp + 24) / 48;
    if (f < 1) f = 1;
    if (f > 8) f = 8;
    return f;                          // 96 -> 2, 144 -> 3, 192 -> 4
}
static long cmQz(double cm) {          // centimetros -> punto fijo, sin valores fijos
    return (long)(cm * (double)gPXcm * (double)QZ);
}
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
    if (gFuAct && gFuAct->nG) {          // comando con datos: vector por glifo
        textoFu(gFuAct, x, y, t, c, gFS < 1 ? 1.0f : (float)gFS);
        return;
    }
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

static void linea(long x0, long y0, long x1, long y1, unsigned int c) {
    long dx = x1 > x0 ? x1 - x0 : x0 - x1;
    long dy = y1 > y0 ? y1 - y0 : y0 - y1;
    long sx = x1 > x0 ? 1 : -1, sy = y1 > y0 ? 1 : -1, er = dx - dy;
    while (1) {
        px((int)x0, (int)y0, c);
        if (x0 == x1 && y0 == y1) break;
        long e2 = er * 2;
        if (e2 > -dy) { er -= dy; x0 += sx; }
        if (e2 < dx) { er += dx; y0 += sy; }
    }
}

// ---- [L3] fuentes en archivos: catalogo dinamico + espaciado por pares ------
static struct Glifo* fuenteDame(struct Fuente* F, long cod) {
    if (!F) return 0;
    for (long i = 0; i < F->nG; i++)
        if (F->gl[i].cod == cod) return &F->gl[i];
    return 0;
}
static long espDame(struct Fuente* F, long a, long b) {  // consecutividad
    if (!F) return 0;
    for (long i = 0; i < F->nK; i++)
        if (F->kr[i * 3] == a && F->kr[i * 3 + 1] == b)
            return F->kr[i * 3 + 2];
    return 0;
}
static struct Glifo* fuentePone(struct Fuente* F, long cod, long an, long alt,
                                long nP, long nR) {
    if (!F || cod < 0 || cod > 255) return 0;
    if (an < 1) an = 1;
    if (alt < 1) alt = 1;
    if (nP < 0) nP = 0;
    if (nR < 0) nR = 0;
    if (nP > 512 || nR > 200 || an > 200 || alt > 200) return 0;
    struct Glifo* g = fuenteDame(F, cod);
    if (!g) {                            // catalogo dinamico: crece al doble
        if (F->nG >= F->cG) {
            long nc = F->cG < 4 ? 4 : F->cG * 2;
            struct Glifo* nl = (struct Glifo*)xM((unsigned long)nc
                                                 * sizeof(struct Glifo));
            for (long i = 0; i < F->nG; i++) nl[i] = F->gl[i];
            xMe((void*)F->gl);
            F->gl = nl; F->cG = nc;
        }
        g = &F->gl[F->nG++];
    } else {                             // mismo codigo: reemplazo en el acto
        if (g->pts) xMe((void*)g->pts);
        if (g->bmp) xMe((void*)g->bmp);
    }
    g->cod = cod; g->an = an; g->alt = alt;
    g->nP = nP; g->nR = nR; g->pts = 0; g->bmp = 0; g->vr++;
    if (nP > 0) {
        g->pts = (long*)xM((unsigned long)(nP * 2) * sizeof(long));
        for (long i = 0; i < nP * 2; i++) g->pts[i] = 0;
    }
    if (nR > 0) {
        g->bmp = (unsigned char*)xM((unsigned long)(nR * an));
        for (long i = 0; i < nR * an; i++) g->bmp[i] = 0;
    }
    F->sucio = 1;
    return g;
}
static void fuenteQuita(struct Fuente* F, long cod) {    // segun demanda
    if (!F) return;
    for (long i = 0; i < F->nG; i++) {
        if (F->gl[i].cod != cod) continue;
        if (F->gl[i].pts) xMe((void*)F->gl[i].pts);
        if (F->gl[i].bmp) xMe((void*)F->gl[i].bmp);
        for (long j = i; j < F->nG - 1; j++) F->gl[j] = F->gl[j + 1];
        F->nG--;
        F->sucio = 1;
        return;
    }
}
static void espPone(struct Fuente* F, long a, long b, long dx) {
    if (!F) return;
    for (long i = 0; i < F->nK; i++)
        if (F->kr[i * 3] == a && F->kr[i * 3 + 1] == b) {
            F->kr[i * 3 + 2] = dx;
            F->sucio = 1;
            return;
        }
    if (F->nK >= F->cK) {                // pares dinamicos: doble y copia
        long nc = F->cK < 8 ? 8 : F->cK * 2;
        long* nl = (long*)xM((unsigned long)nc * 3 * sizeof(long));
        for (long i = 0; i < F->nK * 3; i++) nl[i] = F->kr[i];
        xMe((void*)F->kr);
        F->kr = nl; F->cK = nc;
    }
    F->kr[F->nK * 3] = a;
    F->kr[F->nK * 3 + 1] = b;
    F->kr[F->nK * 3 + 2] = dx;
    F->nK++;
    F->sucio = 1;
}
static float anchoFu(struct Fuente* F, const char* t, float esc) {
    float w = 0;
    long ant = 0;
    for (long k = 0; t && t[k]; k++) {
        long cc = (unsigned char)t[k];
        long dx = ant ? espDame(F, ant, cc) : 0;
        struct Glifo* g = fuenteDame(F, cc);
        long sc = (long)(esc < 1 ? 1 : esc);
        w += (float)(g ? g->an + dx : 5 * sc + dx);
        ant = cc;
    }
    return w * esc;
}
static void glifoDibuja(struct Glifo* g, float x, float y, unsigned int c,
                        float esc) {
    if (g->nP > 0 && g->pts) {           // conectores: polyline a escala libre
        long th = (long)(esc < 1 ? 1 : esc);
        for (long p = 0; p + 1 < g->nP; p++) {
            float x0 = x + (float)g->pts[p * 2] * esc;
            float y0 = y + (float)g->pts[p * 2 + 1] * esc;
            float x1 = x + (float)g->pts[p * 2 + 2] * esc;
            float y1 = y + (float)g->pts[p * 2 + 3] * esc;
            for (long o = 0; o < th; o++) // espesor = esc: linea gruesa a gusto
                linea((long)x0, (long)y0 + o, (long)x1, (long)y1 + o, c);
        }
    } else if (g->nR > 0 && g->bmp) {    // bitmap en variable: escala entera
        long oe = (long)(esc < 1 ? 1 : esc);
        for (long r = 0; r < g->nR; r++)
            for (long i2 = 0; i2 < g->an; i2++)
                for (long b2 = 0; b2 < oe; b2++)
                    for (long a2 = 0; a2 < oe; a2++)
                        if (g->bmp[r * g->an + i2])
                            px((int)(x + i2 * oe + a2),
                               (int)(y + r * oe + b2), c);
    }
}
static void textoFu(struct Fuente* F, float x, float y, const char* t,
                    unsigned int c, float esc) {
    float cx = x;
    long ant = 0;
    for (long k = 0; t && t[k]; k++) {
        long cc = (unsigned char)t[k];
        struct Glifo* g = fuenteDame(F, cc);
        long dx = ant ? espDame(F, ant, cc) : 0;
        if (g) {
            glifoDibuja(g, cx, y, c, esc);
            cx += (float)(g->an + dx) * esc;
        } else {                         // glifo faltante: nativo 5x3 de socorro
            long sc = (long)(esc < 1 ? 1 : esc);
            const char* gn = fGly((char)cc);
            for (long j = 0; j < 5; j++) {
                int fil = gn[j] - '0';
                for (long i2 = 0; i2 < 3; i2++) {
                    if (((fil >> (2 - i2)) & 1) == 0) continue;
                    for (long b2 = 0; b2 < sc; b2++)
                        for (long a2 = 0; a2 < sc; a2++)
                            px((int)(cx + i2 * sc + a2 + 1),
                               (int)(y + j * sc + b2 + 1), c);
                }
            }
            cx += 5 * sc + (float)dx;
        }
        ant = cc;
    }
}
static long lnLong(const char* l, long vi) {   // i-esimo numero de la linea
    long i = 0;
    while (*l) {
        while (*l == ' ' || *l == '\t' || *l == ',') l++;
        if (*l == 0) break;
        if (*l == '-' || (*l >= '0' && *l <= '9')) {
            char* fin = 0;
            long v = strtol(l, &fin, 10);
            if (i++ == vi) return v;
            l = fin;
        } else {
            while (*l && *l != ' ' && *l != '\t' && *l != ',') l++;
        }
    }
    return 0;
}
static long fuenteCarga(struct Fuente* F) {  // agrega del disco: G/P/R/K
    if (!F || !F->arch) return 0;
    FILE* f = fopenD(F->arch, "rb");
    if (!f) return 0;
    char ln[2048];
    struct Glifo* pend = 0;
    long nr = 0;
    while (fgets(ln, sizeof(ln), f)) {
        for (char* q = ln; *q; q++) if (*q == '\n' || *q == '\r') *q = 0;
        char* l = ln;
        while (*l == ' ' || *l == '\t') l++;
        if (*l == 0 || *l == '#') continue;
        if (l[0] == 'G') {
            long cod = lnLong(l, 0), an = lnLong(l, 1), alt = lnLong(l, 2);
            long nP = lnLong(l, 3), nR = lnLong(l, 4);
            pend = fuentePone(F, cod, an, alt, nP, nR);   // topea adentro
            nr = 0;
        } else if (l[0] == 'P' && pend && pend->nP > 0 && pend->pts) {
            for (long i = 0; i < pend->nP * 2; i++)
                pend->pts[i] = lnLong(l, i);
        } else if (l[0] == 'R' && pend && pend->nR > nr && pend->bmp) {
            long i2 = 0;
            for (char* q = l + 1; *q && i2 < pend->an; q++)
                if (*q == '0' || *q == '1')
                    pend->bmp[nr * pend->an + i2++] = (unsigned char)(*q - '0');
            nr++;
        } else if (l[0] == 'K') {
            espPone(F, lnLong(l, 0), lnLong(l, 1), lnLong(l, 2));
        }
    }
    fclose(f);
    F->sucio = 0;                        // recien cargada: sin deuda al disco
    return 1;
}
static void fuenteGuarda(struct Fuente* F) { // el layout entero a su archivo
    if (!F || !F->arch || !F->sucio) return;
    FILE* f = fopenD(F->arch, "wb");
    if (!f) return;
    fprintf(f, "# fuente contexto %s\n", F->ctx);
    for (long i = 0; i < F->nG; i++) {
        struct Glifo* g = &F->gl[i];
        fprintf(f, "G %ld %ld %ld %ld %ld\n", g->cod, g->an, g->alt,
                g->nP, g->nR);
        if (g->nP > 0 && g->pts) {
            fprintf(f, "P");
            for (long p = 0; p < g->nP * 2; p++) fprintf(f, " %ld", g->pts[p]);
            fprintf(f, "\n");
        }
        for (long r = 0; g->nR > 0 && g->bmp && r < g->nR; r++) {
            fprintf(f, "R ");
            for (long i2 = 0; i2 < g->an; i2++)
                fprintf(f, "%c", g->bmp[r * g->an + i2] ? '1' : '0');
            fprintf(f, "\n");
        }
    }
    for (long i = 0; i < F->nK; i++)
        fprintf(f, "K %ld %ld %ld\n", F->kr[i * 3], F->kr[i * 3 + 1],
                F->kr[i * 3 + 2]);
    fclose(f);
    F->sucio = 0;
}
static void fuenteLibera(struct Fuente* F) {
    if (!F) return;
    while (F->nG) fuenteQuita(F, F->gl[F->nG - 1].cod);
    if (F->gl) { xMe((void*)F->gl); F->gl = 0; }
    F->nG = 0; F->cG = 0;
    if (F->kr) { xMe((void*)F->kr); F->kr = 0; }
    F->nK = 0; F->cK = 0; F->sucio = 0;
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
// Pintor real: cada sere se toma de su cache (cachePinta) y se vuelca a
// escala, asi el colado es la escena misma (widgets, titulos, textos), no
// rects crudos. Resolucion pedida: base 64x48 para la base y, anidada con
// camara, la camara Colado pide la suya (gColImg en WinMain).
static volatile long gColAnti = 400, gColImp = 300, gColUrg = 300;   // pesos 0..1000
static Ser* gColNodo = 0;               // camara 'm' que muestra lo colado
static Ser* gColPnl = 0;                // panel con los sliders de los pesos
static volatile long gColNva = 0;       // embudo: colado nuevo
static unsigned int gBufCol[64 * 48];   // imagen colada base 64x48
#if defined(_WIN32)
static unsigned int* gColImg = 0;       // colado a resolucion de la camara
static long gColWc = 0, gColHc = 0;
#endif

static void coladorPinta(unsigned int* dst, long dw, long dh) {
    if (!gRoot || !dst || dw < 12 || dh < 12) return;
    for (long i = 0; i < dw * dh; i++) dst[i] = 0;
    long bx0 = 1 << 30, by0 = 1 << 30, bx1 = -(1 << 30), by1 = -(1 << 30);
    for (long i = 0; i < gRoot->nA; i++) {
        Ser* s = gRoot->A[i];
        if (!s || s->tc == 0) continue;
        long fl = ccG(s, CI_FLG);
        if (s->tc != 'r' && !(fl & 1)) continue;
        long w = ccG(s, CI_W), h = ccG(s, CI_H);
        if (w <= 0 || h <= 0) continue;
        long x = 0, y2 = 0; posEf(s, &x, &y2);
        if (x < bx0) bx0 = x;
        if (y2 < by0) by0 = y2;
        if (x + w > bx1) bx1 = x + w;
        if (y2 + h > by1) by1 = y2 + h;
    }
    __sync_lock_test_and_set(&gColNva, 1);
    if (bx1 <= bx0 || by1 <= by0) return;
    float fs = (float)(dw - 4) / (float)(bx1 - bx0);
    float fy = (float)(dh - 4) / (float)(by1 - by0);
    if (fy < fs) fs = fy;
    unsigned int* tg0 = gTgt;            // el pintor rasteriza en gTgt
    long tw0 = gTW, th0 = gTH;
    struct Fuente* fu0 = gFuAct;
    gTgt = dst; gTW = dw; gTH = dh;
    gFuAct = &gFuUI;                     // el texto sale con su contexto
    for (long i = 0; i < gRoot->nA; i++) {
        Ser* s = gRoot->A[i];
        if (!s || s->tc == 0) continue;
        long fl = ccG(s, CI_FLG);
        long vis = fl & 1;
        if (s->tc != 'r' && !vis) continue;
        long w = ccG(s, CI_W), h = ccG(s, CI_H);
        if (w <= 0 || h <= 0) continue;
        long impL = (fl & 8) ? 900 : (ccG(s, CI_ABI) ? 500 : (vis ? 300 : 0));
        long dc = gTic - s->ulti;
        float fres = 1000.0f - (dc > 16000 ? 1000.0f : (float)dc / 16.0f);
        if (fres < 0) fres = 0;
        float urg = s->nM > 0 ? 600.0f : 100.0f;
        float sc = ((float)gColAnti * fres + (float)gColImp * (float)impL
                    + (float)gColUrg * urg) / 1000.0f;
        if (sc < 1.0f) continue;         // el colador lo deja pasar de largo
        float ax = 2.0f + ((float)ccG(s, CI_X) - (float)bx0) * fs
                   - 3.0f * (float)QZ * fs;    // cache: marco de 3 px en (0,0)
        float ay = 2.0f + ((float)ccG(s, CI_Y) - (float)by0) * fs
                   - 3.0f * (float)QZ * fs;
        long long* c4 = vis ? cachePinta(s, 1) : 0;
        if (c4 && (long)c4[1] > 0 && (long)c4[2] > 0) {
            blitEsc(c4, ax, ay, (float)c4[1] * (float)QZ * fs,
                    (float)c4[2] * (float)QZ * fs);
        } else {                         // sin pintura (invisible): rect color
            float aw = (float)w * fs, ah = (float)h * fs;
            if (aw < 1.0f) aw = 1.0f;
            if (ah < 1.0f) ah = 1.0f;
            unsigned int col = pr((float)ccG(s, CI_R) / QZ,
                                  (float)ccG(s, CI_G) / QZ,
                                  (float)ccG(s, CI_B) / QZ,
                                  (float)ccG(s, CI_A) / QZ);
            triLleno(dst, dw, dh, ax, ay, ax + aw, ay, ax, ay + ah, col);
            triLleno(dst, dw, dh, ax + aw, ay, ax, ay + ah, ax + aw, ay + ah, col);
        }
    }
    gTgt = tg0; gTW = tw0; gTH = th0;
    gFuAct = fu0;
}
static void colador() {                  // pase base: siempre 64x48
    coladorPinta(gBufCol, 64, 48);
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
static Ser** gLin = 0;     // lista colada pintador (mismo orden sirve al pick):
static long gLinN = 0;     // dinamica: crece al doble, nunca se recorta
static long gLinC = 0;

static void fsmBand(float w, long nA, float* cwt, float* chp,
                    float* fyA, float* rail) {   // geometria del diagrama fsm
    *cwt = (w - 8) / (nA > 1 ? (float)nA : 1);
    if (*cwt < 4) *cwt = 4;
    *chp = 3 * gFS + 4;                  // alto del chip: de la fuente
    *fyA = 5 * gFS + 2;                  // franja del diagrama
    *rail = *fyA + *chp + 2;             // riel de saltos
}
static void pintaNodo(Ser* s, float ox, float oy) {
    if (!s) return;
    long fl = ccG(s, CI_FLG);
    if (s->tc != 'r' && !(fl & 1)) return;
    long ex = 0, ey = 0; posEf(s, &ex, &ey);
    float x = ox + (float)ex / QZ;
    float y = oy + (float)ey / QZ;
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
    } else if (s->tc == 'f') {           // maquina: panel + diagrama de estados
        rectoQ(x, y, w, h, col, red);
        rectoQ(x + 1, y + 1, w - 2 < 0 ? 0 : w - 2, 14 < h - 2 ? 14 : h - 2,
               pr(r * 1.35f, g * 1.35f, b * 1.35f, a > 0.5f ? 1 : a), red);
        texto(x + 4, y + 3, s->lb, pr(1, 1, 1, 1));
        if (s->nA > 0) {
            float cwt = 0, chp = 0, fyA = 0, rail = 0;
            fsmBand(w, s->nA, &cwt, &chp, &fyA, &rail);
            for (long i = 0; i < s->nA; i++) {
                float cx = x + 4 + i * cwt;
                unsigned int cc = (i == ccG(s, CI_EST))
                    ? pr(1, 0.75f, 0.15f, 1)         // estado vivo: ambar
                    : pr(0.25f, 0.3f, 0.4f, 1);
                rectoQ(cx, y + fyA, cwt - 2 < 2 ? 2 : cwt - 2, chp, cc, 2);
                Ser* es2 = s->A[i];
                if (!es2) continue;
                for (long rj = 0; rj < es2->nM; rj++) {   // saltos op 11: rieles
                    if (!es2->M[rj] || !es2->TM[rj] || es2->TM[rj][0] != 'f') continue;
                    double* r6 = (double*)es2->M[rj];
                    if ((long)r6[3] != 11) continue;
                    long nx = (long)r6[4];
                    if (nx < 0 || nx >= s->nA || nx == i) continue;
                    float x0d = cx + (cwt - 2) * 0.5f;
                    float x1d = x + 4 + nx * cwt + (cwt - 2) * 0.5f;
                    if (x1d < x0d) { float tt = x0d; x0d = x1d; x1d = tt; }
                    rectoQ(x0d, y + rail, x1d - x0d, 1.5f, pr(0.5f, 0.8f, 1, 1), 0);
                }
            }
        }
        char fb[32];
        snprintf(fb, sizeof(fb), "est %ld de %ld", ccG(s, CI_EST), s->nA);
        float fy = y + h - 6 * gFS;
        if (fy < y + 16) fy = y + 16;
        texto(x + 4, fy, fb, pr(0.7f, 0.85f, 1, 1));
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
    if (gLinN >= gLinC) {                 // gLin dinamico: doble y copia
        long nc = gLinC ? gLinC * 2 : 1024;
        Ser** nl = (Ser**)xM((unsigned long)nc * sizeof(Ser*));
        for (long i = 0; i < gLinN; i++) nl[i] = gLin[i];
        xMe((void*)gLin);
        gLin = nl; gLinC = nc;
    }
    gLin[gLinN++] = s;
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
static void blitEsc(long long* c4, float dx, float dy,  // cache a escala
                    float dw, float dh) {
    if (!c4 || dw < 1.0f || dh < 1.0f) return;
    unsigned int* im = (unsigned int*)c4[0];
    long w = (long)c4[1], h = (long)c4[2];
    if (w < 1 || h < 1) return;
    long tw = (long)dw, th = (long)dh;
    for (long j = 0; j < th; j++) {
        long sj = (long)((float)j * (float)h / (float)th);
        for (long i = 0; i < tw; i++) {
            unsigned int c = im[sj * w +
                                (long)((float)i * (float)w / (float)tw)];
            if ((c >> 24) & 255) px((int)(dx + (float)i), (int)(dy + (float)j), c);
        }
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
static long arFol(Ser* s) {              // el nodo tiene fila con arco al fol
    if (!s) return 0;
    for (long i = 0; i < s->nM; i++)
        if (s->M[i] && s->TM[i] && s->TM[i][0] == 'f'
            && (long)((double*)s->M[i])[4] < 0) return 1;
    return 0;
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
static void segArcos(Ser* t, float wx, float wy, float wz,
                     float w, float hh, float dz) { // semiarco: banda curva
    long arc = ccG(t, CI_ARC);
    if (arc < 0) arc = -arc;
    long pasos = 24;                     // barrido trozado en 24 segmentos
    double pi = 3.14159265358979;
    double a0 = (double)ccG(t, CI_AGL) / (double)QZ * (pi / 180.0);
    double sp = (double)arc / (double)QZ * (pi / 180.0);
    double ro = (double)w / 60.0;        // radio exterior (mundo: px / 30 / 2)
    double ri = ro - (double)hh * 2.0;   // grosor de banda = alto H del nodo
    if (ri < ro * 0.25) ri = ro * 0.25;  // banda minima
    if (ri > ro) ri = ro * 0.25;
    long long* c4 = 0;                   // textura: la imagen 'x' del nodo
    for (long i = 0; i < t->nM; i++)
        if (t->TM[i] && t->TM[i][0] == 'x' && t->M[i]) c4 = (long long*)t->M[i];
    unsigned int* im2 = c4 ? (unsigned int*)c4[0] : 0;
    long iw = c4 ? (long)c4[1] : 0, ih = c4 ? (long)c4[2] : 0;
    float fr = (float)ccG(t, CI_R) / QZ, fg2 = (float)ccG(t, CI_G) / QZ;
    float fb = (float)ccG(t, CI_B) / QZ;
    float sY = (float)sin((double)g3Az);   // nz = 0: entra solo el giro en azimut
    float cX = (float)cos((double)g3El), sX = (float)sin((double)g3El);
    for (long s2 = 0; s2 < pasos; s2++) {
        double f0 = (double)s2 / pasos, f1 = (double)(s2 + 1) / pasos;
        double r0 = a0 + sp * f0, r1 = a0 + sp * f1;
        double am = a0 + sp * (f0 + f1) * 0.5;
        float nx = (float)cos(am), ny = (float)sin(am);
        float z2 = -nx * sY;             // banda en el piso (nz = 0)
        float rz2 = ny * sX + z2 * cX;
        float ry2 = ny * cX - z2 * sX;
        float sh = 0.35f + 0.65f * (0.4f - rz2 * 0.7f + 0.6f * ry2 + 0.4f * rz2);
        if (sh < 0.2f) sh = 0.2f;
        if (sh > 1.0f) sh = 1.0f;
        unsigned int col;
        if (im2 && iw > 0 && ih > 0) {   // texturizado: un sampleo por segmento
            long tu = (long)((f0 + f1) * 0.5 * (double)iw);
            if (tu >= iw) tu = iw - 1;
            long tv = ih / 2; if (tv >= ih) tv = ih - 1;
            unsigned int px2 = im2[tv * iw + tu];
            col = pr((float)((px2 >> 16) & 255) / 255.0f * sh,
                     (float)((px2 >> 8) & 255) / 255.0f * sh,
                     (float)(px2 & 255) / 255.0f * sh, 1.0f);
        } else
            col = pr(fr * sh, fg2 * sh, fb * sh, 1.0f);
        unsigned int colO = pr((float)((col >> 16) & 255) / 255.0f * 0.7f,
                               (float)((col >> 8) & 255) / 255.0f * 0.7f,
                               (float)(col & 255) / 255.0f * 0.7f, 1.0f);
        float ax0, ay0, ax1, ay1, ix1, iy1, ix0, iy0, qw;
        pr3(wx + (float)(cos(r0) * ro), wy + (float)(sin(r0) * ro),
            wz + dz, &ax0, &ay0, &qw);
        pr3(wx + (float)(cos(r1) * ro), wy + (float)(sin(r1) * ro),
            wz + dz, &ax1, &ay1, &qw);
        pr3(wx + (float)(cos(r1) * ri), wy + (float)(sin(r1) * ri),
            wz + dz, &ix1, &iy1, &qw);
        pr3(wx + (float)(cos(r0) * ri), wy + (float)(sin(r0) * ri),
            wz + dz, &ix0, &iy0, &qw);
        triLleno(gImg, gW, gH, ax0, ay0, ax1, ay1, ix1, iy1, col);
        triLleno(gImg, gW, gH, ax0, ay0, ix1, iy1, ix0, iy0, col);
        float x1, y1, x0, y0;
        pr3(wx + (float)(cos(r1) * ro), wy + (float)(sin(r1) * ro),
            wz, &x1, &y1, &qw);          // canto exterior: todo el espesor
        pr3(wx + (float)(cos(r0) * ro), wy + (float)(sin(r0) * ro),
            wz, &x0, &y0, &qw);
        triLleno(gImg, gW, gH, ax0, ay0, ax1, ay1, x1, y1, colO);
        triLleno(gImg, gW, gH, ax0, ay0, x1, y1, x0, y0, colO);
    }
}

static void pinta3D() {
    if (!gImg || !gRoot) return;
    long gFS0 = gFS;
    gTgt = gImg; gTW = gW; gTH = gH;
    gFuAct = &gFu3D;                     // contexto 3D: su propia tipografia
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
        long e3x = 0, e3y = 0; posEf(t, &e3x, &e3y);
        long x = e3x / QZ, y = e3y / QZ;
        long w = ccG(t, CI_W) / QZ, h = ccG(t, CI_H) / QZ;
        if (ccG(t, CI_ARC) != 0) {           // la banda vive fuera del rect:
            long cxA = x + w / 2, cyA = y + h / 2;   // el anillo entra al bbox
            long rp = (w > h ? w : h) / 2 + 2;
            if (cxA - rp < bx0) bx0 = cxA - rp;
            if (cyA - rp < by0) by0 = cyA - rp;
            if (cxA + rp > bx1) bx1 = cxA + rp;
            if (cyA + rp > by1) by1 = cyA + rp;
            continue;
        }
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
            long e3x = 0, e3y = 0; posEf(t, &e3x, &e3y);
            float wx = ((float)(e3x + ccG(t, CI_W) / 2))
                       / QZ / 30.0f - ccx;
            float wy = ((float)(e3y + ccG(t, CI_H) / 2))
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
            long e4x = 0, e4y = 0; posEf(t, &e4x, &e4y);
            float wx = (float)e4x / QZ / 30.0f + hw - ccx;
            float wy = (float)e4y / QZ / 30.0f + hh - ccy;
            float wz = -1.1f * (float)niv;
            if (ccG(t, CI_ARC) != 0) {   // semiarco: banda curva en el piso
                segArcos(t, wx, wy, wz, w, hh, dz);
                continue;                // sin caja
            }
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
    unsigned int wc = pr(0.45f, 0.7f, 1.0f, 1.0f);   // cable de arco al fol
    for (long i = 0; i < g3N3; i++)          // nodo con arco -> cable al padre:
        for (long j = 0; j < g3N3; j++)      // la fila se ve fluyendo en 3D
            if (g3Se[j] && g3Se[j]->fol == g3Se[i] && arFol(g3Se[j]))
                linea((long)g3Px[j], (long)g3Py[j],
                      (long)g3Px[i], (long)g3Py[i], wc);
    if (bx1 > bx0 && by1 > by0) {
        long nmax = 0;
        for (long i = 0; i < g3N3; i++) if (g3Nv[i] > nmax) nmax = g3Nv[i];
        float ccx = (float)(bx0 + bx1) * 0.5f / 30.0f;
        float ccy = (float)(by0 + by1) * 0.5f / 30.0f;
        float zf = -1.1f * (float)nmax;      // piso: al fondo del ultimo nivel
        unsigned int gcp = pr(0.13f, 0.16f, 0.22f, 1.0f);
        float x0m = (float)bx0 / 30.0f - ccx, x1m = (float)bx1 / 30.0f - ccx;
        float y0m = (float)by0 / 30.0f - ccy, y1m = (float)by1 / 30.0f - ccy;
        for (long gx = ((bx0 - 1) / 60) * 60; gx <= bx1; gx += 60) {
            float a1, b1, a2, b2, qw;
            pr3((float)gx / 30.0f - ccx, y0m, zf, &a1, &b1, &qw);
            pr3((float)gx / 30.0f - ccx, y1m, zf, &a2, &b2, &qw);
            linea((long)a1, (long)b1, (long)a2, (long)b2, gcp);
        }
        for (long gy = ((by0 - 1) / 60) * 60; gy <= by1; gy += 60) {
            float a1, b1, a2, b2, qw;
            pr3(x0m, (float)gy / 30.0f - ccy, zf, &a1, &b1, &qw);
            pr3(x1m, (float)gy / 30.0f - ccy, zf, &a2, &b2, &qw);
            linea((long)a1, (long)b1, (long)a2, (long)b2, gcp);
        }
        float xr = (float)bx0 / 30.0f - ccx - 0.8f;   // regla de profundidad
        long gFSr = gFS;
        gFS = 1;
        for (long niv = 0; niv <= nmax; niv++) {
            float zp = -1.1f * (float)niv;
            float a1, b1, a2, b2, qw;
            pr3(xr, y0m, zp, &a1, &b1, &qw);
            pr3(xr, y0m + 0.8f, zp, &a2, &b2, &qw);
            linea((long)a1, (long)b1, (long)a2, (long)b2, wc);
            char et[8];
            et[0] = 'N'; et[1] = (char)('0' + (niv % 10)); et[2] = 0;
            texto((float)a1 + 2.0f, (float)b1 - 3.0f, et,
                  pr(0.35f, 0.4f, 0.5f, 1.0f));
        }
        gFS = gFSr;
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
    gFuAct = 0;
    gImgGen = -1;                        // en 3D se repinta cada frame
}
static long long* cachePinta(Ser* c, long pintar) { // cache con pintura fresca
    long w = extW(c), h = extH(c);
    long long* c4 = cacheRow(c, w, h);
    if (!c4) return 0;
    if (!pintar) { c4[3] = -1; return c4; }   // invisible: cache vencida
    long gv = gvUp(c);
    if (gv == c4[3]) return c4;               // vigente: nada que hacer
    unsigned int* sub = (unsigned int*)c4[0];
    memset(sub, 0, (size_t)((long)c4[1] * (long)c4[2]) * sizeof(unsigned int));
    unsigned int* tg0 = gTgt;
    long tw0 = gTW, th0 = gTH;
    gTgt = sub; gTW = (long)c4[1]; gTH = (long)c4[2];
    long px2 = 0, py2 = 0; posEf(c, &px2, &py2);
    pintaNodo(c, 3 - (float)px2 / QZ,      // pintado local al cache
                 3 - (float)py2 / QZ);
    c4[3] = gv;
    gTgt = tg0; gTW = tw0; gTH = th0;
    return c4;
}
static void pintaTodo() {
    if (!gImg) return;
    if (g3D) { pinta3D(); return; }      // 3D: vista orbital de la escena
    if (gImgGen == gGen && gCamVisto == gCamVer) return;   // nada cambio en TODO
    gTgt = gImg; gTW = gW; gTH = gH;
    gFuAct = &gFuUI;                     // el texto del mundo sale con su fuente
    gLinN = 0;
    listaRec(gRoot);                              // pick comparte el colado
    if (gRoot) {
        for (long pase = 0; pase < 2; pase++)         // pin CI_FLG&8 al final
        for (long i = 0; i < gRoot->nA; i++) {
            Ser* c = gRoot->A[i];
            if (!c || (((ccG(c, CI_FLG) & 8) != 0) != (pase == 1))) continue;
            long vis = ccG(c, CI_FLG) & 1;
            long pin = ccG(c, CI_FLG) & 8;
            long ex = 0, ey = 0; posEf(c, &ex, &ey);
            int ax = (int)(ex - (pin ? 0 : gCamX) - 3 * QZ) / QZ;
            int ay = (int)(ey - (pin ? 0 : gCamY) - 3 * QZ) / QZ;
            limpia(ax, ay, extW(c), extH(c));     // zona -> fondo
            if (!vis) continue;
            long long* c4 = cachePinta(c, 1);     // fresco: repinta si cambio
            if (!c4) continue;
            blit(c4, ax, ay);
        }
    }
    gImgGen = gGen;
    gCamVisto = gCamVer;
    gFuAct = 0;
}

// ---- [K] pick por zonas anidables (mismo orden de colado) ------------------
static Ser* eligeZ(Ser* s, float mx, float my) {  // mx,my: coords del fol de s
    if (!s) return 0;
    long fl = ccG(s, CI_FLG);
    if (s->tc != 'r' && !(fl & 1)) return 0;
    float x = 0, y = 0, w = 0, h = 0;
    if (s->tc != 'r') {
        long ex = 0, ey = 0; posEf(s, &ex, &ey);
        x = (float)ex / QZ;
        y = (float)ey / QZ;
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

static Ser* gSel = 0; static Ser* gAnt = 0;   // seleccion actual y previa
static Ser* gArra = 0;               // nodo arrastrado
static float gDOffX, gDOffY;          // anclaje del arrastre
static int   gSlid = 0;               // arrastrando un slider

// ---- [EX] exportes: PNG sin librerias + documento de escena ------------------
static unsigned int exTab[256];          // crc32 de mesa propia: sin zlib
static long exTabOK = 0;
static unsigned int crcIni(void) { return 0xFFFFFFFFu; }
static unsigned int crcUpd(unsigned int c, const unsigned char* d, long n) {
    if (!exTabOK) {
        for (unsigned int i2 = 0; i2 < 256; i2++) {
            unsigned int k = i2;
            for (long j = 0; j < 8; j++)
                k = (k & 1) ? (0xEDB88320u ^ (k >> 1)) : (k >> 1);
            exTab[i2] = k;
        }
        exTabOK = 1;
    }
    for (long i = 0; i < n; i++) c = exTab[(c ^ d[i]) & 255] ^ (c >> 8);
    return c;
}
static unsigned int crcFin(unsigned int c) { return c ^ 0xFFFFFFFFu; }
static unsigned int adlerD(const unsigned char* d, long n) {
    unsigned int a = 1, b = 0;
    for (long i = 0; i < n; i++) {
        a = (a + d[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}
static void grChunk(FILE* f, const char* tip, const unsigned char* d, long n) {
    unsigned char be[8];
    be[0] = (unsigned char)((n >> 24) & 255);
    be[1] = (unsigned char)((n >> 16) & 255);
    be[2] = (unsigned char)((n >> 8) & 255);
    be[3] = (unsigned char)(n & 255);
    fwrite(be, 1, 4, f);
    fwrite(tip, 1, 4, f);
    if (n > 0 && d) fwrite(d, 1, (unsigned long)n, f);
    unsigned int cc = crcIni();
    cc = crcUpd(cc, (const unsigned char*)tip, 4);
    if (n > 0 && d) cc = crcUpd(cc, d, n);
    cc = crcFin(cc);
    be[0] = (unsigned char)((cc >> 24) & 255);
    be[1] = (unsigned char)((cc >> 16) & 255);
    be[2] = (unsigned char)((cc >> 8) & 255);
    be[3] = (unsigned char)(cc & 255);
    fwrite(be, 1, 4, f);
}
static long pngGraba(const char* nom, unsigned int* im, long w, long h) {
    if (!im || w < 1 || h < 1 || w > 8192 || h > 8192) return 0;
    FILE* f = fopenD(nom, "wb");
    if (!f) return 0;
    static const unsigned char firma[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    fwrite(firma, 1, 8, f);
    long cru = w * 3 + 1;                // fila frames: filtro 0 + RGB
    unsigned char ih[13];
    ih[0] = (unsigned char)((w >> 24) & 255);
    ih[1] = (unsigned char)((w >> 16) & 255);
    ih[2] = (unsigned char)((w >> 8) & 255);
    ih[3] = (unsigned char)(w & 255);
    ih[4] = (unsigned char)((h >> 24) & 255);
    ih[5] = (unsigned char)((h >> 16) & 255);
    ih[6] = (unsigned char)((h >> 8) & 255);
    ih[7] = (unsigned char)(h & 255);
    ih[8] = 8; ih[9] = 2; ih[10] = 0; ih[11] = 0; ih[12] = 0;   // 8 bits, RGB
    grChunk(f, "IHDR", ih, 13);
    unsigned char* crudo = (unsigned char*)xM((unsigned long)(cru * h));
    for (long y = 0; y < h; y++) {
        crudo[y * cru] = 0;
        for (long x = 0; x < w; x++) {
            unsigned int pv = im[y * w + x];
            crudo[y * cru + 1 + x * 3] = (unsigned char)((pv >> 16) & 255);
            crudo[y * cru + 2 + x * 3] = (unsigned char)((pv >> 8) & 255);
            crudo[y * cru + 3 + x * 3] = (unsigned char)(pv & 255);
        }
    }
    long nb2 = (cru * h + 65534) / 65535;// zlib: bloques GUARDADOS, fiables
    unsigned char* z = (unsigned char*)xM((unsigned long)(2 + nb2 * 5
                                           + cru * h + 4));
    z[0] = 0x78; z[1] = 0x01;
    long pz = 2, re = cru * h, of = 0;
    while (re > 0) {
        long bl = re < 65535 ? re : 65535;
        z[pz++] = (unsigned char)(bl >= re ? 1 : 0);
        z[pz++] = (unsigned char)(bl & 255);
        z[pz++] = (unsigned char)((bl >> 8) & 255);
        z[pz++] = (unsigned char)((~bl) & 255);          // NLEN = ~LEN
        z[pz++] = (unsigned char)(((~bl) >> 8) & 255);
        memcpy(z + pz, crudo + of, (unsigned long)bl);
        pz += bl; of += bl; re -= bl;
    }
    unsigned int ad = adlerD(crudo, cru * h);
    z[pz++] = (unsigned char)((ad >> 24) & 255);
    z[pz++] = (unsigned char)((ad >> 16) & 255);
    z[pz++] = (unsigned char)((ad >> 8) & 255);
    z[pz++] = (unsigned char)(ad & 255);
    grChunk(f, "IDAT", z, pz);
    xMe((void*)z);
    xMe((void*)crudo);
    grChunk(f, "IEND", 0, 0);
    fclose(f);
    return 1;
}
static void docRec(FILE* f, Ser* t, long prof) {     // el arbol como documento
    if (!t) return;
    for (long i = 0; i < prof; i++) fprintf(f, "  ");
    long px2 = 0, py2 = 0; posEf(t, &px2, &py2);
    fprintf(f, "%c '%s' [%ld x %ld] en (%ld, %ld) val %ld\n", t->tc,
            t->lb ? t->lb : "", ccG(t, CI_W) / QZ, ccG(t, CI_H) / QZ,
            px2 / QZ, py2 / QZ, ccG(t, CI_VAL));
    if (ccG(t, CI_RAD) != 0)
        fprintf(f, "  polar: radio %ld grados %ld\n",
                ccG(t, CI_RAD) / QZ, ccG(t, CI_AGL) / QZ);
    for (long m = 0; m < t->nM; m++) {               // cada fila SM al pie
        if (!t->M[m] || !t->TM[m] || t->TM[m][0] != 'f') continue;
        double* r6 = (double*)t->M[m];
        for (long i = 0; i <= prof; i++) fprintf(f, "  ");
        fprintf(f, "fila: cond %g ci %g cmp %g op %g dst %g arg %g\n",
                r6[0], r6[1], r6[2], r6[3], r6[4], r6[5]);
    }
    if (ccG(t, CI_PON) > 0) {                        // pool de reservas activo
        for (long i = 0; i <= prof; i++) fprintf(f, "  ");
        fprintf(f, "pool: %ld de %ld slots ocupados (epoch %ld)\n",
                resVivos(t), ccG(t, CI_PON), resEpoch(t));
    }
    if (vidaN(t) > 0) {
        for (long i = 0; i <= prof; i++) fprintf(f, "  ");
        fprintf(f, "vidas: %ld snapshots (tope %ld)\n", vidaN(t), (long)VID_TOPE);
    }
    if (t->tc == 'f' && t->nA > 0) {                 // maquina de estados viva
        for (long i = 0; i <= prof; i++) fprintf(f, "  ");
        fprintf(f, "fsm: estado %ld de %ld estados\n", ccG(t, CI_EST), t->nA);
    }
    for (long i = 0; i < t->nA; i++) docRec(f, t->A[i], prof + 1);
}
static long docEscena(const char* nom) {
    FILE* f = fopenD(nom, "wb");
    if (!f) return 0;
    fprintf(f, "Documento de escena - Ser\n");
    fprintf(f, "tic %ld\n", gTic);
    fprintf(f, "contadores ca ser %ld muertes %ld matrices %ld filas %ld\n",
            gCa[CA_SER], gCa[CA_MUER], gCa[CA_MAT], gCa[CA_FIL]);
    fprintf(f, "contadores ca ordenes %ld reservas %ld exportes %ld vidas %ld\n",
            gCa[CA_ORD], gCa[CA_RES], gCa[CA_EXP], gCa[CA_VIDA]);
    docRec(f, gRoot, 0);
    fclose(f);
    return 1;
}

// ---- [E] escritor unico: ejecuta la cola ------------------------------------
static void escritor() {
    while (oTail != oHead) {
        Ord* o = &oRing[oTail & 255];
        if (o->k >= 10) caSuma(CA_EXP);
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
            FILE* f = fopenD(gRuta, "wb");
            if (f) {
                fprintf(f, "G %ld %ld %ld\n", gW, gH, gPPP);  // contexto de origen
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
                    if (ccG(s, CI_AGL) || ccG(s, CI_RAD) || ccG(s, CI_ARC)
                        || ccG(s, CI_EST))
                        fprintf(f, "P %ld %ld %ld %ld %ld\n", p,
                            ccG(s, CI_AGL), ccG(s, CI_RAD), ccG(s, CI_ARC),
                            ccG(s, CI_EST));     // polar + estado: linea extendida
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
        } else if (o->k == 9) {                  // clonar rama (deep copy)
            Ser* t = o->t;
            if (t && t->tc != 0 && t->tc != 'r' && t->fol) {
                Ser* c2 = clonaSer(t, t->fol);
                if (c2) {
                    ccS(c2, CI_X, ccG(t, CI_X) + 30 * QZ);
                    ccS(c2, CI_Y, ccG(t, CI_Y) + 26 * QZ);
                    lgE(c2, -3, 0, 0);           // crear: U lo quita entero
                    if (gObj && gObj->tc != 0 && gObj != c2)
                        ccS(gObj, CI_SEL, 0);
                    gAnt = gSel; gSel = c2; gObj = c2;
                    syncInsp();
                }
            }
        } else if (o->k == 10) {                  // exportar lo pintado: PPM
            FILE* f = fopenD("ser_exp.ppm", "wb");
            if (f) {
                fprintf(f, "P6\n%ld %ld\n255\n", gW, gH);
                for (long i = 0; i < gW * gH; i++) {
                    unsigned char r4 = (unsigned char)((gImg[i] >> 16) & 255);
                    unsigned char g4 = (unsigned char)((gImg[i] >> 8) & 255);
                    unsigned char b4 = (unsigned char)(gImg[i] & 255);
                    fwrite(&r4, 1, 1, f); fwrite(&g4, 1, 1, f);
                    fwrite(&b4, 1, 1, f);
                }
                fclose(f);
            }
        } else if (o->k == 11) {                  // exportar lo pintado: PNG
            pngGraba("ser_exp.png", gImg, gW, gH);
        } else if (o->k == 12) {                  // documento de escena legible
            docEscena("ser_doc.txt");
        }
        caSuma(CA_ORD);
        oTail = oTail + 1;
    }
    for (long z = 0; z < 256; z++) oRing[z].tx = 0;   // tras drenar todo
}
// ---- [I] interaccion (mis para todos) ---------------------------------------

static long esDe(Ser* ap, Ser* t) {      // ap vive dentro de la rama t
    for (; ap; ap = ap->fol) if (ap == t) return 1;
    return 0;
}
static void borraApuntados(Ser* t) {     // ser colgado en la rama borrada
    Ser** ap[20] = { &gObj, &gSel, &gAnt, &gInsp, &gSes, &gForm, &gCamNodo,
                     &gColNodo, &gColPnl, &gInspEd, &gInspVis, &gInspPn, &gInspR,
                     &gInspG, &gInspB, &gInspRad, &gAyTxt,
                     &gMenu, &gInsp2, &gFilTxt };
    for (long i = 0; i < 20; i++) if (esDe(*ap[i], t)) *ap[i] = 0;
    gFilLast = 0;                        // el foco nuevo reordena fila 0
    if (!gObj) gEdito = 0;
}
static double* filaDe(Ser* t, long idx, long* nfOut) {   // fila SM elegida
    long nf = 0;
    double* hit = 0;
    if (t)
        for (long m = 0; m < t->nM; m++) {
            if (!t->M[m] || !t->TM[m] || t->TM[m][0] != 'f') continue;
            if (!hit && nf == idx) hit = (double*)t->M[m];
            nf++;
        }
    if (nfOut) *nfOut = nf;
    return hit;
}
static void lgFila(Ser* t, double* f) {  // entra la fila al log antes de tocarla
    lgE(t, -5, 0, 0);
    struct Lg* e = &gLg[gLgN - 1];
    for (long i = 0; i < 6; i++) e->row[i] = f[i];
    e->olb = (char*)f;                   // solo a los fines de validar vigencia
}
static Ser* clonaHasta(Ser* t, Ser* fol, long nivel) {   // copia profunda
    if (!t || t->tc == 0 || nivel > 8) return 0;
    char n2c[3]; nvoN2(n2c, t->tc);
    char lbc[64];
    long nl = 0;
    if (t->lb) while (t->lb[nl] && nl < 60) { lbc[nl] = t->lb[nl]; nl++; }
    lbc[nl] = 0;
    if (nl < 59) { lbc[nl] = '2'; lbc[nl + 1] = 0; }
    Ser* s = serCr(fol, t->tc, n2c, lbc);
    for (long i = 0; i < CI_MAX; i++) s->C[i] = t->C[i];
    s->ulti = t->ulti;
    for (long m = 0; m < t->nM; m++) {
        if (!t->M[m] || !t->TM[m]) continue;
        double* r = (double*)t->M[m];
        if (t->TM[m][0] == 'f' && t->TM[m][1] == '6')
            f6Add(s, r[0], r[1], r[2], r[3], r[4], r[5]);
        else if (t->TM[m][0] == 'n' && t->TM[m][1] == '9')
            n9Add(s, r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8]);
    }
    for (long i = 0; i < t->nA && i < 256; i++) if (t->A[i])
        clonaHasta(t->A[i], s, nivel + 1);
    return s;
}
static Ser* clonaSer(Ser* t, Ser* fol) { return clonaHasta(t, fol, 0); }

static void inspAplica(Ser* sl) {        // slider ligado -> objetivo elegido
    if (!sl) return;                     // cualquier slider con CI_EXT actua igual
    long ch = ccG(sl, CI_EXT);
    long vr = ccG(sl, CI_VAL);
    if (ch == 100 || ch == 101 || ch == 102) {   // pesos del colador
        if (ch == 100) gColAnti = vr;
        else if (ch == 101) gColImp = vr;
        else gColUrg = vr;
        colador();                       // nueva mirada: se ve al toque
        return;
    }
    if (!gObj) return;
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
static void syncDrw() {                  // panel filas: texto vivo de la fila
    if (!gFilTxt || !gFilTxt->lb || (gEdito && gObj == gFilTxt)) return;
    if (gObj != gFilLast) { gFilI = 0; gFilLast = gObj; }
    long nf = 0;
    double* f = filaDe(gObj, gFilI, &nf);
    char sc[160];
    if (gObj && f) {
        static const char* NC[14] = { "X", "Y", "W", "H", "R", "G", "B",
                                      "A", "AB", "FLG", "VAL", "RED",
                                      "EXT", "SEL" };
        static const char* OP[6] = { "SET", "ADD", "XOR", "CLI", "MUL",
                                     "CRB" };
        static const char* CN[4] = { "SI", "<", ">", "=" };
        char cci[12], cdst[24];
        long ci2 = (long)f[1];
        snprintf(cci, sizeof(cci), "%s", (ci2 >= 0 && ci2 < CI_MAX) ? NC[ci2]
                                                                   : "?");
        long d2 = (long)f[4];
        if (d2 >= 0)
            snprintf(cdst, sizeof(cdst), "%s",
                     (d2 < CI_MAX) ? NC[d2] : "?");
        else
            snprintf(cdst, sizeof(cdst), "fol.%s",
                     (-d2 - 1 >= 0 && -d2 - 1 < CI_MAX) ? NC[-d2 - 1] : "?");
        snprintf(sc, sizeof(sc), "FILA %ld/%ld  %s  CI=%s  O=%s  DST=%s  ARG=%ld",
                 (nf > 0) ? (gFilI % nf) + 1 : 0, nf,
                 CN[(long)f[0] & 3], cci, OP[(long)f[3] % 6], cdst, (long)f[5]);
    } else {
        snprintf(sc, sizeof(sc), "FILA -/-  (el nodo con foco no tiene filas)");
    }
    if (strcmp(sc, gFilTxt->lb) == 0) return;
    snprintf(gFilTxt->lb, 195, "%s", sc);
    gFilTxt->vr++; gGen++;
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

static long fsmPisa(Ser* m, float mx, float my) {
    if (!m || m->nA <= 0 || g3D) return 0;
    long ex = 0, ey = 0; posEf(m, &ex, &ey);
    float sx = (float)ex / QZ, sy = (float)ey / QZ;
    for (Ser* p = m->fol; p && p != gRoot; p = p->fol) {
        long pex = 0, pey = 0; posEf(p, &pex, &pey);
        sx += (float)pex / QZ; sy += (float)pey / QZ;
    }
    Ser* r = m;                          // camara: la aplica la rama tope
    while (r->fol && r->fol != gRoot) r = r->fol;
    if (!(ccG(r, CI_FLG) & 8)) { sx -= (float)gCamX / QZ; sy -= (float)gCamY / QZ; }
    float w2 = (float)ccG(m, CI_W) / QZ, h2 = (float)ccG(m, CI_H) / QZ;
    float lx = mx - sx, ly = my - sy;
    if (lx < 0 || ly < 0 || lx > w2 || ly > h2) return 0;
    float cwt = 0, chp = 0, fyA = 0, rail = 0;
    fsmBand(w2, m->nA, &cwt, &chp, &fyA, &rail);
    if (ly >= fyA - 2 && ly <= fyA + chp + 2) {
        long i = (long)((lx - 4) / cwt);         // un chip: saltar ahi
        if (i < 0 || i >= m->nA || !m->A[i]) return 0;
        lgEmp();
        lgE(m, CI_EST, ccG(m, CI_EST), 0);       // el salto es deshacible
        ccS(m, CI_EST, i);
        gAnt = gSel; gSel = m->A[i];
        if (gObj && gObj->tc != 0 && gObj != m->A[i]) ccS(gObj, CI_SEL, 0);
        gObj = m->A[i]; gFilI = 0; gFilLast = gObj;
        syncInsp();
        return 1;                                // sin arrastre: el chip manda
    }
    if (ly >= fyA + chp && ly <= rail + 4) {
        for (long i = 0; i < m->nA; i++) {       // un riel: su fila op 11
            Ser* es = m->A[i];
            if (!es) continue;
            long k = 0;
            for (long rj = 0; rj < es->nM; rj++) {
                if (!es->M[rj] || !es->TM[rj] || es->TM[rj][0] != 'f') continue;
                double* r6 = (double*)es->M[rj];
                long filaIdx = k++;
                if ((long)r6[3] != 11) continue;
                long nx = (long)r6[4];
                if (nx < 0 || nx >= m->nA || nx == i) continue;
                float x0d = 4 + i * cwt + (cwt - 2) * 0.5f;
                float x1d = 4 + nx * cwt + (cwt - 2) * 0.5f;
                if (x1d < x0d) { float tt = x0d; x0d = x1d; x1d = tt; }
                if (lx >= x0d - 2 && lx <= x1d + 2) {
                    gAnt = gSel; gSel = es;
                    if (gObj && gObj->tc != 0 && gObj != es) ccS(gObj, CI_SEL, 0);
                    gObj = es; gFilI = filaIdx; gFilLast = gObj;
                    syncInsp();
                    return 1;                    // el editor de filas la sigue
                }
            }
        }
        return 0;
    }
    return 0;
}
static void pisaDerecho(float mx, float my) {    // boton derecho: menu
    if (g3D) return;
    if (gMenu && (ccG(gMenu, CI_FLG) & 1) &&
        mx >= (float)ccG(gMenu, CI_X) / QZ &&
        mx <= (float)(ccG(gMenu, CI_X) + ccG(gMenu, CI_W)) / QZ &&
        my >= (float)ccG(gMenu, CI_Y) / QZ &&
        my <= (float)(ccG(gMenu, CI_Y) + ccG(gMenu, CI_H)) / QZ) {
        ccS(gMenu, CI_FLG, 0);           // abierto encima: se cierra
        return;
    }
    if (!gMenu) return;
    lgEmp();
    Ser* s = elige(mx, my);
    if (s && !esDe(s, gInsp) && !esDe(s, gMenu) && !esDe(s, gInsp2)) {
        if (gObj && gObj->tc != 0 && gObj != s) ccS(gObj, CI_SEL, 0);
        gAnt = gSel; gSel = s; gObj = s; gFilI = 0;
        syncInsp();
    }
    float mw = (float)ccG(gMenu, CI_W) / QZ, mh = (float)ccG(gMenu, CI_H) / QZ;
    float nx = mx + 2, ny = my + 2;
    if (nx + mw > (float)gW) nx = (float)gW - mw;
    if (ny + mh > (float)gH) ny = (float)gH - mh;
    ccS(gMenu, CI_X, (long)(nx < 0 ? 0 : nx) * QZ);
    ccS(gMenu, CI_Y, (long)(ny < 0 ? 0 : ny) * QZ);
    ccS(gMenu, CI_FLG, ccG(gMenu, CI_FLG) | 1);
}
static void pisaDoble(float mx, float my) {      // doble clic: fijar (pin)
    if (g3D) return;
    Ser* s = elige(mx, my);
    if (!s || s->tc == 'r') return;
    lgEmp();
    lgE(s, CI_FLG, ccG(s, CI_FLG), 0);
    ccS(s, CI_FLG, ccG(s, CI_FLG) ^ 8);
    mandaAlFrente(s);
    gAnt = gSel; gSel = s;
    if (gObj && gObj->tc != 0 && gObj != s) ccS(gObj, CI_SEL, 0);
    gObj = s; gFilI = 0;
    syncInsp();
    if (gMenu) ccS(gMenu, CI_FLG, 0);
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
    if (s->tc == 'f' && fsmPisa(s, mx, my)) return;   // diagrama: chips y rieles
    ratonMundo(s, mx, my, &mx, &my);         // pin: pantalla / resto: mundo
    lgEmp();                                 // un clic = un grupo de deshacer
    gAnt = gSel;
    gSel = s;
    long enI = 0;
    for (Ser* p = s; p; p = p->fol)
        if (p == gInsp || p == gMenu || p == gInsp2) { enI = 1; break; }
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
    if (s->tc == 'p' && s != gMenu) {    // el menu no colapsa con el clic
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
        else if (v == 6 && gObj && gObj->tc != 0) {          // menu: fijar
            lgE(gObj, CI_FLG, ccG(gObj, CI_FLG), 0);
            ccS(gObj, CI_FLG, ccG(gObj, CI_FLG) ^ 8);
        }
        else if (v == 7 && gObj && gObj->tc != 0)            // menu: al frente
            ordP(3, gObj, 0, 0, 0, 0, 0, 0);
        else if (v == 8 && gObj && gObj->tc != 0)            // menu: clonar
            ordP(9, gObj, 0, 0, 0, 0, 0, 0);
        else if (v == 9 && gObj && gObj->tc != 0)            // menu: borrar
            ordP(2, gObj, 0, 0, 0, 0, 0, 0);
        else if (v == 10 && gInsp2)                          // menu: filas si/no
            ccS(gInsp2, CI_FLG, (ccG(gInsp2, CI_FLG) & 1)
                               ? (ccG(gInsp2, CI_FLG) & ~1)
                               : (ccG(gInsp2, CI_FLG) | 1));
        else if (v == 19) ordP(10, 0, 0, 0, 0, 0, 0, 0);     // menu: EXPORT
        else if (v == 20) ordP(11, 0, 0, 0, 0, 0, 0, 0);     // menu: PNG
        else if (v == 21) ordP(12, 0, 0, 0, 0, 0, 0, 0);     // menu: DOC
        else if (v == 18 && gMenu)                           // menu: cerrar
            ccS(gMenu, CI_FLG, 0);
        else if (v >= 11 && v <= 17 && gObj && gObj->tc != 0) {
            long nf = 0;
            double* f = filaDe(gObj, gFilI, &nf);
            if (v == 11) {                           // FILA: cicla la elegida
                if (nf > 0) gFilI = (gFilI + 1) % nf;
            } else if (f) {
                lgFila(gObj, f);                     // U repone la fila entera
                if (v == 12) f[0] = (double)(((long)f[0] + 1) & 3);
                else if (v == 13) f[1] = (double)(((long)f[1] + 1) % CI_MAX);
                else if (v == 14) f[3] = (double)(((long)f[3] + 1) % 6);
                else if (v == 15) {                  // DST: propio -> fol -> propio
                    long d2 = (long)f[4];
                    if (d2 >= 0) d2++; else d2--;
                    if (d2 > CI_MAX - 1) d2 = -1;
                    if (d2 < -(long)CI_MAX) d2 = 0;
                    f[4] = (double)d2;
                }
                else if (v == 16) f[5] += (double)64;
                else if (v == 17) f[5] -= (double)64;
                if (v == 16 || v == 17) exRows(gObj);
                else { gObj->vr++; gGen++; }
            }
        }
        if (gMenu && s != gMenu && esDe(s, gMenu))   // accion -> cierra el menu
            ccS(gMenu, CI_FLG, 0);
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
    if (k == 'E') { ordP(10, 0, 0, 0, 0, 0, 0, 0); return; }  // exportar PPM
    if (k == 'I') { ordP(11, 0, 0, 0, 0, 0, 0, 0); return; }  // exportar PNG
    if (k == 'O') { ordP(12, 0, 0, 0, 0, 0, 0, 0); return; }  // documento de escena
    if (k == 'U') { deshacer(); return; }          // deshacer: ultimo grupo
    if (k == 'H') { vidaDeshacer(); return; }      // H: deshacer ultima vida
    if (k == 'K') {                                // clon del objetivo
        if (gObj) ordP(9, gObj, 0, 0, 0, 0, 0, 0);
        return;
    }
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
    FILE* f = fopenD(ruta, "rb");
    if (!f) return 0;
    if (!gRoot) desEscenaRoot();
    char lin[512];
    Ser** prev = (Ser**)xM(1024 * sizeof(Ser*));
    for (long i = 0; i < 1024; i++) prev[i] = 0;
    long np = 0;
    double efx = 1, efy = 1;               // rescalado: contexto de origen vs actual
    while (fgets(lin, sizeof(lin), f)) {
        if (lin[0] == 'G') {                     // contexto grafico en que se guardo
            float gw = 0, gh = 0;
            leeNum(lin, 1, &gw); leeNum(lin, 2, &gh);
            if (gw > 16 && gh > 16 && gW > 16 && gH > 16) {
                efx = (double)gW / gw; efy = (double)gH / gh;
                if (efx < 0.25) efx = 0.25;
                if (efx > 4) efx = 4;
                if (efy < 0.25) efy = 0.25;
                if (efy > 4) efy = 4;
                if (efx > 0.98 && efx < 1.02) efx = 1;
                if (efy > 0.98 && efy < 1.02) efy = 1;
            }
        } else if (lin[0] == 'N') {
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
            if (s != gRoot || (fol >= 0)) {      // rescalado salvo la raiz viva
                ccS(s, CI_X, (long)((double)x * efx)); ccS(s, CI_Y, (long)((double)y * efy));
                ccS(s, CI_W, (long)((double)w * efx)); ccS(s, CI_H, (long)((double)h * efy));
            } else {                             // raiz: el archivo ya es punto fijo
                ccS(s, CI_X, (long)x); ccS(s, CI_Y, (long)y);
            }
            if (s != gRoot) {
                ccS(s, CI_W, (long)((double)w * efx)); ccS(s, CI_H, (long)((double)h * efy));
            } else {                             // raiz: tamano de la ventana viva
                ccS(s, CI_W, gW * QZ); ccS(s, CI_H, gH * QZ);
            }
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
        } else if (lin[0] == 'P') {              // polar + estado de maquina persistidos
            float idx = 0, ag = 0, rd = 0, ac = 0, es = 0;
            leeNum(lin, 1, &idx); leeNum(lin, 2, &ag); leeNum(lin, 3, &rd);
            leeNum(lin, 4, &ac);  leeNum(lin, 5, &es);
            if (idx >= 0 && idx < np) {
                Ser* s = prev[(long)idx];
                if (ag) ccS(s, CI_AGL, (long)ag);
                if (rd) ccS(s, CI_RAD, (long)rd);
                if (ac) ccS(s, CI_ARC, (long)ac);
                if (es) ccS(s, CI_EST, (long)es);
            }
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
static void creaColador() {              // sliders de los pesos del colador
    if (gColPnl || !gRoot) return;
    gColPnl = serCr(gRoot, 'p', "pCo", "Colador");
    ccS(gColPnl, CI_X, 640 * QZ); ccS(gColPnl, CI_Y, 262 * QZ);
    ccS(gColPnl, CI_W, 150 * QZ); ccS(gColPnl, CI_H, 104 * QZ);
    ccS(gColPnl, CI_R, 0.1f * QZ); ccS(gColPnl, CI_G, 0.12f * QZ);
    ccS(gColPnl, CI_B, 0.16f * QZ);
    ccS(gColPnl, CI_A, QZ); ccS(gColPnl, CI_FLG, 1); ccS(gColPnl, CI_ABI, 1);
    ccS(gColPnl, CI_RED, 8);
    static const char* NB[3] = { "ANTI", "IMP", "URG" };
    long vivos[3] = { (long)gColAnti, (long)gColImp, (long)gColUrg };
    for (long i = 0; i < 3; i++) {
        Ser* s2 = serCr(gColPnl, 's', "wC", NB[i]);
        ccS(s2, CI_X, 6 * QZ); ccS(s2, CI_Y, (20 + i * 24) * QZ);
        ccS(s2, CI_W, 138 * QZ); ccS(s2, CI_H, 16 * QZ);
        ccS(s2, CI_R, 0.2f * QZ); ccS(s2, CI_G, 0.7f * QZ); ccS(s2, CI_B, 0.5f * QZ);
        ccS(s2, CI_A, QZ); ccS(s2, CI_FLG, 1 | 4); ccS(s2, CI_RED, 6);
        ccS(s2, CI_EXT, (long)(100 + i));
        ccS(s2, CI_VAL, vivos[i]);       // el slider nace con el peso real
    }
    Ser* h3 = serCr(gColPnl, 'l', "wH", "efectos anti/imp/urg");
    ccS(h3, CI_X, 6 * QZ); ccS(h3, CI_Y, 90 * QZ);
    ccS(h3, CI_R, 0.8f * QZ); ccS(h3, CI_G, 0.8f * QZ); ccS(h3, CI_B, 0.8f * QZ);
    ccS(h3, CI_A, QZ); ccS(h3, CI_FLG, 1);
}
static Ser* esteMeta(const char* lb) {   // panel de esquema que volvio del archivo
    if (!gRoot) return 0;
    for (long i = 0; i < gRoot->nA; i++) {
        Ser* q = gRoot->A[i];
        if (q && q->tc == 'p' && q->lb && strcmp(q->lb, lb) == 0) return q;
    }
    return 0;
}
static void linkeaMeta() {               // re-vincula menu e inspector si el
    Ser* m = esteMeta("Menu");           // archivo DEL QUE VENIMOS ya los tenia:
    if (m) gMenu = m;                    // sin esto quedan duplicados al cargar
    Ser* pc = esteMeta("Colador");
    if (pc) gColPnl = pc;
    Ser* r = esteMeta("Filas SM");
    if (r) {
        gInsp2 = r;
        gFilTxt = 0;
        for (long i = 0; i < r->nA; i++)
            if (r->A[i] && r->A[i]->tc == 'l' && r->A[i]->lb) {
                gFilTxt = r->A[i];
                if (gFilTxt->lb) xMe(gFilTxt->lb);   // syncDrw escribe hasta 195
                gFilTxt->lb = (char*)xM(200);
                gFilTxt->lb[0] = 0;
                break;
            }
        if (!gFilTxt) gInsp2 = 0;
    }
    if (gMenu) ccS(gMenu, CI_FLG, ccG(gMenu, CI_FLG) & ~1);   // arranca cerrado
}
static void creaMenu() {                 // menu contextual: acciones reales
    if (gMenu || !gRoot) return;
    gMenu = serCr(gRoot, 'p', "pM2", "Menu");
    ccS(gMenu, CI_X, (gW - 140) * QZ); ccS(gMenu, CI_Y, (gH - 210) * QZ);
    ccS(gMenu, CI_W, 130 * QZ); ccS(gMenu, CI_H, 206 * QZ);
    ccS(gMenu, CI_R, 0.12f * QZ); ccS(gMenu, CI_G, 0.14f * QZ);
    ccS(gMenu, CI_B, 0.22f * QZ);
    ccS(gMenu, CI_A, QZ); ccS(gMenu, CI_FLG, 0); ccS(gMenu, CI_RED, 8);
    ccS(gMenu, CI_ABI, 1);               // abierto: sin esto el pick no baja

    static const char* NB[8] = { "FIX", "FRENTE", "CLON", "BORRAR", "FILAS",
                                 "EXPORT", "PNG", "DOC" };
    for (long i = 0; i < 8; i++) {
        Ser* e = serCr(gMenu, 'e', "mE", NB[i]);
        ccS(e, CI_X, 6 * QZ); ccS(e, CI_Y, (20 + i * 23) * QZ);
        ccS(e, CI_W, 118 * QZ); ccS(e, CI_H, 19 * QZ);
        ccS(e, CI_R, 0.2f * QZ); ccS(e, CI_G, 0.4f * QZ); ccS(e, CI_B, 0.6f * QZ);
        ccS(e, CI_A, QZ); ccS(e, CI_FLG, 1 | 4); ccS(e, CI_RED, 6);
        long vl = i <= 4 ? 6 + i : (i == 5 ? 19 : (i == 6 ? 20 : 21));
        ccS(e, CI_VAL, vl);              // 19: PPM (filas=11..17) 20 png 21 doc
    }
}
static void creaFilas() {                // inspector de filas SM: editables
    if (gInsp2 || !gRoot) return;
    gInsp2 = serCr(gRoot, 'p', "pF2", "Filas SM");
    ccS(gInsp2, CI_X, 16 * QZ); ccS(gInsp2, CI_Y, 372 * QZ);
    ccS(gInsp2, CI_W, 260 * QZ); ccS(gInsp2, CI_H, 100 * QZ);
    ccS(gInsp2, CI_R, 0.13f * QZ); ccS(gInsp2, CI_G, 0.11f * QZ);
    ccS(gInsp2, CI_B, 0.19f * QZ);
    ccS(gInsp2, CI_A, QZ); ccS(gInsp2, CI_FLG, 1 | 8); ccS(gInsp2, CI_ABI, 1);
    ccS(gInsp2, CI_RED, 8);
    gFilTxt = serCr(gInsp2, 'l', "fT", "FILA -/-");
    if (gFilTxt->lb) xMe(gFilTxt->lb);            // syncDrw escribe hasta 195:
    gFilTxt->lb = (char*)xM(200); gFilTxt->lb[0] = 0;   // no basta el xSt del ser
    ccS(gFilTxt, CI_X, 6 * QZ); ccS(gFilTxt, CI_Y, 20 * QZ);
    ccS(gFilTxt, CI_R, 0.9f * QZ); ccS(gFilTxt, CI_G, 0.85f * QZ);
    ccS(gFilTxt, CI_B, 0.6f * QZ); ccS(gFilTxt, CI_A, QZ); ccS(gFilTxt, CI_FLG, 1);
    static const char* NB[7] = { "FILA", "COND", "CI", "OP", "DST", "MAS", "MENOS" };
    for (long i = 0; i < 7; i++) {
        Ser* e = serCr(gInsp2, 'e', "fE", NB[i]);
        ccS(e, CI_X, (4 + i * 35) * QZ); ccS(e, CI_Y, 56 * QZ);
        ccS(e, CI_W, 33 * QZ); ccS(e, CI_H, 18 * QZ);
        ccS(e, CI_R, 0.2f * QZ); ccS(e, CI_G, 0.5f * QZ); ccS(e, CI_B, 0.4f * QZ);
        ccS(e, CI_A, QZ); ccS(e, CI_FLG, 1 | 4); ccS(e, CI_RED, 6);
        ccS(e, CI_VAL, (long)(11 + i));
    }
    Ser* h2 = serCr(gInsp2, 'l', "fH", "clic nodo: clica aca y edita su fila");
    ccS(h2, CI_X, 6 * QZ); ccS(h2, CI_Y, 78 * QZ);
    ccS(h2, CI_R, QZ); ccS(h2, CI_G, QZ); ccS(h2, CI_B, QZ);
    ccS(h2, CI_A, QZ); ccS(h2, CI_FLG, 1);
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
        "FLEC:camara  U:deshacer todo  H:deshacer vida  L:etiqueta  "
        "V-P-D-E en menu derecho  K:clonar  G:3D  DBL:fija  "
        "F/B/Z/X:orden z  E/I:export PPM/PNG  O:doc");
    ccS(ay2, CI_X, 6 * QZ); ccS(ay2, CI_Y, 19 * QZ);
    ccS(ay2, CI_R, 0.9f * QZ); ccS(ay2, CI_G, 0.9f * QZ); ccS(ay2, CI_B, 0.95f * QZ);
    ccS(ay2, CI_A, QZ); ccS(ay2, CI_FLG, 1);
    creaMenu();
    creaFilas();
    creaColador();
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
    FILE* f = fopenD(gRuta, "rb");
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
    ccS(cam, CI_X, 280 * QZ); ccS(cam, CI_Y, 460 * QZ);  // entre paneles fijos
    ccS(cam, CI_W, 40 * QZ); ccS(cam, CI_H, 20 * QZ);
    ccS(cam, CI_FLG, 1);
    unsigned int fr[8];                        // frame 4x2: azul arriba rojo abajo
    for (long i = 0; i < 4; i++) fr[i] = 0xff0000ffu;
    for (long i = 4; i < 8; i++) fr[i] = 0xffff0000u;
    ordFeed(cam, fr, 4, 2); escritor();
    pintaTodo();
    unsigned int cv = gImg[470 * gW + 300];
    printf("camara muestra frame: %s\n",
           ((cv >> 16) & 255) == 255 ? "PASA" : "FALLA");
    gRq = 0; pintaTodo();
    printf("cache camara sin cambios: %s\n", gRq == 0 ? "PASA" : "FALLA");
    for (long i = 0; i < 8; i++) fr[i] = 0xff00ff00u;   // todo verde
    ordFeed(cam, fr, 4, 2); escritor(); pintaTodo();
    unsigned int cv2 = gImg[470 * gW + 300];
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
        long pz2 = (long)(2.0f + (70.0f - 40.0f) * fs);
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
    {   // colador a medida (nido camara): la camara pide su resolucion
        Ser* rv = gRoot; gRoot = 0;
        Ser* rr = serCr(0, 'r', "cR", "ColMed");
        gRoot = rr;
        Ser* cA = serCr(rr, 'r', "cA", "rojo");
        ccS(cA, CI_X, 30 * QZ); ccS(cA, CI_Y, 40 * QZ);
        ccS(cA, CI_W, 90 * QZ); ccS(cA, CI_H, 50 * QZ);
        ccS(cA, CI_R, QZ); ccS(cA, CI_A, QZ); ccS(cA, CI_FLG, 1);
        Ser* cB = serCr(rr, 'r', "cB", "verde");
        ccS(cB, CI_X, 90 * QZ); ccS(cB, CI_Y, 40 * QZ);
        ccS(cB, CI_W, 90 * QZ); ccS(cB, CI_H, 50 * QZ);
        ccS(cB, CI_G, QZ); ccS(cB, CI_A, QZ); ccS(cB, CI_FLG, 1);
        unsigned int* cb = (unsigned int*)xM((unsigned long)(180 * 120)
                                             * sizeof(unsigned int));
        coladorPinta(cb, 180, 120);
        float f1 = 176.0f / (150.0f * QZ), f2 = 116.0f / (50.0f * QZ);
        if (f2 < f1) f1 = f2;              // fit del colador a 180x120
        long gy = (long)(2.0f + (80.0f - 40.0f) * (float)QZ * f1);
        long gp = (long)(2.0f + (150.0f - 30.0f) * (float)QZ * f1);
        long rp = (long)(2.0f + (60.0f - 30.0f) * (float)QZ * f1);
        long gc = cb[gy * 180 + gp], rc = cb[gy * 180 + rp];
        long fz = cb[100 * 180 + 5] == 0;  // rincon sin nodos: fondo limpio
        printf("colador a medida (nido camara): %s (G=%08x R=%08x fz=%ld)\n",
               (gc == pr(0, 1, 0, 1) && rc == pr(1, 0, 0, 1) && fz)
               ? "PASA" : "FALLA", (unsigned)gc, (unsigned)rc, fz);
        xMe((void*)cb);
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
        long wr = 0;                     // cable nodo->padre de "Rojo": op 0
        unsigned int wcol = pr(0.45f, 0.7f, 1.0f, 1.0f);
        for (unsigned long i = 0; i < (unsigned long)gW * (unsigned long)gH; i++)
            if (gImg[i] == wcol) wr++;
        printf("arco al fol se ve en 3D: %s (%ld px)\n",
               wr > 8 ? "PASA" : "FALLA", wr);
        g3D = 0; gImgGen = -1;
        pintaTodo();
    }
    {   // semiarco 3D: banda curva con y sin textura
        Ser* rv = gRoot; gRoot = 0;
        Ser* rr = serCr(0, 'r', "cR", "Arcos");
        gRoot = rr;
        Ser* a1 = serCr(rr, 'o', "a1", "Arco");
        ccS(a1, CI_X, 30 * QZ); ccS(a1, CI_Y, 30 * QZ);
        ccS(a1, CI_W, 320 * QZ); ccS(a1, CI_H, 30 * QZ);
        ccS(a1, CI_R, QZ); ccS(a1, CI_A, QZ); ccS(a1, CI_FLG, 1);
        ccS(a1, CI_AGL, 0); ccS(a1, CI_ARC, 90 * QZ);   // cuarto de anillo
        unsigned int fnd = pr(0.05f, 0.06f, 0.09f, 1.0f);
        g3D = 1; gImgGen = -1;
        pintaTodo();                       // banda de color puro (sin textura)
        long nzA = 0;
        for (unsigned long i = 0; i < (unsigned long)gW * (unsigned long)gH; i++)
            if (gImg[i] != fnd) nzA++;
        long long* c4 = camRow(a1, 32, 8); // textura: azul en todo el ancho
        for (long i = 0; i < 32 * 8; i++)
            ((unsigned int*)c4[0])[i] = pr(0.1f, 0.2f, 0.9f, 1.0f);
        c4[3] = 5;
        gImgGen = -1;
        pintaTodo();                       // banda texturizada (azul sombreado)
        long azul = 0;
        for (unsigned long i = 0; i < (unsigned long)gW * (unsigned long)gH; i++) {
            unsigned int px = gImg[i];
            long b1 = (long)(px & 255), g1 = (long)((px >> 8) & 255);
            long r1 = (long)((px >> 16) & 255);
            if (b1 > g1 && b1 > r1 && b1 > 120) azul++;
        }
        printf("semiarco 3D pinta la banda: %s (%ld px banda, %ld azules)\n",
               (nzA > 400 && azul > 60) ? "PASA" : "FALLA", nzA, azul);
        ccS(a1, CI_ARC, 0);                // misma geometria como caja:
        gImgGen = -1;                      // color puro, ni un azul texturizado
        pintaTodo();
        long nzC = 0, azulC = 0;
        for (unsigned long i = 0; i < (unsigned long)gW * (unsigned long)gH; i++) {
            unsigned int px = gImg[i];
            if (px != fnd) nzC++;
            long b1 = (long)(px & 255);
            if (b1 > (long)((px >> 8) & 255) && b1 > (long)((px >> 16) & 255)
                && b1 > 120) azulC++;
        }
        printf("semiarco vs caja (banda %ld px, caja %ld px, azules banda %ld / caja %ld): %s\n",
               nzA, nzC, azul, azulC,
               (nzC > 400 && azulC * 4 < azul) ? "PASA" : "FALLA");
        g3D = 0; gImgGen = -1;
        pintaTodo();                       // escena original de vuelta
        serAl(rr);
        gRoot = rv;
        gImgGen = -1;
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
    FILE* q = fopenD(gRuta, "rb");
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
        gSes = 0; gForm = 0; gCamNodo = 0; gColNodo = 0; gColPnl = 0; gSel = 0; gAnt = 0; gArra = 0;
        gInsp = 0; gInspEd = 0; gInspVis = 0; gInspPn = 0;
        gInspR = 0; gInspG = 0; gInspB = 0; gInspRad = 0;
        gAyTxt = 0; gObj = 0;                        // sin nodos fantasma al recargar
        gMenu = 0; gInsp2 = 0; gFilTxt = 0; gFilI = 0; gFilLast = 0;
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
    {   // sesion: polar y estado de maquina sobreviven el guardado (linea P)
        Ser* mq = serCr(gRoot, 'f', "mq", "FSM test");
        ccS(mq, CI_W, 80 * QZ); ccS(mq, CI_H, 24 * QZ);
        ccS(mq, CI_AGL, 45 * QZ); ccS(mq, CI_RAD, 30 * QZ); ccS(mq, CI_ARC, 90 * QZ);
        Ser* e0 = serCr(mq, 'p', "e0", "cuenta");
        Ser* e1 = serCr(mq, 'p', "e1", "listo");
        f6Add(e0, 1, CI_VAL, 3, 11, 0, 1);       // VAL < 3: se queda; no: al 1
        f6Add(e1, 0, CI_EXT, 0, 0, CI_EXT, 7);
        f6Add(e1, 0, CI_VAL, 0, 11, 0, 0);       // siempre: vuelve al 0
        ccS(mq, CI_EST, 1);                      // justo en el estado 1
        ordP(4, 0, 0, 0, 0, 0, 0, 0); escritor();
        long v0 = gVivos;
        while (gRoot->nA > 0) {
            Ser* h = gRoot->A[gRoot->nA - 1];
            gRoot->nA--;
            serAl(h);
        }
        serAl(gRoot); gRoot = 0;
        gSes = 0; gForm = 0; gCamNodo = 0; gColNodo = 0; gColPnl = 0; gSel = 0; gAnt = 0; gArra = 0;
        gInsp = 0; gInspEd = 0; gInspVis = 0; gInspPn = 0;
        gInspR = 0; gInspG = 0; gInspB = 0; gInspRad = 0;
        gAyTxt = 0; gObj = 0; gMenu = 0; gInsp2 = 0; gFilTxt = 0; gFilI = 0; gFilLast = 0;
        long rr = desEscena(gRuta);
        Ser* mq2 = 0;
        for (long i = 0; gRoot && !mq2 && i < gRoot->nA; i++)
            if (gRoot->A[i]->tc == 'f') mq2 = gRoot->A[i];
        Ser* e1b = (mq2 && mq2->nA > 1) ? mq2->A[1] : 0;
        printf("polar+fsm persisten: %s (est %ld, agl %ld, rad %ld, arc %ld, filas %ld)\n",
               rr == v0 && mq2 && mq2->nA == 2 && e1b && e1b->nM == 2
               && ccG(mq2, CI_EST) == 1 && ccG(mq2, CI_AGL) == 45 * QZ
               && ccG(mq2, CI_RAD) == 30 * QZ && ccG(mq2, CI_ARC) == 90 * QZ
               && ccG(e1b, CI_EXT) == 0 ? "PASA" : "FALLA",
               mq2 ? ccG(mq2, CI_EST) : -2, mq2 ? ccG(mq2, CI_AGL) : -2,
               mq2 ? ccG(mq2, CI_RAD) : -2, mq2 ? ccG(mq2, CI_ARC) : -2,
               e1b ? e1b->nM : -1);
    }
    {   // fsm: op 11 salta entre estados y el estado vivo ejecuta sus filas
        Ser* m = serCr(gRoot, 'f', "fs", "FSM viva");
        ccS(m, CI_W, 80 * QZ); ccS(m, CI_H, 24 * QZ);
        Ser* a0 = serCr(m, 'p', "s0", "cuenta");
        Ser* a1 = serCr(m, 'p', "s1", "listo");
        f6Add(a0, 0, CI_VAL, 0, 1, CI_VAL, 1);   // por paso: VAL += 1
        f6Add(a0, 1, CI_VAL, 3, 11, 0, 1);       // VAL < 3: quedarse; no: saltar a 1
        f6Add(a1, 0, CI_VAL, 0, 0, CI_EXT, 7);   // EXT = 7 mientras vive el estado 1
        f6Add(a1, 0, CI_VAL, 0, 11, 0, 0);       // siempre: volver a 0
        long p1 = fsmPaso(m);
        long p2 = fsmPaso(m);
        long p3 = fsmPaso(m);
        long c3 = ccG(a0, CI_VAL);
        long p4 = fsmPaso(m);
        long x4 = ccG(a1, CI_EXT);
        long p5 = fsmPaso(m);
        printf("fsm salta entre estados: %s (p1 %ld p2 %ld, paso3 est %ld val %ld, "
               "paso4 est %ld ext %ld, paso5 est %ld)\n",
               p1 == 0 && p2 == 0 && p3 == 1 && c3 == 3 && p4 == 0 && x4 == 7
               && p5 == 1 ? "PASA" : "FALLA",
               p1, p2, p3, c3, p4, x4, p5);
        for (long i = 0; i < gRoot->nA; i++) if (gRoot->A[i] == m) {
            for (long j = i; j < gRoot->nA - 1; j++) gRoot->A[j] = gRoot->A[j + 1];
            gRoot->nA--;
            break;
        }
        serAl(m);                                // sin maquina corriendo en la escena
    }
    {   // ppp base: contexto grafico medido, no numeros fijos
        long q1 = cmQz(1.0) / QZ;
        long fs1 = fsDePpp(96), fs3 = fsDePpp(144), fs4 = fsDePpp(192);
        printf("ppp base: %s (ppp %ld, px/cm %ld, 1cm = %ld px, fs 96/144/192 = "
               "%ld/%ld/%ld)\n",
               gPPP == 96 && gPXcm == 38 && q1 == 38
               && fs1 == 2 && fs3 == 3 && fs4 == 4 ? "PASA" : "FALLA",
               gPPP, gPXcm, q1, fs1, fs3, fs4);
    }
    {   // diagrama de estados: franja de chips, el vivo en ambar
        Ser* rv = gRoot; gRoot = 0; desEscenaRoot();   // raiz limpia: solo el diagrama
        Ser* dm = serCr(gRoot, 'f', "dg", "Diagrama");
        ccS(dm, CI_W, 180 * QZ); ccS(dm, CI_H, 40 * QZ);
        ccS(dm, CI_X, 300 * QZ); ccS(dm, CI_Y, 200 * QZ);
        ccS(dm, CI_FLG, 1); ccS(dm, CI_R, QZ);
        for (long i = 0; i < 3; i++) {
            Ser* ei = serCr(dm, 'p', "ei", "st");
            f6Add(ei, 0, CI_VAL, 0, 11, (double)((i + 1) % 3), 0);  // i -> i+1 mod 3
        }
        gImgGen = -1; pintaTodo();
        long ambI = 0, ambD = 0;
        for (long yy = 205; yy < 245; yy++)          // el nodo vive en y 200..240
            for (long xx = 0; xx < gW; xx++)
                if (gImg[(unsigned long)yy * gW + xx] == pr(1, 0.75f, 0.15f, 1)) {
                    if (xx < 390) ambI++; else ambD++;
                }
        ccS(dm, CI_EST, 2);                      // el vivo pasa al chip de la derecha
        gImgGen = -1; pintaTodo();
        long ambI2 = 0, ambD2 = 0;
        for (long yy = 205; yy < 245; yy++)
            for (long xx = 0; xx < gW; xx++)
                if (gImg[(unsigned long)yy * gW + xx] == pr(1, 0.75f, 0.15f, 1)) {
                    if (xx < 390) ambI2++; else ambD2++;
                }
        printf("diagrama de estados: %s (ambar izq/der %ld/%ld -> %ld/%ld)\n",
               ambI > 0 && ambD == 0 && ambI2 == 0 && ambD2 > 0 ? "PASA" : "FALLA",
               ambI, ambD, ambI2, ambD2);
        for (long i = 0; i < gRoot->nA; i++) if (gRoot->A[i] == dm) {
            for (long j = i; j < gRoot->nA - 1; j++) gRoot->A[j] = gRoot->A[j + 1];
            gRoot->nA--;
            break;
        }
        serAl(dm);
        serAl(gRoot);                            // la raiz limpia muere entera
        gRoot = rv; gImgGen = -1;
    }
    {   // editor de saltos: un chip dispara el salto con vuelta atras
        Ser* rv = gRoot; gRoot = 0; desEscenaRoot();   // raiz limpia: solo la maquina
        Ser* dm = serCr(gRoot, 'f', "ed", "Editor");
        ccS(dm, CI_W, 180 * QZ); ccS(dm, CI_H, 40 * QZ);
        ccS(dm, CI_X, 300 * QZ); ccS(dm, CI_Y, 200 * QZ);
        ccS(dm, CI_FLG, 9);                      // pineada: la camara no aplica
        for (long i = 0; i < 3; i++) serCr(dm, 'p', "ei", "st");
        float cwt = 0, chp = 0, fyA = 0, rail = 0;
        fsmBand(180, dm->nA, &cwt, &chp, &fyA, &rail);
        long hit = fsmPisa(dm, 300 + 4 + cwt + cwt * 0.5f, 200 + fyA + chp * 0.5f);
        printf("chip salta la maquina: %s (hit %ld, est %ld, filI %ld)\n",
               hit == 1 && ccG(dm, CI_EST) == 1 && gObj == dm->A[1]
               && gSel == dm->A[1] && gFilLast == gObj && gFilI == 0
               ? "PASA" : "FALLA", hit, ccG(dm, CI_EST), gFilI);
        for (long i = 0; i < gRoot->nA; i++) if (gRoot->A[i] == dm) {
            for (long j = i; j < gRoot->nA - 1; j++) gRoot->A[j] = gRoot->A[j + 1];
            gRoot->nA--;
            break;
        }
        serAl(dm);
        serAl(gRoot);                            // la raiz limpia muere entera
        gRoot = rv; gImgGen = -1;
        gObj = 0; gSel = 0; gAnt = 0; gFilI = 0; gFilLast = 0;
    }
    {   // editor de saltos: el riel selecciona su fila op 11
        Ser* rv = gRoot; gRoot = 0; desEscenaRoot();   // raiz limpia: solo la maquina
        Ser* dm = serCr(gRoot, 'f', "ed", "Editor");
        ccS(dm, CI_W, 180 * QZ); ccS(dm, CI_H, 40 * QZ);
        ccS(dm, CI_X, 300 * QZ); ccS(dm, CI_Y, 200 * QZ);
        ccS(dm, CI_FLG, 9);                      // pineada: la camara no aplica
        for (long i = 0; i < 3; i++) {
            Ser* ei = serCr(dm, 'p', "ei", "st");
            f6Add(ei, 0, CI_VAL, 0, 11, (double)((i + 1) % 3), 0);  // i -> i+1 mod 3
        }
        float cwt = 0, chp = 0, fyA = 0, rail = 0;
        fsmBand(180, dm->nA, &cwt, &chp, &fyA, &rail);
        long hit = fsmPisa(dm, 360, 200 + rail + 1);   // riel entre el chip 0 y el 1
        printf("riel elige la fila: %s (hit %ld, filI %ld)\n",
               hit == 1 && gObj == dm->A[0] && gSel == dm->A[0]
               && gFilI == 0 && gFilLast == gObj ? "PASA" : "FALLA",
               hit, gFilI);
        for (long i = 0; i < gRoot->nA; i++) if (gRoot->A[i] == dm) {
            for (long j = i; j < gRoot->nA - 1; j++) gRoot->A[j] = gRoot->A[j + 1];
            gRoot->nA--;
            break;
        }
        serAl(dm);
        serAl(gRoot);                            // la raiz limpia muere entera
        gRoot = rv; gImgGen = -1;
        gObj = 0; gSel = 0; gAnt = 0; gFilI = 0; gFilLast = 0;
    }
    {   // rescalado automatico: cambio de contexto grafico, el layout sigue
        FILE* v = fopenD(gRuta, "wb");
        if (v) {
            fprintf(v, "G 400 260 96\n");
            fprintf(v, "N 0 -1 r Ro 0 0 102400 66560 0 0 0 256 0 9 \n");
            fprintf(v, "N 1 0 p Tst 4096 4096 25600 5120 0 0 0 256 1 9 0\n");
            fclose(v);
        }
        while (gRoot->nA > 0) {
            Ser* h = gRoot->A[gRoot->nA - 1];
            gRoot->nA--;
            serAl(h);
        }
        serAl(gRoot); gRoot = 0;
        gSes = 0; gForm = 0; gCamNodo = 0; gColNodo = 0; gColPnl = 0; gSel = 0; gAnt = 0; gArra = 0;
        gInsp = 0; gInspEd = 0; gInspVis = 0; gInspPn = 0;
        gInspR = 0; gInspG = 0; gInspB = 0; gInspRad = 0;
        gAyTxt = 0; gObj = 0; gMenu = 0; gInsp2 = 0; gFilTxt = 0; gFilI = 0; gFilLast = 0;
        long rr = desEscena(gRuta);
        Ser* tst = (gRoot && gRoot->nA > 0) ? gRoot->A[0] : 0;
        printf("rescalado del layout: %s (nodos %ld, hijo w %ld h %ld x %ld, "
               "raiz w %ld)\n",
               rr == 2 && tst && ccG(tst, CI_W) == 100 * QZ * 2
               && ccG(tst, CI_H) == 20 * QZ * 2 && ccG(tst, CI_X) == 16 * QZ * 2
               && gRoot && ccG(gRoot, CI_W) == gW * QZ ? "PASA" : "FALLA",
               rr, tst ? ccG(tst, CI_W) : -1, tst ? ccG(tst, CI_H) : -1,
               tst ? ccG(tst, CI_X) : -1,
               gRoot ? ccG(gRoot, CI_W) / QZ : -1);
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
    gSes = 0; gForm = 0; gCamNodo = 0; gColNodo = 0; gColPnl = 0; gSel = 0; gAnt = 0; gArra = 0;
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
        FILE* v = fopenD(gRuta, "wb");
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
        gSes = 0; gForm = 0; gCamNodo = 0; gColNodo = 0; gColPnl = 0; gSel = 0; gAnt = 0; gArra = 0;
        gInsp = 0; gInspEd = 0; gInspVis = 0; gInspPn = 0;
        gInspR = 0; gInspG = 0; gInspB = 0; gInspRad = 0;
        gAyTxt = 0; gObj = 0;
        desEscena(gRuta);
        printf("migracion de formato viejo: %s (x=%ld w=%ld val=%ld lb=%s)\n",
               (gRoot && ccG(gRoot, CI_X) == 4096 && ccG(gRoot, CI_W) == gW * QZ
                && ccG(gRoot, CI_VAL) == 555
                && gRoot->lb && strcmp(gRoot->lb, "Dmo") == 0) ? "PASA" : "FALLA",
               gRoot ? ccG(gRoot, CI_X) : -1, gRoot ? ccG(gRoot, CI_W) : -1,
               gRoot ? ccG(gRoot, CI_VAL) : -1,
               gRoot && gRoot->lb ? gRoot->lb : "(nil)");
    }
    {                                    // reconstruimos el demo completo:
        while (gRoot->nA > 0) {          // aca viven los 5 controles nuevos
            Ser* h = gRoot->A[gRoot->nA - 1];
            gRoot->nA--;
            serAl(h);
        }
        serAl(gRoot); gRoot = 0;
        gSes = 0; gForm = 0; gCamNodo = 0; gColNodo = 0; gColPnl = 0;
        gInsp = 0; gInspEd = 0; gInspVis = 0; gInspPn = 0;
        gInspR = 0; gInspG = 0; gInspB = 0; gInspRad = 0;
        gAyTxt = 0; gObj = 0; gSel = 0; gAnt = 0; gArra = 0; gEdito = 0;
        gMenu = 0; gInsp2 = 0; gFilTxt = 0; gFilI = 0; gFilLast = 0;
        escModelo();
    }
    {                                    // clon profundo (op 9 / tecla K)
        Ser* rb = 0;
        for (long i = 0; i < gRoot->nA && !rb; i++) {
            Ser* p2 = gRoot->A[i];
            if (!p2 || p2->tc != 'p') continue;
            for (long j = 0; j < p2->nA; j++)
                if (p2->A[j] && p2->A[j]->lb &&
                    strcmp(p2->A[j]->lb, "Rojo") == 0) { rb = p2->A[j]; break; }
        }
        long fi = 0;
        double* f = filaDe(rb, 0, &fi);
        long v0 = gVivos;
        ordP(9, rb, 0, 0, 0, 0, 0, 0);
        escritor();
        Ser* cl = 0;
        for (long j = 0; j < rb->fol->nA; j++) {
            Ser* q = rb->fol->A[j];
            if (q != rb && q->lb && strcmp(q->lb, "Rojo2") == 0) { cl = q; break; }
        }
        printf("clon profundo nace: %s (nM=%ld arg=%ld x=%ld)\n",
               (cl && gVivos == v0 + 1 && cl->nM == fi ? "PASA" : "FALLA"),
               cl ? cl->nM : -1,
               cl && cl->nM == 1 ? (long)((double*)cl->M[0])[5] / QZ : -1,
               cl ? (ccG(cl, CI_X) - ccG(rb, CI_X)) / QZ : -1);
        deshacer();
        escritor();
        printf("clon deshace: %s (vivos %ld -> %ld)\n",
               gVivos == v0 ? "PASA" : "FALLA", v0, gVivos);
        (void)f;
    }
    {                                    // menu de boton derecho: FIX hace pin
        Ser* obj0 = gObj ? gObj : 0;
        pisaDerecho(100, 100);           // preselecciona lo que este ahi
        printf("menu derecho abre: %s\n",
               gMenu && (ccG(gMenu, CI_FLG) & 1) ? "PASA" : "FALLA");
        printf("menu preselecciona: %s\n",
               (gObj && gObj != obj0) ? "PASA" : "FALLA");
        Ser* obj = gObj;
        long fl0 = ccG(obj, CI_FLG);
        float mfx = (float)ccG(gMenu, CI_X) / QZ + 20;
        float mfy = (float)ccG(gMenu, CI_Y) / QZ + 28;
        pisaRaton(mfx, mfy);             // FIX = VAL 6
        printf("menu FIX pin: %s (flg %ld -> %ld)\n",
               (ccG(obj, CI_FLG) & 8) ? "PASA" : "FALLA", fl0, ccG(obj, CI_FLG));
        printf("menu FIX cierra: %s\n",
               (ccG(gMenu, CI_FLG) & 1) ? "FALLA" : "PASA");
        deshacer();
        escritor();
        printf("menu FIX deshace: %s (flg %ld)\n",
               (ccG(obj, CI_FLG) & 8) ? "FALLA" : "PASA", ccG(obj, CI_FLG));
        pisaDerecho(100, 100);           // se abre de nuevo (el FIX lo cerro)
        float mnx = (float)ccG(gMenu, CI_X) / QZ + 40;
        float mny = (float)ccG(gMenu, CI_Y) / QZ + 40;
        pisaDerecho(mnx, mny);           // derecho abierto encima: se cierra
        printf("menu se cierra encima: %s\n",
               (ccG(gMenu, CI_FLG) & 1) ? "FALLA" : "PASA");
    }
    {                                    // filas SM editables + deshacer fila
        Ser* rb = 0;
        for (long j = 0; j < gMenu->fol->nA; j++) {
            Ser* p2 = gMenu->fol->A[j];
            if (p2 && p2->tc == 'p') {
                for (long i = 0; i < p2->nA; i++)
                    if (p2->A[i] && p2->A[i]->lb &&
                        strcmp(p2->A[i]->lb, "Rojo") == 0) { rb = p2->A[i]; break; }
            }
            if (rb) break;
        }
        gObj = rb; gSel = rb; syncInsp();
        gFilI = 0; gFilLast = 0;
        long nf = 0;
        double* f = filaDe(rb, 0, &nf);
        printf("filaRojo visible en panel: %s\n", f && nf == 1 ? "PASA" : "FALLA");
        syncDrw();
        printf("texto filas sigue: %s (%s)\n",
               gFilTxt && gFilTxt->lb &&
               strncmp(gFilTxt->lb, "FILA 1/1", 8) == 0 ? "PASA" : "FALLA",
               gFilTxt && gFilTxt->lb ? gFilTxt->lb : "(nil)");
        float cx = (float)ccG(gInsp2, CI_X) / QZ + 4 + 35 + 16;   // COND: VAL 12
        float cy = (float)ccG(gInsp2, CI_Y) / QZ + 56 + 9;
        pisaRaton(cx, cy);
        printf("fila COND cicla: %s (cond %ld -> %ld)\n",
               f && (long)f[0] == 1 ? "PASA" : "FALLA", 0L, f ? (long)f[0] : -1L);
        deshacer();
        escritor();
        printf("fila COND deshace: %s (cond %ld)\n",
               f && (long)f[0] == 0 ? "PASA" : "FALLA", f ? (long)f[0] : -1);
        float mx2 = (float)ccG(gInsp2, CI_X) / QZ + 4 + 5 * 35 + 16;  // MAS: VAL 16
        pisaRaton(mx2, cy);
        printf("fila MAS arg: %s (arg %ld)\n",
               f && (long)f[5] == QZ + 64 ? "PASA" : "FALLA",
               f ? (long)f[5] : -1);
        deshacer();
        escritor();
        printf("fila MAS deshace: %s (arg %ld)\n",
               f && (long)f[5] == QZ ? "PASA" : "FALLA", f ? (long)f[5] : -1);
    }
    {                                    // doble clic: fijar (pin) y despin
        Ser* obj = 0;
        for (long i = 0; i < gRoot->nA && !obj; i++) {
            Ser* p2 = gRoot->A[i];
            if (p2 && p2->tc == 'p' && p2->lb && strcmp(p2->lb, "Demo") == 0)
                obj = p2;
        }
        Ser* rb = 0;
        for (long i = 0; i < obj->nA; i++)
            if (obj->A[i] && obj->A[i]->lb && strcmp(obj->A[i]->lb, "Rojo") == 0)
                rb = obj->A[i];
        long fl0 = ccG(rb, CI_FLG);
        float bx = (float)ccG(rb, CI_X) / QZ + (float)ccG(rb->fol, CI_X) / QZ + 30;
        float by = (float)ccG(rb, CI_Y) / QZ + (float)ccG(rb->fol, CI_Y) / QZ + 10;
        pisaDoble(bx, by);
        printf("doble clic fija: %s (flg %ld -> %ld)\n",
               (ccG(rb, CI_FLG) & 8) ? "PASA" : "FALLA",
               (long)(fl0 & 8 ? 8 : 0), (long)ccG(rb, CI_FLG));
        pisaDoble(bx, by);
        printf("doble clic suelta: %s\n",
               (ccG(rb, CI_FLG) & 8) ? "FALLA" : "PASA");
    }
    {                                    // organos meta-UI reaceden si faltan
        gMenu = 0; gInsp2 = 0; gFilTxt = 0;
        long na0 = gRoot->nA;
        linkeaMeta();
        printf("linkea re-vincula los del archivo: %s\n",
               (gMenu && gInsp2 && gFilTxt) ? "PASA" : "FALLA");
        printf("linkea no duplica: %s (nA %ld -> %ld)\n",
               gRoot->nA == na0 ? "PASA" : "FALLA", na0, gRoot->nA);
        gMenu = 0; gInsp2 = 0; gFilTxt = 0;
        creaMenu(); creaFilas();         // sin rastro en el archivo: se crean
        printf("creaMenu/creaFilas reaciden: %s\n",
               (gMenu && gInsp2 && gFilTxt && gRoot->nA == na0 + 2) ? "PASA" : "FALLA");
    }
    {                                    // sliders del colador: pesos al toque
        Ser* sA = gColPnl && gColPnl->nA > 0 ? gColPnl->A[0] : 0;
        long okS = sA && sA->tc == 's' && ccG(sA, CI_EXT) == 100;
        printf("colador 3 sliders: %s\n",
               (okS && gColPnl->nA > 1 && ccG(gColPnl->A[1], CI_EXT) == 101 &&
                gColPnl->nA > 2 && ccG(gColPnl->A[2], CI_EXT) == 102) ? "PASA" : "FALLA");
        printf("colador slider nace con peso: %s (VAL %ld)\n",
               (okS && ccG(sA, CI_VAL) == 400) ? "PASA" : "FALLA",
               okS ? ccG(sA, CI_VAL) : -1);
        if (okS) {
            ccS(sA, CI_VAL, 900);
            inspAplica(sA);              // aplica y colada de nuevo al toque
            printf("peso ANTI por slider: %s (anti=%ld)\n",
                   (long)gColAnti == 900 ? "PASA" : "FALLA", (long)gColAnti);
            long nz = 0;
            for (long i = 0; i < 64 * 48; i++) if (gBufCol[i]) nz++;
            printf("colador recolada al toque: %s (pix %ld)\n",
                   nz > 0 ? "PASA" : "FALLA", nz);
            ccS(sA, CI_VAL, 400);
            inspAplica(sA);              // restaurar: no ensucie otros tests
            printf("peso ANTI restaurado: %s\n",
                   (long)gColAnti == 400 ? "PASA" : "FALLA");
        }
    }
    {                                    // gLin dinamico: crece al doble
        Ser* pS2 = serCr(gRoot, 'p', "pS2", "Stress");
        ccS(pS2, CI_X, 850 * QZ); ccS(pS2, CI_Y, 10 * QZ);
        ccS(pS2, CI_W, 60 * QZ); ccS(pS2, CI_H, 60 * QZ);
        ccS(pS2, CI_A, QZ); ccS(pS2, CI_FLG, 1);   // fuera de pantalla: solo lista
        ccS(pS2, CI_ABI, 1);              // abierto: listaRec baja a los 1100
        for (long i = 0; i < 1100; i++) {
            Ser* b2 = serCr(pS2, 'e', "sB", "s");
            ccS(b2, CI_A, QZ); ccS(b2, CI_FLG, 1);
        }
        gLinN = 0; listaRec(gRoot);
        printf("gLin dinamico: %s (N %ld, cap crecida)\n",
               gLinN > 1024 ? "PASA" : "FALLA", gLinN);
        for (long i = 0; i < gRoot->nA; i++)       // sacar el panel Stress: sin
            if (gRoot->A[i] == pS2) {              // escritor, no ensucia la escena
                for (long j = i; j < gRoot->nA - 1; j++) gRoot->A[j] = gRoot->A[j + 1];
                gRoot->nA--; break;
            }
        gLinN = 0; listaRec(gRoot);
    }
    {                                    // tipografia 3x5: fuga basica
        int dif = strcmp(fGly('n'), fGly('N')) == 0
               && strcmp(fGly('a'), fGly('A')) == 0
               && strcmp(fGly('o'), fGly('O')) == 0
               && strcmp(fGly('s'), fGly('S')) == 0;
        printf("fuga 3x5 basica (n==N, sin minusculas propias): %s\n",
               dif ? "PASA" : "FALLA");
        long fs0 = gFS;
        static unsigned int bk2[14 * 20];
        for (long j = 0; j < 14; j++)
            for (long i2 = 0; i2 < 20; i2++)
                bk2[j * 20 + i2] = gImg[(long)(430 + j) * gW + 300 + i2];
        gFS = 2; gTgt = gImg; gTW = gW; gTH = gH;
        for (long j = 0; j < 14; j++)
            for (long i2 = 0; i2 < 20; i2++)
                gImg[(long)(430 + j) * gW + 300 + i2] = 0;
        texto(300.0f, 430.0f, "jn", pr(1, 1, 1, 1));
        long nb = 0;
        for (long j = 0; j < 14; j++)
            for (long i2 = 0; i2 < 20; i2++)
                if (gImg[(long)(430 + j) * gW + 300 + i2]) nb++;
        printf("fuga pinta minuscula como mayuscula: %s (%ld px)\n",
               nb > 10 ? "PASA" : "FALLA", nb);
        for (long j = 0; j < 14; j++)
            for (long i2 = 0; i2 < 20; i2++)
                gImg[(long)(430 + j) * gW + 300 + i2] = bk2[j * 20 + i2];
        gFS = fs0;
    }
    {                                // tipografia de datos: glifos de conectores
        struct Fuente* F = &gFuUI;
        struct Glifo* gi = fuentePone(F, 'i', 2, 7, 4, 0);   // i angosta: minima
        if (gi) {
            long p2[8] = { 1, 0, 1, 5, 1, 5, 1, 7 };
            for (long i2 = 0; i2 < 8; i2++) gi->pts[i2] = p2[i2];
        }
        struct Glifo* gm = fuentePone(F, 'm', 9, 7, 6, 0);   // m ancha: sus puntos
        if (gm) {
            long p3[12] = { 0, 7, 0, 0, 4, 4, 8, 0, 8, 7, 4, 4 };
            for (long i2 = 0; i2 < 12; i2++) gm->pts[i2] = p3[i2];
        }
        printf("glifo angosto vs ancho: %s (i an=%ld m an=%ld)\n",
               (gi && gm && gi->an < gm->an) ? "PASA" : "FALLA",
               gi ? gi->an : -1, gm ? gm->an : -1);
        float w0 = anchoFu(F, "im", 1.0f);    // sin par: cada glifo su ancho
        espPone(F, 'i', 'm', 2);     // consecutividad: i antes de m corre 2
        float wim = anchoFu(F, "im", 1.0f);   // con par i-m: corre +2
        printf("espaciado por consecutividad: %s (sin %.0f vs con %.0f)\n",
               (wim - w0 == 2.0f) ? "PASA" : "FALLA", w0, wim);
        struct Glifo* gb = fuentePone(F, 254, 3, 2, 0, 2);   // bitmap: cuadrito
        if (gb)
            for (long r = 0; r < 2; r++)
                for (long i2 = 0; i2 < 3; i2++)
                    gb->bmp[r * 3 + i2] = (unsigned char)(r == 0 || i2 == 0);
        printf("glifo bitmap en variable: %s\n",
               (gb && gb->bmp && gb->nR == 2) ? "PASA" : "FALLA");
        long fs0 = gFS;
        static unsigned int bk3[16 * 60];
        for (long j = 0; j < 16; j++)
            for (long i2 = 0; i2 < 60; i2++)
                bk3[j * 60 + i2] = gImg[(long)(430 + j) * gW + 300 + i2];
        for (long j = 0; j < 16; j++)
            for (long i2 = 0; i2 < 60; i2++)
                gImg[(long)(430 + j) * gW + 300 + i2] = 0;
        gFS = 2; gFuAct = F;             // el mismo texto() sale por el contexto
        texto(300.0f, 430.0f, "imq", pr(1, 1, 1, 1));   // q sin glifu: nativo 5x3
        long nx = 0;
        for (long j = 0; j < 16; j++)
            for (long i2 = 0; i2 < 60; i2++)
                if (gImg[(long)(430 + j) * gW + 300 + i2]) nx++;
        printf("texto con contexto vectorial (faltante al nativo): %s (%ld px)\n",
               nx > 12 ? "PASA" : "FALLA", nx);
        for (long j = 0; j < 16; j++)
            for (long i2 = 0; i2 < 60; i2++)
                gImg[(long)(430 + j) * gW + 300 + i2] = bk3[j * 60 + i2];
        gFS = fs0; gFuAct = 0;
        fuenteGuarda(F);                 // el layout entero a su archivo
        fuenteQuita(F, 'm');
        long sinM = fuenteDame(F, 'm') == 0;
        long cargo = fuenteCarga(F);     // recarga: el archivo lo devuelve
        long leDuelve = fuenteDame(F, 'm') && espDame(F, 'i', 'm') == 2;
        printf("guarda/quita/carga el layout: %s\n",
               (sinM && cargo && leDuelve) ? "PASA" : "FALLA");
        fuenteQuita(F, 254);             // agrega/quita en vivo: ud. manda
        printf("agrega/quita en vivo: %s\n",
               (fuentePone(F, 253, 3, 5, 2, 0) && fuenteDame(F, 253) &&
                (fuenteQuita(F, 253), fuenteDame(F, 253) == 0)) ? "PASA" : "FALLA");
        removeD("fuente-ui.txt");
    }
    {   // datos junto al exe: raiz propia con el nombre del exe, ramificando
        long okd = gDirDatos[0] != 0;
        FILE* tv = fopenD("prueba_raiz.txt", "wb");
        if (tv) { fprintf(tv, "ramificando\n"); fclose(tv); }
        FILE* tr = fopenD("prueba_raiz.txt", "rb");
        long leido = tr != 0;
        if (tr) fclose(tr);
        long sera = removeD("prueba_raiz.txt");
        printf("datos junto al exe (raiz '%s'): %s\n", gDirDatos,
               (okd && leido && sera) ? "PASA" : "FALLA");
    }
    {                                    // export: tecla E -> ser_exp.ppm
        removeD("ser_exp.ppm");
        tecla('E'); escritor();
        FILE* e2 = fopenD("ser_exp.ppm", "rb");
        printf("export E: %s\n", e2 ? "PASA" : "FALLA");
        if (e2) {
            char mag[2] = { 0, 0 };
            fread(mag, 1, 2, e2);
            printf("export P6: %s",(mag[0] == 'P' && mag[1] == '6') ? " PASA\n" : " FALLA\n");
            fclose(e2);
            removeD("ser_exp.ppm");
        }
    }
    {   // export PNG (I): driver propio sin librerias, estructura verificada
        long okP = 0, okE = 0;
        removeD("ser_exp.png");
        tecla('I'); escritor();
        FILE* p = fopenD("ser_exp.png", "rb");
        if (p) {
            unsigned char sig[8], tip[4], cr[4];
            unsigned char big[65536];
            long dimOk = 0, idat = 0, viu = 0, crcOk = 1, esID = 0;
            if (fread(sig, 1, 8, p) == 8 &&
                sig[0] == 0x89 && sig[1] == 'P' && sig[2] == 'N' &&
                sig[3] == 'G' && sig[4] == 0x0d && sig[5] == 0x0a &&
                sig[6] == 0x1a && sig[7] == 0x0a) {
                for (;;) {
                    unsigned char ln[4];
                    long l;
                    if (fread(ln, 1, 4, p) != 4) break;
                    l = ((long)ln[0] << 24) | ((long)ln[1] << 16) |
                        ((long)ln[2] << 8) | (long)ln[3];
                    if (fread(tip, 1, 4, p) != 4) break;
                    esID = (tip[0] == 'I' && tip[1] == 'D' &&
                            tip[2] == 'A' && tip[3] == 'T');
                    if (esID) idat = 1;
                    unsigned int cc = crcIni();
                    cc = crcUpd(cc, tip, 4);
                    long re = l;                     // data leida al hilo (stream)
                    long primero = 1;
                    while (re > 0) {
                        long bl = re < 65536 ? re : 65536;
                        if (fread(big, 1, (size_t)bl, p) != (size_t)bl) {
                            crcOk = 0; break;
                        }
                        cc = crcUpd(cc, big, bl);
                        if (!esID && primero && tip[0] == 'I' &&
                            tip[1] == 'H' && tip[2] == 'D' && tip[3] == 'R' &&
                            l >= 8)
                            dimOk = (((long)big[0] << 24) | ((long)big[1] << 16) |
                                     ((long)big[2] << 8) | (long)big[3]) == gW &&
                                    (((long)big[4] << 24) | ((long)big[5] << 16) |
                                     ((long)big[6] << 8) | (long)big[7]) == gH;
                        primero = 0; re -= bl;
                    }
                    if (crcOk == 0) break;
                    if (fread(cr, 1, 4, p) != 4) { crcOk = 0; break; }
                    cc = crcFin(cc);
                    if ((cc >> 24) != cr[0] || ((cc >> 16) & 255) != cr[1] ||
                        ((cc >> 8) & 255) != cr[2] || (cc & 255) != cr[3]) {
                        crcOk = 0; break;
                    }
                    if (tip[0] == 'I' && tip[1] == 'E' && tip[2] == 'N' &&
                        tip[3] == 'D') {
                        viu = (l == 0 && cr[0] == 0xae && cr[1] == 0x42 &&
                               cr[2] == 0x60 && cr[3] == 0x82);
                        break;
                    }
                }
            }
            okP = dimOk && idat;
            okE = crcOk && viu;
            fclose(p);
        }
        printf("export PNG (I): %s\n", okP ? "PASA" : "FALLA");
        printf("png estructura (crc+IEND): %s\n", okE ? "PASA" : "FALLA");
        removeD("ser_exp.png");
    }
    {   // documento de escena: O -> ser_doc.txt
        long nod = 0, fil = 0;
        char li[256];
        removeD("ser_doc.txt");
        tecla('O'); escritor();
        FILE* e4 = fopenD("ser_doc.txt", "rb");
        if (e4) {
            while (fgets(li, sizeof(li), e4)) {
                if (strstr(li, " val ")) nod++;
                if (strstr(li, "fila: ")) fil++;
            }
            fclose(e4);
        }
        printf("documento de escena (nodos %ld filas %ld): %s\n", nod, fil,
               (nod > 3 && fil > 0) ? "PASA" : "FALLA");
        removeD("ser_doc.txt");
    }
    {   // pool de reservas CAS: tomar, agotar, soltar, reusar
        Ser* rv = gRoot; gRoot = 0;
        long s0 = gCa[CA_RES];
        Ser* p = serCr(0, 'p', "pl", "Pool");
        resFija(p, 2);
        long a1 = resToma(p);
        long a2 = resToma(p);
        long lleno = resToma(p);         // pool agotado
        long viv = resVivos(p);
        resSuelta(p, 0);
        long reu = resToma(p);           // reusa el hueco libre
        int okP = (a1 == 0 && a2 == 1 && lleno == -1 && viv == 2
                   && reu == 0 && resVivos(p) == 2
                   && gCa[CA_RES] == s0 + 3);
        printf("pool de reservas CAS (toma/suelta/reusa): %s\n",
               okP ? "PASA" : "FALLA");
        serAl(p);
        gRoot = rv;
    }
    {   // filas SM con reservas: op 6 toma, op 7 suelta, determinista por pasada
        Ser* rv = gRoot; gRoot = 0;
        Ser* p = serCr(0, 'p', "pr", "Reserva");
        resFija(p, 1);
        // pasada: toma, agotado = 0, suelta, reusa; igual en cada pasada
        f6Add(p, 0, 0, 0, 6, CI_EXT, 0);          // reserva -> EXT = slot + 1
        f6Add(p, 0, 0, 0, 6, CI_VAL, 0);          // pool lleno -> VAL = 0
        f6Add(p, 0, 0, 0, 7, CI_EXT, 0);          // suelta slot 0
        f6Add(p, 0, 0, 0, 6, CI_VAL, 0);          // hueco reusado -> VAL = 1
        exRows(p);
        int okF = (ccG(p, CI_EXT) == 0 && ccG(p, CI_VAL) == 1
                   && resVivos(p) == 1 && ccG(p, CI_TOP) == 1);
        exRows(p);                                 // segunda pasada: mismas marcas
        int okG = (ccG(p, CI_EXT) == 0 && ccG(p, CI_VAL) == 1
                   && resVivos(p) == 1 && ccG(p, CI_TOP) == 1);
        printf("filas SM de reservas (op 6/7): %s (toma=%d suelta=%d)\n",
               (okF && okG) ? "PASA" : "FALLA", okF, okG);
        serAl(p);
        gRoot = rv;
    }
    {   // contadores Ca: sistema vivo medible, doc de escena los lista
        Ser* rv = gRoot; gRoot = 0;
        long vS0 = gCa[CA_SER], vF0 = gCa[CA_FIL], vO0 = gCa[CA_ORD];
        Ser* p = serCr(0, 'p', "cA", "Conta");
        ccS(p, CI_VAL, 0);               // nodo reciclado: estado viejo fuera
        f6Add(p, 0, 0, 0, 1, CI_VAL, 7);
        exRows(p);
        long dS = gCa[CA_SER] - vS0, dF = gCa[CA_FIL] - vF0;
        long v7 = ccG(p, CI_VAL);
        ordP(0, p, CI_VAL, 9, 0, 0, 0, 0); escritor();
        long dO = gCa[CA_ORD] - vO0;
        docEscena("ser_doc.txt");
        long hadC = 0;
        FILE* e5 = fopenD("ser_doc.txt", "rb");
        if (e5) {
            char li[256];
            while (fgets(li, sizeof(li), e5))
                if (strstr(li, "contadores ca ")) hadC++;
            fclose(e5);
        }
        removeD("ser_doc.txt");
        int okA = (dS == 1 && dF >= 1 && dO >= 1 && v7 == 7 && hadC >= 2);
        printf("contadores Ca (ser/filas/ordenes/doc): %s\n",
               okA ? "PASA" : "FALLA");
        serAl(p);
        gRoot = rv;
    }
    {   // reserva 2 fases: reserva -> confirma -> suelta; epoch por liberacion
        Ser* rv = gRoot; gRoot = 0;
        Ser* p = serCr(0, 'p', "pf", "Fase2");
        resFija(p, 2);
        long a1 = resToma(p);
        int okR = (a1 == 0 && ccG(p, CI_TOP) == 1 && resVivos(p) == 1);
        okR = okR && resConfirma(p, a1) && ccG(p, CI_TOP) == 2
              && resVivos(p) == 1;
        long e0 = resEpoch(p);
        resSuelta(p, a1);
        okR = okR && ccG(p, CI_TOP) == 0 && resEpoch(p) == e0 + 1;
        printf("reserva 2 fases (reserva/confirma/suelta/epoch): %s\n",
               okR ? "PASA" : "FALLA");
        serAl(p);
        gRoot = rv;
    }
    {   // soltar reserva sin confirmar: limpia y no cuenta en el epoch
        Ser* rv = gRoot; gRoot = 0;
        Ser* p = serCr(0, 'p', "pf", "Fase1");
        resFija(p, 1);
        long e0 = resEpoch(p);
        long a1 = resToma(p);
        resSuelta(p, a1);                // directo desde reservado (1 -> 0)
        int okN = (a1 == 0 && ccG(p, CI_TOP) == 0 && resEpoch(p) == e0
                   && resVivos(p) == 0);
        long b1 = resToma(p);
        okN = okN && b1 == 0 && resConfirma(p, b1)
              && ccG(p, CI_TOP) == 2 && resVivos(p) == 1;
        resSuelta(p, b1);                // desde confirmado (2 -> 0): epoch + 1
        okN = okN && ccG(p, CI_TOP) == 0 && resEpoch(p) == e0 + 1;
        printf("reserva sin confirmar se suelta limpia (epoch 1): %s\n",
               okN ? "PASA" : "FALLA");
        serAl(p);
        gRoot = rv;
    }
    {   // filas SM de reserva 2 fases: op 6 reserva, op 8 confirma, op 7 suelta
        Ser* rv = gRoot; gRoot = 0;
        Ser* p = serCr(0, 'p', "pf", "FilaF2");
        resFija(p, 1);
        f6Add(p, 0, 0, 0, 6, CI_EXT, 0);          // reserva -> EXT = slot + 1
        f6Add(p, 0, 0, 0, 8, CI_VAL, 0);          // confirma slot 0 -> en uso
        exRows(p);
        int okH = (ccG(p, CI_EXT) == 1 && ccG(p, CI_VAL) == 1
                   && ccG(p, CI_TOP) == 2);
        f6Add(p, 0, 0, 0, 7, CI_EXT, 0);          // suelta el confirmado
        exRows(p);
        okH = okH && ccG(p, CI_EXT) == 0 && ccG(p, CI_TOP) == 0
              && resEpoch(p) == 1;
        printf("filas SM de reserva 2 fases (op 6/8/7): %s\n",
               okH ? "PASA" : "FALLA");
        serAl(p);
        gRoot = rv;
    }
    {   // escaneo rotativo: toma/suelta secuencial reparte los slots
        Ser* rv = gRoot; gRoot = 0;
        Ser* p = serCr(0, 'p', "pf", "Rota");
        resFija(p, 2);
        long x1 = resToma(p);
        resSuelta(p, x1);
        long y1 = resToma(p);
        resSuelta(p, y1);
        long z1 = resToma(p);
        int okT = (x1 == 0 && y1 == 1 && z1 == 0 && ccG(p, CI_POS) == 1);
        printf("scan rotativo del pool (alterna slots): %s\n",
               okT ? "PASA" : "FALLA");
        serAl(p);
        gRoot = rv;
    }
    {   // vidas: snapshot y rest; el estado vuelve intacto
        Ser* rv = gRoot; gRoot = 0;
        Ser* p = serCr(0, 'p', "vf", "Vidas");
        ccS(p, CI_VAL, 5); ccS(p, CI_X, 0);   // nodo reciclado: reset explicito
        vidaToma(p);
        ccS(p, CI_VAL, 99); ccS(p, CI_X, 7);  // la vida sigue: todo cambia
        int okV = (vidaN(p) == 1 && ccG(p, CI_VAL) == 99);
        okV = okV && vidaRest(p, 0, 0) && ccG(p, CI_VAL) == 5
              && ccG(p, CI_X) == 0;
        okV = okV && vidaN(p) == 1;           // rest conserva el snapshot
        printf("vidas: snapshot y rest (estado restaurado): %s\n",
               okV ? "PASA" : "FALLA");
        serAl(p);
        gRoot = rv;
    }
    {   // vidas: tope 10, el mas viejo sale de la cola
        Ser* rv = gRoot; gRoot = 0;
        Ser* p = serCr(0, 'p', "vf", "Tope");
        for (long i = 0; i < 12; i++) { ccS(p, CI_VAL, i); vidaToma(p); }
        int okW = (vidaN(p) == 10 && vidaRest(p, 0, 0)
                   && ccG(p, CI_VAL) == 11);  // el mas nuevo queda
        vidaRest(p, 9, 0);
        okW = okW && ccG(p, CI_VAL) == 2;     // el mas viejo que sobrevive
        printf("vidas: tope 10 (el mas viejo sale): %s\n",
               okW ? "PASA" : "FALLA");
        serAl(p);
        gRoot = rv;
    }
    {   // filas SM de vidas: op 9 snapshot, op 10 rest
        Ser* rv = gRoot; gRoot = 0;
        Ser* p = serCr(0, 'p', "vf", "FilaV");
        ccS(p, CI_VAL, 3); ccS(p, CI_EXT, 0); ccS(p, CI_SEL, 0);
        f6Add(p, 0, 0, 0, 9, CI_EXT, 0);      // EXT = snapshots (1)
        f6Add(p, 0, 0, 0, 0, CI_VAL, 42);     // vida sigue: VAL = 42
        f6Add(p, 0, 0, 0, 10, CI_SEL, 0);     // rest mas reciente -> SEL = 1
        exRows(p);
        int okF = (ccG(p, CI_EXT) == 0 && ccG(p, CI_SEL) == 1
                   && ccG(p, CI_VAL) == 3 && vidaN(p) == 1);
        printf("filas SM de vidas (op 9/10, el rest revierte el EXT de la misma pasada): %s\n",
               okF ? "PASA" : "FALLA");
        serAl(p);
        gRoot = rv;
    }
    {   // deshacer global de vidas: bitacora con tope, muertos fuera
        Ser* rv = gRoot; gRoot = 0;
        long e0 = gCa[CA_VIDA];
        Ser* a = serCr(0, 'p', "vf", "VivA");
        Ser* b = serCr(0, 'p', "vf", "VivB");
        ccS(a, CI_VAL, 11); vidaToma(a);
        ccS(a, CI_VAL, 22);
        ccS(b, CI_VAL, 33); vidaToma(b);
        ccS(b, CI_VAL, 44);
        long d1 = vidaDeshacer();             // b vuelve: 44 -> 33
        long d2 = vidaDeshacer();             // a vuelve: 22 -> 11
        int okD = (d1 && d2 && ccG(b, CI_VAL) == 33 && ccG(a, CI_VAL) == 11
                   && gCa[CA_VIDA] == e0 + 2);   // deshacer no snapshottea
        for (long i = 0; i < 40; i++) { ccS(b, CI_VAL, 100 + i); vidaToma(b); }
        okD = okD && vidaN(b) == 10 && gVidN == 1;   // dedupe: un solo nodo
        long d3 = vidaDeshacer();             // b otra vez: vuelve a 139
        okD = okD && d3 && ccG(b, CI_VAL) == 139;
        serAl(a); serAl(b);
        okD = okD && gVidN == 0;              // los muertos salen de la bitacora
        printf("deshacer global de vidas (bitacora tope %ld): %s\n",
               (long)VID_N, okD ? "PASA" : "FALLA");
        gRoot = rv;
    }
    {   // polar: posicion efectiva = declarada + radio * (cos, sin)
        Ser* rv = gRoot; gRoot = 0;
        Ser* p = serCr(0, 'b', "Pl", "Polar");
        ccS(p, CI_X, 100 * QZ); ccS(p, CI_Y, 50 * QZ);
        int okP = 1; long ex = -1, ey = -1;
        posEf(p, &ex, &ey);
        okP = (ex == 100 * QZ && ey == 50 * QZ);   // radios 0: como siempre
        ccS(p, CI_RAD, 30 * QZ);
        posEf(p, &ex, &ey);
        okP = okP && ex == 130 * QZ && ey == 50 * QZ;   // 0 grados: a la derecha
        ccS(p, CI_AGL, 90 * QZ);
        posEf(p, &ex, &ey);
        okP = okP && ex == 100 * QZ && ey == 80 * QZ;   // 90 grados: abajo
        ccS(p, CI_RAD, 0);                              // radio 0: vuelve limpio
        posEf(p, &ex, &ey);
        okP = okP && ex == 100 * QZ && ey == 50 * QZ;
        FILE* df = fopenD("ser_doc2.txt", "w");        // doc: lista el polar
        int hadP = 0, hadP2 = 0;
        if (df) { docRec(df, p, 0); fclose(df);
            df = fopenD("ser_doc2.txt", "r");
            if (df) { char li[256];
                while (fgets(li, sizeof(li), df))
                    if (strstr(li, "polar: radio")) hadP++;
                fclose(df); }
            removeD("ser_doc2.txt"); }
        okP = okP && hadP == 0;
        ccS(p, CI_RAD, 30 * QZ);
        df = fopenD("ser_doc2.txt", "w");
        if (df) { docRec(df, p, 0); fclose(df);
            df = fopenD("ser_doc2.txt", "r");
            if (df) { char li[256];
                while (fgets(li, sizeof(li), df))
                    if (strstr(li, "polar: radio 30")) hadP2++;
                fclose(df); }
            removeD("ser_doc2.txt"); }
        okP = okP && hadP2 == 1;
        printf("polar (posicion efectiva rad/agl + doc): %s\n",
               okP ? "PASA" : "FALLA");
        serAl(p);
        gRoot = rv;
    }
    {   // espiral con filas SM: angulo y radio crecen, la posicion gira
        Ser* rv = gRoot; gRoot = 0;
        Ser* p = serCr(0, 'b', "Pl", "Espira");
        ccS(p, CI_X, 0); ccS(p, CI_Y, 0); ccS(p, CI_AGL, 0); ccS(p, CI_RAD, 0);
        f6Add(p, 0, 0, 0, 1, CI_AGL, 45 * QZ);   // agl += 45 grados
        f6Add(p, 0, 0, 0, 1, CI_RAD, 10 * QZ);   // rad += 10
        exRows(p);
        int okS = (ccG(p, CI_AGL) == 45 * QZ && ccG(p, CI_RAD) == 10 * QZ);
        exRows(p); exRows(p); exRows(p);         // 4 pasadas: 180 grados, 40
        okS = okS && ccG(p, CI_AGL) == 180 * QZ && ccG(p, CI_RAD) == 40 * QZ;
        long ex2 = -1, ey2 = -1; posEf(p, &ex2, &ey2);
        okS = okS && ex2 == -40 * QZ && ey2 == 0;   // 180 grados: a la izquierda
        printf("espiral con filas SM (agl/rad crecen): %s\n",
               okS ? "PASA" : "FALLA");
        serAl(p);
        gRoot = rv;
    }
    FILE* o = fopenD("ser_check.ppm", "wb");
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
    printf("contadores: ser %ld filas %ld ordenes %ld reservas %ld exportes %ld"
           " vidas %ld\n",
           gCa[CA_SER], gCa[CA_FIL], gCa[CA_ORD], gCa[CA_RES], gCa[CA_EXP],
           gCa[CA_VIDA]);
    fuenteLibera(&gFuUI);                // tipografias de datos: fuera
    fuenteLibera(&gFu3D);
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
        fuenteGuarda(&gFuUI);            // cada tipografia a su archivo si cambio
        fuenteGuarda(&gFu3D);
        PostQuitMessage(0); return 0;
    case WM_MOUSEMOVE: {
        if (g3D) {                       // orbita: arrastrar = girar la camara
            static long u3x = -1, u3y = -1;
            long mx2 = (long)(short)(l & 65535), my2 = (long)(short)((l >> 16) & 65535);
            if (!(w & 1)) { u3x = -1; u3y = -1; return 0; }
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
    case WM_LBUTTONDBLCLK: {             // doble clic: fija (pin) el nodo
        POINT p; GetCursorPos(&p); ScreenToClient(gVen, &p);
        pisaDoble((float)p.x, (float)p.y);
        return 0; }
    case WM_RBUTTONDOWN: {               // boton derecho: menu contextual
        POINT p; GetCursorPos(&p); ScreenToClient(gVen, &p);
        pisaDerecho((float)p.x, (float)p.y);
        return 0; }
    case WM_SIZE: {                      // la ventana manda su tamano: rehacer
        long nw = (long)(l & 65535), nh = (long)((l >> 16) & 65535);
        if (nw > 40 && nh > 40 && (nw != gW || nh != gH) && gImg) {
            xMe((void*)gImg);
            gW = nw; gH = nh;
            gImg = (unsigned int*)xM((unsigned long)(gW * gH) * sizeof(unsigned int));
            unsigned int bgf = pr(0.07f, 0.08f, 0.1f, 1);
            for (unsigned long i = 0; i < (unsigned long)(gW * gH); i++) gImg[i] = bgf;
            gImgGen = -1;                // todo el frame se repinta
            if (gHdc) {                  // el ppp puede cambiar de monitor
                long pppB = (long)GetDeviceCaps(gHdc, LOGPIXELSX);
                if (pppB >= 48 && pppB != gPPP) {
                    gPPP = pppB;
                    gPXcm = (long)((double)pppB / 2.54 + 0.5);
                    gFS = fsDePpp(pppB);
                }
            }
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
    dirDatosArma();                      // datos en <dir exe>/<exe>/ desde ya
    WNDCLASSA w;
    w.style = CS_OWNDC | CS_DBLCLKS;
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
    long pppA = (long)GetDeviceCaps(gHdc, LOGPIXELSX);   // ppp real de la pantalla
    if (pppA < 48) pppA = 96;
    gPPP = pppA;
    gPXcm = (long)((double)pppA / 2.54 + 0.5);
    gFS = fsDePpp(pppA);                 // la fuente se mide, no se fija
    gImg = (unsigned int*)xM((unsigned long)(gW * gH) * sizeof(unsigned int));
    unsigned int bgf = pr(0.07f, 0.08f, 0.1f, 1);
    for (unsigned long i = 0; i < (unsigned long)(gW * gH); i++) gImg[i] = bgf;
    ShowWindow(gVen, 1);                 // el WM_SIZE inicial no cambia nada
    if (!desEscena(gRuta)) escModelo();
    linkeaMeta();                        // el archivo ya los tenia: se reusan
    if (!gMenu) creaMenu();              // organos meta-UI reaceden si faltan
    if (!gInsp2) creaFilas();
    if (fuenteCarga(&gFuUI)) gGen++;     // tipografias del sistema o del layout
    if (fuenteCarga(&gFu3D)) gGen++;     // (si no hay: el nativo 5x3 de socorro)
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
        if (gColNodo && __sync_lock_test_and_set(&gColNva, 0)) {
            long cw2 = ccG(gColNodo, CI_W) / QZ, ch2 = ccG(gColNodo, CI_H) / QZ;
            if (cw2 < 16) cw2 = 16;
            if (ch2 < 12) ch2 = 12;
            if (cw2 > 2048) cw2 = 2048;      // tope del cache de camara
            if (ch2 > 2048) ch2 = 2048;
            if (gColImg && (gColWc != cw2 || gColHc != ch2)) {
                xMe((void*)gColImg); gColImg = 0;
            }
            if (!gColImg) {
                gColImg = (unsigned int*)xM((unsigned long)(cw2 * ch2)
                                            * sizeof(unsigned int));
                gColWc = cw2; gColHc = ch2;
            }
            coladorPinta(gColImg, cw2, ch2);  // colado a la res de la camara
            ordFeed(gColNodo, gColImg, cw2, ch2);   // embudo del colado (nido)
        }
        escritor();
        tickN9(gRoot);                       // contadores vivos por cuadro
        syncAyuda();                         // barra fija sigue al foco
        syncDrw();                           // panel filas sigue a la fila
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
    dirDatosArma();                      // datos en <dir exe>/<exe>_d/ aqui
    pruebas();
    return 0;
}
#endif

