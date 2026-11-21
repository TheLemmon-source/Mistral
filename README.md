# Ser

Sistema universal en C++: una sola rama anidable (`struct Ser`) donde todo termina
siendo **condición y asignación**. Sin structs especializados, sin enums, sin
hardcodeo: cada nodo guarda su estado en matrices dinámicas `M[]` dirigidas por
descriptores `TM[]`, en punto fijo (`QZ = 256`).

Un único archivo: `src/Ser.cpp`, compilable en Windows x64
(MinGW64 / GCC 9.2, Dev-C++ o Makefile.win).

## Estructura del repo

```
README.md      <- este archivo (generalidades del repo)
src/Ser.cpp    <- el sistema completo
oroga.dbg      <- volcado de serializacion (formato N/V, se regenera)
```

## Meta-UI

Todo control puede llevar filas SM que **generan otros controles**
(op 5 `crb`), no solo asignar su propio estado. El panel `Gen` de la
escena modelo lo demuestra: al abrirlo despliega un boton, un check
y un slider propios; el tope por `arg` evita duplicados en cada
reapertura. Eso es el paso previo a formularios en cascada del todo
scriptados (hoy el editor requiere clics para crear controles).

## Inspector

El panel fijado `Inspect` presenta el nodo objetivo `gObj` (se sigue con un
clic sobre cualquier control, no solo seleccion estetica) como campos
editables: sliders r/g/b/redondeo que escriben al objetivo, checks `Vi`
(visible) y `Pn` (pin), check `Ed` (modo etiqueta). La tecla `L` abate el
modo etiqueta: escribir agrega caracteres al `lb` del objetivo (mayusculas,
retroceso borra). Atajos con objetivo: `V` visible, `P` pin, `F` al frente,
`B` al fondo, `Z` un paso adelante, `X` un paso atras, `D` borrar.

## Contadores (9 slots)

Fila `TM` char `'n9'`: **9 doubles** `[dst, delta, min, max, resetMin,
resetMax, compMin, compMax, activo]` – la convencion IRR completa:
valor en `C[dst]`, `delta` por tic, topes con resets especificos
(al pisar `max` v = `resetMax`; al pisar `min` v = `resetMin`),
`compMin`/`compMax` (1 = reset, 2 = desactivar la fila) y `activo`
(la fila apagada no avanza). `n9Add` la anexa; `tickN9` recorre el arbol
por frame, asi los contadores avanzan solos en ventana. Persisten como
lineas `W idx + 9 doubles` en `oroga.dbg`.

## Pick por zonas anidables

El pick (`elige`) ya no recorre la lista colada entera: desciende como el
render (mismo orden de colado, mismo doble pase del pin) y cada
**zona** (`'p'` panel, `'q'` sesion, `'c'` cubo) funciona como caja
cancelante: si el punto cae fuera de su rect, la rama entera se
cancela sin recorrer hijos. Los nodos pintados despues ganan, como en
pantalla. La lista colada `gLin` queda como aviso estadistico, no como
indispensable del pick.

## Deshacer

Toda asignacion de estado que pasa por la GUI queda logueada por
**grupos** (un clic = un grupo, una tecla = un grupo, un arrastre = un
grupo): canal, valor previo y etiqueta previa. `U` repone el ultimo
grupo completo en reversa. Borrar y reordenar en z no se reponen
(quedan para filas SM). Sin lugar en el anillo de 128 entradas el
fondo se pierde (aviso implicito), nunca corrompe.

## Hilos por pares (par vigilante, impar obrero)

Dos hilos, cero mutex: **par** = vigilante (`alVigilante`) que duerme 16 ms,
hace avanzar el tic cronologico `gTic` y genera el frame de la fuente de
camara en `gBufCam`; **impar** = la ventana misma (escritor + pintado). El
pase de dato es un embudo interlocked: `__sync_lock_test_and_set(&gCamNva, 0)`
avisa "hay frame nuevo" sin carrera, y el escritor `k==6` usa un **seqlock**
contra `gCamF` (hasta 4 reintentos) para nunca leer un frame a medio armar.
En Win son `CreateThread`/`CloseHandle`; en POSIX, `pthread_create`/`pthread_join`.
Validado por pruebas headless: "hilo par tic cronologico" e "hilo par alimenta
la camara".

## Cubo 3D y colador (el mismo Ser)

