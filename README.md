# Ser

Sistema universal en C++: una sola rama anidable (`struct Ser`) donde todo termina
siendo **condición y asignación**. Sin structs especializados, sin enums, sin
hardcodeo: cada nodo guarda su estado en matrices dinámicas `M[]` dirigidas por
descriptores `TM[]`, en punto fijo (`QZ = 256`).

Un único archivo: `src/Ser.cpp` (~1040 líneas), compilable en Windows x64
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

## Compilar

Con GCC (MinGW64 en Windows):

```
g++ -O2 -std=c++11 src/Ser.cpp -o ser.exe -luser32 -lgdi32
```

En Dev-C++ (Proyecto Win32 GUI, C++11) o Makefile.win → target `Ser.o` +
enlazado con `-luser32 -lgdi32`. Con ventana abre un editor de controles:
botones, checks, sliders, redondeados, caches, sesiones anidadas.

Sin modificar nada, `main()` corre primero la batería de pruebas (11/11 PASA,
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
- Persistencia: serializacion `N idx fol tc label x y w h r g b a abi flg val`
  y `V idx c0..c5` (los 6 double con `%g`, sin truncado), round-trip
  verificado por prueba (incluye `CI_VAL` y con decimales), mas
  `W idx + 9 doubles` para contadores `n9`. La linea
  `N ... -1` mapea a la raiz ya existente (no duplica). **Autosave**:
  al cerrar la ventana (`WM_DESTROY`) se volca `oroga.dbg` con la
  sesion completa. Recortes con aviso en stderr: arbol > 1024 nodos,
  colado > 1024, cola `oRing` llena (la orden se pierde, no corrompe).
- Ventana: sin resize ni maximizar (el buffer `gImg` es fijo `gW x gH`,
  recrearlo quedo para cuando haya camara global). Minimizar/restaurar
  repinta bien (tamanio constante).

## Notas LLP64 (Windows x64)

En Windows x64 `long` es de 4 bytes (LLP64) pero los punteros son de 8:
por eso las filas de cache `'g'` usan `long long` (garantizado 64 bits),
no `long`. En Linux (`long` = 8) compila igual.
