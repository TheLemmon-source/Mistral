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
del mundo visible, lo ajusta a una imagen de 64x48 (`gBufCol`) y pinta las
ramas con el color de su nodo, filtradas por puntaje con **tres pesos
volatiles** `gColAnti`/`gColImp`/`gColUrg` (frescura: cada nodo guarda el
tic de su ultimo toque en `s->ulti`, que `ccS` actualiza en cada asignacion;
importancia: pin 900 / abierto 500 / visible 300; urgencia: filas SM 600).
Lo colado alimenta la camara **Colado** (bezuel fijado, `gColNodo`) por el
mismo embudo `gColNva`. Es el esqueleto del colador a imagen del roadmap:
la escena condensada en una imagen que ya se puede anidar, guardar y serializar.

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

## Minusculas 3x5 y arcos con profundidad (3D completo)

- **Minusculas propias en la fuente 3x5**: antes el glifo de una
  minuscula era la mayuscula (`'a'` dibujaba `A`): todo texto se veia
  gritado. Ahora `fGly` tiene 26 glifos minusculos con x-height chico
  (3 filas) y la misma base que las mayusculas: `Colador`, `Rojo`,
  `efectos anti/imp/urg` se leen en case real. Labels nuevos ya se
  escriben en case natural sin trabajo extra.
- **Arcos al fol con profundidad en 3D**: los arcos logicos (filas SM
  con `dst -1-k` = canal `k` del padre) ahora se ven en el modo 3D:
  `pinta3D` dibuja un cable azul del centro proyectado del nodo al
  centro proyectado de su padre para cada nodo que escribe al fol
  (`arFol`). La fila fluye en la escena: el nodo que escribe y el
  canal que recibe quedan conectados visualmente. El trayecto usa el
  proyectado (orbita + zoom) para que gire con la camara.
- Direcciones futuras de la fila `x` (quedan): ticks a profundidad y
  semi-arcos texturizados.

## Compilar

Con GCC (MinGW64 en Windows):

```
g++ -O2 -std=c++11 src/Ser.cpp -o ser.exe -luser32 -lgdi32
```

En Dev-C++ (Proyecto Win32 GUI, C++11) o Makefile.win → target `Ser.o` +
enlazado con `-luser32 -lgdi32`. Con ventana abre un editor de controles:
botones, checks, sliders, redondeados, caches, sesiones anidadas.

Sin modificar nada, `main()` corre primero la batería de pruebas (85/85 PASA,
sin ventana) y escribe `ser_check.ppm` 800x520 como render de verificación.

## Formato

- Nodo: `struct Ser` con `fol` (padre), `A[]` (hijos, orden = z-order),
  etiqueta `n2[2]` y tipo `tc` char (`r` rect, `p` panel, `b` boton,
  `k` check, `s` slider, `l` texto, `e` editor, `c` contenedor,
  `o` cubo/textura, `q` sesion, `m` camara).
- Banderas `CI_FLG`: bit 1 visible, bit 2 marcado, bit 4 de editor,
  **bit 8 = pin**: primer plano (se colan al final del z-order, pick
  los gana) e inamovible (sin `mandaAlFrente`, sin arrastre).
- Camara `m`: fila `TM` char `'x'` = `[img,w,h,ver]`. Se alimenta con
  `ordFeed` (orden `k==6` del escritor). Sin fuente pinta `CAM`.
  El bezel `Cam` de la escena modelo esta fijado (FLG 1|8) y contiene
  al control de camara; con ventana corre una fuente sintetica animada.
- Matrices `M[i]` con identificador `TM[i]` char*: `'f6'` = fila SM
  `[cond,ci,cmp,op,dst,arg]`, `'g'` = cache de campocromatico
  (`long long[4]`: img, w, h, version), `'s'` = texto.
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