La fuente del par ya no dibuja un degradado: **proyecta un cubo 3D girando**
(perspectiva `x/(z+3.2)`, pintor por promedio de z, luz fija, relleno por
scanlines `triLleno`). El 3D no necesita nada nuevo: es la misma fila `'x'`
`[img,w,h,ver]` que produce pixels y viaja por el mismo embudo/seqlock.
El **colador** es el tercer paso, un pase del impar cada 8 tics: toma el bbox
del mundo visible y lo ajusta a la resolucion pedida. Ya no son rects crudos:
cada rama sale **con el pintor real** (widgets, titulos, textos) desde el
cache por version (`cachePinta`), volcado a escala (`blitEsc`), filtrado por
puntaje con **tres pesos volatiles** `gColAnti`/`gColImp`/`gColUrg`
(frescura: cada nodo guarda el tic de su ultimo toque en `s->ulti`, que
`ccS` actualiza en cada asignacion; importancia: pin 900 / abierto 500 /
visible 300; urgencia: filas SM 600). La base queda en `gBufCol` 64x48 para
la bateria y, **anidado con camara**, la camara **Colado** (`gColNodo`)
pide su propia resolucion: el embudo `gColNva` redibuja a ese tamano
(`gColImg`, hasta 2048) y la alimenta con pixels nitidos. Es la base del
colador a imagen del roadmap: la escena entera en una imagen que ya se
puede anidar, guardar y serializar sin migrar nada despues.

## Camara global

`gCamX`/`gCamY` volatiles: **pantalla = mundo - camara** (solo ramas sin
pin: los fijados viven en pantalla). El escritor
la mueve con op `8` (`ordP(8, 0, dx, dy, 0,0,0,0)`); flechas del teclado
la desplazan a pasos de 32 px y `C` la vuelve al origen. El render
reubica cada cache de raiz con la camara; el pick convierte
pantalla -> mundo antes de bajar por zonas.

## Barra de Ayuda y foco visible

La escena de arranque trae una barra **fijada** (pin) al pie: una línea
clava los atajos (FLECHAS cámara, C origen, U deshacer, L etiqueta,
V visible, P pin, D borrar, K clonar, E exporta, F/B/Z/X orden z) y una línea de estado
se reescribe por frame con el foco real: `SEL nombre [tipo]` + marcas
`OCULTO`/`PIN` (sin repintar si no cambia: solo sube versión al
cambiar el texto). El nodo con foco (`gObj`) lleva **marco ámbar de
2 px** además del borde blanco de selección.

Además la cámara ya no arrastra la UI fijada: los hijos tope de la
raíz con pin se pintan y se pícan en **espacio de pantalla** (la
cámara se aplica por hijo según su pin), y el arrastre/slider
convierte el ratón a mundo con `ratonMundo` solo cuando corresponde.

## Filas desde texto (fTx)

`fTx(nodo, texto)` genera filas en runtime desde texto: una linea (o
segmento entre `;`) con 6 numeros crea una fila SM `f6`; con 9 crea un
contador `n9`; cualquier otra cuenta se descarta. Los valores son
doubles (condiciones fraccionarias incluidas). Es el comienzo del
parser general: el conteo de numeros por linea sustituye la gramatica.

## Demo coherente y arcos al fol

Todo control del panel `Demo` ahora hace algo visible, y las filas SM
pueden apuntar **al padre**: en `dst` lleva un entero >= 0 (canal propio,
como siempre) o negativo con la convencion `-1 - k` = canal `k` del
`fol`. `f6Add(b, 0.5, 0,0,0, -1-CI_R, QZ)` pinta de rojo al panel que
contiene al boton. Asi dependen de una sola fila: `Rojo`/`Verde`/`Azul`
pintan el panel, `Fijar` lo clava en pantalla (xor del pin, op 2) y el
slider `R obj` (canal `CI_EXT`) actua sobre **el nodo elegido con un
clic**, no solo sobre el padre.

## Consistencia de sesion: tipos y apuntadores

Tres bugs viejos que se tapaban mutuamente:

- **Al recargar, el tipo tc se perdia**: el guardado escribe `tc` como
  letra (`%c`) pero la carga lo leia con `leeNum` (numerico), asi que
  todo nodo recargado nacia con tipo 0 y la ventana despertaba sin
  botones/checks/sliders funcionales tras reiniciar (parte del "parece
  a dibujos sin funcionalidad"). La carga ahora lee el campo 3 como
  token (`campoTxt`), para el formato viejo y el nuevo.
- **Crash al editar etiqueta**: eliminar un nodo (k==2, DELETE/D) nunca
  limpiaba `gObj`/`gSel`/`gAnt`/inspectores/etc: quedaban punteros
  colgando y el proximo toque pisaba memoria liberada. Ahora
  `borraApuntados` limpia los 19 apuntadores globales (agrego: menu
  contextual, panel de filas, texto vivo) antes de liberar.
- **Flechas que escribian basura en la etiqueta**: en `WM_KEYDOWN` los
  codigos VK (flechas = 0x25..0x28) caian en `tecla((char)w)` y se
  insertaban como `% ' &`. Ahora los VK especiales cortan antes y solo
  pasan a la etiqueta backspace y glifos 32..126.

## Deshacer real (crear y borrar incluidos)

El log de deshacer creció y ahora cubre todo: `gLg[2048]` ya no se vacía
con un arrastre largo (antes 2 entradas por píxel de `WM_MOUSEMOVE`
comían los 128 lugares), y **crear (op 1) y borrar (op 2) son
deshacibles**: borrar manda el nodo al **cementerio** (`gCem[8]`) en vez
de liberarlo, `U` lo vuelve a su mismo padre (al hueco que dejó o al
final); si el cementerio se llena, evacua al más viejo **purgando su
rastro del log** (sus entradas quedan 0 y se saltan) para nunca resucitar
memoria liberada. `D`/DELETE abre su propio grupo de undo. Undo de
etiqueta (`-2`) devuelve el texto exacto previo (y ahora la **fila SM**
entera, ci `-5`). La barra de ayuda lo dice: `U:deshacer todo ...`
(BORRAR esta en el menu del boton derecho y vuelve con U).

## Modo 3D de la escena (G)

`G` entra/sale del **modo 3D**: la escena misma vista en profundidad, no
un motor nuevo. Cada nodo visible es una **placa** (su caja x/y/w/h, con
tope de 1200 px) en `z = -1.1 * nivel` (padre atrás, hijos adelante),
proyectada con la misma matemática del cubo de la cámara (rotY+rotX,
perspectiva `1/(z+6)`, mundo = píxel/30, escala neta 1:1 en el centro) y
rellenada con `triLleno` por el **pintor**: ordenamiento por z entre
placas y entre las 6 caras de cada caja, con **luz** por normal rotada
(`sh = 0.35 + 0.65*(0.4 - rz*0.7 + 0.6*ry + 0.4*rz)` tope 0.2..1).
Interacción: FLECHAS o arrastre giran la órbita, `+`/`-` zoom (0.3..3),
un **clic elige el tablero** (pick por centro proyectado, radio 18 px:
queda `gSel`/`gObj` y el resto del editor opera sobre el mismo Ser).
Las etiquetas se dibujan planas encima (siempre legibles) y ámbar para
el nodo con foco. El bbox de la escena se computa en píxeles y se
centra: ninguna escena se va de pantalla.

## Clon, menu de boton derecho y filas editables

La interfaz dejo de ser dibujo sin proposito:

- **Clon profundo** (tecla `K` o CLON en el menu): op `9` del escritor
  copia la rama entera (hijos, filas `f6`/`n9`, hasta nivel 8) junto al
  original (+30, +26), lo deja seleccionado y es deshacible con `U`.
- **Menu de boton derecho**: `WM_RBUTTONDOWN` pre-selecciona lo que
  esta bajo el cursor y abre un panel contextual (FIX/FRENTE/CLON/
  BORRAR/FILAS/EXPORT) pegado al cursor (clamp a la ventana). FIX hace
  pin, FRENTE es op 3, CLON es op 9, BORRAR es op 2, FILAS muestra/
  oculta el inspector de filas, EXPORT es op 10. Toda accion cierra el
  menu; un derecho sobre el menu abierto tambien lo cierra.
- **Doble clic fija**: `WM_LBUTTONDBLCLK` (registro con `CS_DBLCLKS`)
  toggle del pin (FLG bit 8), manda al frente y lo deja con foco.
- **Filas SM editables desde la GUI**: panel fijado "Filas SM" que
  muestra la fila elegida del nodo con foco (`FILA i/n COND CI O DST
  ARG` vivo) y botones FILA (cicla fila), COND (SI/</>/=), CI (cicla
  canal), OP (cicla op), DST (propio -> fol.k -> propio), MAS/MENOS
  (arg ±64, ejecuta `exRows` en el acto). Cada cambio entra al log con
  **undo de la fila entera** (ci `-5`: valores previos + puntero
  validado contra `M[]` vivo al deshacer).
- **Paneles meta que re-aparecen**: `linkeaMeta()` re-vincula Menu,
  Colador y Filas SM cuando vuelven del `oroga.dbg` (sin duplicarlos);
  si el archivo no los tenia, creaMenu/creaFilas/creaColador los crean.

## gLin dinamico, sliders del colador y exportar

- **Lista colada sin techo**: `gLin` era un array fijo de 1024 y el
  pintador avisaba por stderr cuando el colado se pasaba. Ahora es
  dinamica y crece al doble (1024, 2048, ...) sin nunca recortar: el
  pick y el pintador siguen compartiendo el mismo orden.
- **Panel "Colador" con los pesos vivos**: tres sliders ANTI/IMP/
  URG (`CI_EXT` 100/101/102) que nacen con los pesos reales
  (`gColAnti`/`gColImp`/`gColUrg`). Mover un slider aplica al toque:
  `inspAplica` setea el peso y llama `colador()` en el mismo evento,
  asi la imagen colada reacciona mientras arrastras.
- **Exportar lo pintado**: tecla `E` o EXPORT del menu derecho (op 10
  del escritor) vuelca el frame entero a `ser_exp.ppm` (P6, RGB).
  Es el pie del roadmap de export/documentos: la propia GUI produce
  una imagen que otros programas pueden abrir.
- Ayuda del pie actualizada: `V-P-D-E en menu derecho`.

## Arcos al fol con profundidad (3D)

- **Arcos al fol con profundidad en 3D**: los arcos logicos (filas SM
  con `dst -1-k` = canal `k` del padre) ahora se ven en el modo 3D:
  `pinta3D` dibuja un cable azul del centro proyectado del nodo al
  centro proyectado de su padre para cada nodo que escribe al fol
  (`arFol`). La fila fluye en la escena: el nodo que escribe y el
  canal que recibe quedan conectados visualmente. El trayecto usa el
  proyectado (orbita + zoom) para que gire con la camara.
- Direcciones futuras de la fila `x` (quedan): ticks a profundidad y
  semi-arcos texturizados.

## Tipografia de datos: glifos de conectores, nada hardcodeado

La tipografia dejo de ser un switch fijo: ahora es **dato**. Cada contexto
(`ui`, `3d`, el que venga) tiene su `Fuente` con catalogo dinamico de
`Glifo`s: un glifo es una **polyline de conectores** entre puntos (contornos
a escala libre, el espesor de linea es la escala) **o un bitmap en variable**
(1 byte por pixel, 0/1, escala entera). Igual que el resto del Ser:
condiciones + asignacion, cero constantes de layout quemadas.

- **Gasto minimo por letra**: el em-cuadro es propio de cada glifo. La `i`
  es 2x7 con 4 puntos; la `m` es 9x7 con 6 puntos; un marcador puede ser un
  bitmap 3x2. Nada fuerza una regla comun.
- **Espaciado por consecutividad**: la fuente guarda pares `(a, b, dx)`
  (`K` en el archivo): la distancia entre dos letras depende de la
  consecutividad, no de una regla. Pares como i-u e i-m pueden llevar
  espaciados distintos; los pares no declarados usan 0.
- **Faltante al nativo de socorro**: si un byte no tiene glifo en la fuente
  del contexto, cae al 5x3 fijo (`fGly`) que nunca puede fallar. Sin fuente
  cargada, todo el texto sale por el nativo. El nativo es **sistema de fuga
  basico, a proposito**: mayusculas, digitos y simbolos elementales. Sin
  minusculas (caen a su mayuscula), sin acentos ni ene: es lo minimo vital
  para referencias cuando no se puede acceder a fuentes previstas. Las
  simbologias avanzadas — case real, acentos, estilos — viven en las
  fuentes de datos, extendibles a 6x10 o 10x50 segun demanda, sin tocar
  el escape. Para ver texto case-real en la GUI alcanza con poblar
  `fuente-ui.txt`; el sistema lo recarga al arrancar.
- **Persistencia por contexto**: al salir, cada fuente con cambios se guarda
  a disco (`fuente-ui.txt`, `fuente-3d.txt`); al arrancar se recargan y
  invalidan los caches. Agregar o quitar glifos en vivo (`fuentePone`/
  `fuenteQuita`/`espPone`) marca la deuda `sucio` y el archivo nace solo.
  Formato de texto, por glifo:

  ```
  G <cod> <an> <alt> <nP> <nR>    # glifo: codigo, em-cuadro, puntos | filas
  P <x0> <y0> <x1> <y1> ...       # conectores: 2*nP numeros, polyline
  R 10110...                      # bitmap: <nR> lineas de <an> digitos 0/1
  K <a> <b> <dx>                  # espaciado por consecutividad
  ```

Espera conectar `texto()` con tipografias traidas de discos reales (cargar
fuentes del sistema queda como fila pendiente); lo de hoy ya soporta
patrones simbolicos propios por layout/contexto con alta y baja en vivo.

## Datos junto al exe: una raiz propia, ramificando desde ahi

Los datos del programa ya no caen regados por el directorio actual: viven
en una carpeta con el nombre del exe, en la carpeta donde el exe reside.
`Ser.exe` en `C:\algo\` usa `C:\algo\Ser\`; ahi dentro van el layout
(`oroga.dbg`), las tipografias por contexto (`fuente-ui.txt`,
`fuente-3d.txt`) y los exportes (`ser_exp.ppm`, `ser_exp.png`,
`ser_doc.txt`, `ser_check.ppm`).

La raiz se arma una sola vez al arrancar (idempotente, con separador
final listo para ramas) y todo acceso a datos pasa por `fopenD`/
`removeD`, que agregan la raiz a cada nombre relativo: ramas nuevas
(subcarpetas de fuentes, escenas, documentos) se agregan sin
reconstruir lecturas por arbol de directorios. Sin extension el nombre
lleva sufijo `_d` para no chocar con el propio exe.

## Exportes: PNG propio y documento de escena

La imagen pintada sale a dos formatos sin tocar librerias externas:

- **PNG** (tecla `I`, boton del menu derecho, orden escritor `k==11`):
  `ser_exp.png`, RGB 8 bits, deflate a mano en bloques guardados
  (CRC32 reflexivo por chunk, adler32 al final del stream zlib, NLEN
  complementado). Si lo abre un visor, es porque cumple.
- **Documento de escena** (tecla `O`, boton del menu, orden `k==12`):
  `ser_doc.txt`, el arbol completo indentado por profundidad, cada nodo
  con tipo, etiqueta, tamano, posicion y valor, mas sus filas SM al pie:
  la escena entera en texto legible.

En 3D la escena ahora muestra **rejilla de piso** al fondo del ultimo
nivel, **regla de profundidad** con marcas `N0..Nn` por nivel (cada uno
vive en su lamina z) y las placas de nodos alineadas con centros,
etiquetas y cables: antes el ancho de placa se escalaba en otra unidad
y caian fuera de la proyeccion.

## Pool de reservas CAS y contadores por sistema

Cada nodo puede declarar un **pool de reservas**: `CI_PON` slots que viven
en su propio `C[CI_TOP..CI_TOP+CI_PON)` (0 = libre, 1 = reservado,
2 = confirmado/en uso). Tomar, confirmar y soltar son **CAS atomicas**
(`__sync_bool_compare_and_swap`), asi que sirven para repartir recursos
entre hilos sin mutex:

- `resFija(s, n)` arma o redimensiona el pool (tope 64, slots limpios).
- `resToma(s)` reserva el primer slot libre o -1 si esta lleno, con
  **escaneo rotativo**: arranca donde quedo la ultima toma (`CI_POS`),
  asi toma/suelta secuencial reparte los slots en vez de atorarse en el
  mismo. `resConfirma(s, i)` pasa el slot a en uso (CAS 1 -> 2);
  `resSuelta(s, i)` libera desde reservado o confirmado, y si estaba
  confirmado suma **epoch** (`CI_POE`): un contador de generaciones del
  pool que avanza con cada liberacion confirmada. `resVivos(s)` cuenta
  ocupados. Tomar sin confirmar sigue siendo util: la suelta limpia no
  cuenta epoch, marca de que el recurso nunca se uso.
- Como **filas SM**: op `6` reserva y escribe en `dst` el slot + 1 (0 si
  agotado), op `8` confirma el slot `arg`, op `7` suelta el slot `arg`.
  El condicional de la fila sigue funcionando: "si hay hueco, reservo;
  si no, valgo 0" es una fila, y confirmar sobre un slot ajeno da 0.
- El documento de escena (`O`) lista por nodo `pool: N de M slots
  ocupados (epoch E)`, y al pie dos lineas **contadores ca** con
  telemetria de sistema: seres, muertes, matrices, filas ejecutadas,
  ordenes drenadas por el escritor, reservas tomadas y exportes.

## Vidas: snapshots de estado y deshacer global (H)

De los lotes externos "inspiradores" (snapshots + regla R2 de reforma
segura), adoptado al estilo propio: cada nodo puede sostener
**snapshots de su propio `C[]`** como matrices `vd`
(`[nC, secuencia, C[0..nC))`), cola con **tope 10** por nodo (el mas
viejo sale). Nada de copiar estructuras: una vida son numeros.

- `vidaToma(s)` saca el snapshot, `vidaRest(s, k, pop)` vuelve (k 0 =
  el mas nuevo; con `pop` lo consume), `vidaN(s)` cuenta, y
  `vidaDeshacer()` deshace el **ultimo efecto global**: una bitacora
  de 24 nodos (dedupe, el mas reciente en la punta, los muertos salen
  al morir) recordando quien snapshotteo ultimo.
- Como **filas SM**: op `9` snapshot (`dst` = cantidad) y op `10`
  volver al snapshot `arg` (`dst` = 1 si hubo). Ojo: el rest revierte
  todo el `C[]`, incluso lo escrito por filas anteriores de la misma
  pasada — asi la "reforma" esta cubierta: snapshot antes del cambio,
  rest si salio mal.
- Tecla **H**: `vidaDeshacer()`, el hermano de `U` (deshacer de
  asignaciones) para efectos de vida.
- El doc de escena lista por nodo `vidas: N snapshots (tope 10)` y el
  contador `vidas` suma a la telemetria **ca** al pie.
- Los nodos reciclados del pool conservan su historial? No: `serAl`
  libera las matrices `vd` con el resto; cada vida muere con su nodo.

## Polar: posicion efectiva unificada

La base para la espiral y los semi-arcos, sin volver a tocar nada
arriba: dos canales, `CI_AGL` (angulo en grados * QZ) y `CI_RAD`
(radio en QZ), y **un solo resolutor** `posEf()` que pregunta todo: el
pintor, el pickeo, el 3D (bbox y proyeccion) y el colador. La posicion
efectivo = declarada + radio * (cos, sin) en grados * QZ:

- Radio 0 = exactamente como siempre (x, y absolutos); nada cambia de
  golpe y el arrastre sigue moviendo el pivote declarado.
- Espiral = filas SM que avanzan `CI_AGL` (359*QZ = vuelta entera) y
  crecen `CI_RAD`; el mismo nodo orbita y sale en espiral en 2D y en 3D
  por el mismo camino.
- Los canales viven arriba de los del pool (96/97) y solo se
  materializan al pedirlos (`ccG`/`ccS` crecen `C[]` a demanda).
- El doc de escena imprime la posicion efectiva y anota
  `polar: radio R grados G` en cada nodo con radio activo.

## Semi-arcos texturizados en 3D

Tercer canal polar, `CI_ARC` (barrido en grados * QZ) sobre la base
`CI_AGL`/`CI_RAD`: un nodo con `CI_ARC != 0` deja de ser una caja y se
dibuja en 3D como **banda curva de 24 segmentos** que arranca en el
angulo `CI_AGL` y barre `CI_ARC` grados.

- Geometria ancla al tamano declarado: radio exterior = W/2, grosor de
  la banda = H (interior clampado a 1/4 del exterior); altura = la
  misma placa de las cajas (`dz`).
- Textura = la matriz `'x'` del nodo, muestreada por segmento en el
  centro horizontal del tramo; sin textura cae al color del nodo.
- Sombreado consistente con las caras (misma normal teorica por
  azimut) y canto exterior oscurecido x0.7 como el resto del 3D.
- El bbox 3D cubre el anillo completo (centro +/- radio maximo), y el
  bloque de placas tambien se posiciona por `posEf`: polar + arco
  conviven.
- `CI_ARC = 0` = caja de siempre; nada de lo existente cambia.

## Sesion: polar y estado de maquina persistidos

Linea extendida **`P <idx> <agl> <rad> <arc> <est>`** al pie de cada
nodo en `oroga.dbg` (mismo estilo que `V` y `W`): guarda los canales
`CI_AGL`/`CI_RAD`/`CI_ARC` y el estado `CI_EST` de las maquinas.
Solo se escribe si alguno es distinto de 0, asi que:

- Formato viejo intacto: el lector nuevo ignora lo que no conoce y el
  lector viejo jamas ve la linea `P` (los archivos viejos cargan igual).
- Ceros no persisten: canal ausente = 0, la sesion no engorda.
- Al recargar, espiral, semiarco y la fase de la maquina reaparecen
  exactos donde quedaron.

## Maquinas de estados (fsm)

La FSM del lote inspirador, traducida a "todo es condicion +
asignacion": una maquina es un nodo **`tc 'f'`**, sus estados son sus
hijos, el canal **`CI_EST`** (99) es el estado vivo, y **op 11** en
filas SM es el salto:

- Fila `op 11`: si la condicion de la fila es cierta la maquina salta
  al estado `dst`; si es falsa, al estado `arg`. `-1` o el mismo
  estado = se queda. La condicion es la de siempre (0 siempre,
  1 `<`, 2 `>`, 3 `==`) sobre cualquier canal.
- Las filas del estado vivo corren cada paso (`fsmPaso`): las
  asignaciones/op 6-10 de ese hijo SON la accion del estado, sin
  interprete nuevo. `fsmPaso` corre en `tickN9`, o sea las maquinas
  avanzan por cuadro junto a los contadores `n9`.
- Salto invalido (indice fuera de rango) = se queda: sin crash.
- Pintado propio: panel con `est N de M` al pie, y el doc de escena
  lista `fsm: estado E de N estados` por maquina.
- La configuracion sobrevive el guardado: las filas (con su op 11) van
  por las lineas `V` y el estado por la linea `P`.
- Cada maquina se pinta con su **diagrama de estados**: una franja de
  chips (uno por estado, el vivo en ambar) y rieles cyan con los
  saltos `op 11`. El ancho se reparte generico: cualquier cantidad de
  estados y ramificaciones, sin casos especiales.
- El diagrama **es el editor**: clic en un chip dispara ese salto
  (con vuelta atras: `U` lo deshace) y selecciona el estado; clic en
  un riel selecciona su fila `op 11` directamente en el editor de
  filas de siempre. Pintor y pickeo comparten la misma geometria
  (`fsmBand`), asi que lo que se ve es exactamente lo que se pica.

## Audio: mezclador, eventos y WAV

El audio entra con la misma filosofia: un mezclador es un nodo
**`tc 'a'`** con matriz **`'au'`** (`double[2+n]`: usados, capacidad,
samples float en [-1,1]) y canales propios:

- `CI_SR` (100) sample rate, `CI_DUR` (102) duracion del buffer en ms,
  `CI_AMP` (101) volumen maestro (QZ = 1.0), `CI_PTO` (103) la aguja
  donde escribe la proxima voz. Defaults auto-reparados: 22050 Hz,
  1000 ms, volumen completo; capacidad = `sr*dur/1000+4` con topes
  sanitarios (96000 Hz, 60000 ms).
- **op 12 = tocar por evento**: la frecuencia sale del canal `dst` del
  nodo dueno de la fila (`dst < 0` = lectura al fol via arco, como
  siempre), `arg` = duracion en ms (si `<= 0` usa `CI_DUR` del
  mezclador) y el volumen es `CI_AMP`. Renderiza un seno con
  envolvente fade in/out (n/10), clip [-1,1], sobre la aguja y la
  avanza. Junto con fsm: un estado con fila op 12 **suena al entrar**.
- **Mezcla aditiva**: asignar `CI_PTO` a 0 entre tonos (fila op 0
  `CI_PTO=0`) hace que las voces solapen en el mismo tramo y se sumen;
  dejarla correr las pone en secuencia.
- **op 13 = exportar**: escribe `ser_son.wav` (RIFF/PCM 16 bits mono)
  con lo mezclado hasta ahora.
- **Audio externo**: tecla `J` carga `ser_externo.wav` al mezclador
  vivo (parser RIFF propio, chunks fmt/data, PCM 16 mono); ajusta
  `CI_SR` y capacidad, la aguja sigue al final.
- Teclas: **Y** toca el demo (440 Hz, 300 ms) en el mezclador vivo,
  **W** exporta su WAV. `auVivo()` = el objeto picado si es `tc 'a'`,
  si no el primer mezclador del arbol.
- El pintor dibuja el panel con la **onda** (pico por columna) y la
  aguja cyan; el doc de escena lista
  `audio: N Hz, M muestras de CAP, aguja P`.
- Persistencia: linea `A <idx> <sr> <dur> <amp>` solo si hay algo
  fuera de los defaults. El buffer no persiste: lo reproducible lo
  reproducen las filas, lo externo lo re-carga `J`.

Falta (proximo turno): salida a altavoz (hilo `waveOut`/WASAPI),
grabacion de entrada, bus master de varios mezcladores.

## Contexto grafico: ppp, cm y rescalado del layout

Nada de tamanos fijados a mano: el arranque de la ventana mide el
**ppp real** con `GetDeviceCaps(LOGPIXELSX)` (y lo re-mide en
`WM_SIZE`, por si cambia de monitor); de ahi salen:

- `gPXcm` = ppp / 2.54 y el helper **`cmQz()`**: texturas y textos de
  graficas se piden en centimetros (`cmQz(1.0)` = 1 cm de alto) y se
  resuelven al contexto vivo.
- **`gFS`** (escala de tipografia) es derivada: `fsDePpp(ppp)`; 96 ->
  2, 144 -> 3, 192 -> 4. Otra compu con otra pantalla: el texto misma
  altura fisica, sin tocar codigo.
- El layout guardado lleva su **linea de contexto**
  `G <w> <h> <ppp>` al inicio de `oroga.dbg`. Al recargar en otro
  contexto, los nodos (menos la raiz, que siempre es la ventana viva)
  se **re-escalan automaticamente** por la razon de tamano (tope x4,
  identidad si cambio < 2%): la misma escena en otra compu o
  resolucion reaparece en su lugar. Archivos viejos, sin linea `G`,
  cargan exactos como siempre.

## Compilar

Con GCC (MinGW64 en Windows):

```
g++ -O2 -std=c++11 src/Ser.cpp -o ser.exe -luser32 -lgdi32
```

En Dev-C++ (Proyecto Win32 GUI, C++11) o Makefile.win → target `Ser.o` +
enlazado con `-luser32 -lgdi32`. Con ventana abre un editor de controles:
botones, checks, sliders, redondeados, caches, sesiones anidadas.

Sin modificar nada, `main()` corre primero la batería de pruebas (122/122 PASA,
sin ventana) y escribe `ser_check.ppm` 800x520 como render de verificación.

## Formato

- Nodo: `struct Ser` con `fol` (padre), `A[]` (hijos, orden = z-order),
  etiqueta `n2[2]` y tipo `tc` char (`r` rect, `p` panel, `b` boton,
  `k` check, `s` slider, `l` texto, `e` editor, `c` contenedor,
  `o` cubo/textura, `q` sesion, `m` camara, `f` maquina,
  `a` mezclador).
- Banderas `CI_FLG`: bit 1 visible, bit 2 marcado, bit 4 de editor,
  **bit 8 = pin**: primer plano (se colan al final del z-order, pick
  los gana) e inamovible (sin `mandaAlFrente`, sin arrastre).
- Camara `m`: fila `TM` char `'x'` = `[img,w,h,ver]`. Se alimenta con
  `ordFeed` (orden `k==6` del escritor). Sin fuente pinta `CAM`.
  El bezel `Cam` de la escena modelo esta fijado (FLG 1|8) y contiene
  al control de camara; con ventana corre una fuente sintetica animada.
- Matrices `M[i]` con identificador `TM[i]` char*: `'f6'` = fila SM
  `[cond,ci,cmp,op,dst,arg]`, `'g'` = cache de campocromatico
  (`long long[4]`: img, w, h, version), `'s'` = texto,
  `'au'` = buffer de audio (`double[2+n]`: usados, capacidad, samples).
- Filas SM: op `0=set 1=add 2=toggle 3=clip 4=mul` y op `5=crb`:
  crea rama hija de tipo `(char)dst` bajo el nodo script, con tope
  `arg` (`nA < arg`) => despliegue en cascada sin duplicados. Es el
  meta-UI: filas de condicion/asignacion que generan controles.
  Ejemplo: `f6Add(p, 3, CI_ABI, 1, 5, 'b', 3)` despliega un boton
  la primera vez que el panel abre.
- Z-order multiple: op `7` del escritor con `r[0]`: 0 = al fondo
  (shift a `A[0]`), 1 = un paso adelante, 2 = un paso atras (swaps).
  El viejo "al frente" sigue siendo op `3`.
- Counters en `volatile long`; escritor unico drena `oRing[256]`
  (`oHead`/`oTail` volatiles).
- Persistencia: serializacion `N idx fol tc x y w h r g b a abi flg val etiqueta`
  (la etiqueta va AL FINAL: puede tener espacios sin romper la carga) y
  `V idx c0..c5` (los 6 double con `%g`, sin truncado), round-trip
  verificado por prueba (incluye `CI_VAL` y con decimales), mas
  `W idx + 9 doubles` para contadores `n9`. La linea
  `N ... -1` mapea a la raiz ya existente (no duplica). Los numeros ya
  viajan en punto fijo (sin re-escalar al cargar): el desajuste viejo
  multiplicaba `260*QZ` otra vez al recargar y desbordaba el cache en
  Win32 (`long` de 32 bits) — corregido y con topes (2048) + aritmetica
  en `size_t` en cache/camara. **Migracion automatica**: los
  `oroga.dbg` del formato viejo (etiqueta en el campo 4) se detectan
  solos al cargar y se traducen en el acto — no hace falta borrar el
  archivo; la primera guardada lo reescribe en formato nuevo
  (unico caso sin remedio: un archivo corrupto ya re-guardado por la
  sesion de los puntitos, ahi si borralo una vez). **Autosave**:
  al cerrar la ventana (`WM_DESTROY`) se volca `oroga.dbg` con la
  sesion completa. Recortes con aviso en stderr: arbol > 1024 nodos,
  cola `oRing` llena (la orden se pierde, no corrompe); la lista
  colada ya no recorta: crece dinamica al doble.
- Ventana: **resize y maximizar permitidos** — `WM_SIZE` recrea `gImg`
  con el tamaño nuevo, llena el fondo, invalida el frame completo y
  actualiza `gRoot` (el encabezado `BITMAPINFO` se refresca por frame).
  Los paneles fijados (pin) quedan donde están: viven en espacio de
  pantalla, no en mundo.

## Notas LLP64 (Windows x64)

En Windows x64 `long` es de 4 bytes (LLP64) pero los punteros son de 8:
por eso las filas de cache `'g'` usan `long long` (garantizado 64 bits),
no `long`. En Linux (`long` = 8) compila igual.
