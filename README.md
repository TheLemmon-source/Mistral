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
- Counters en `volatile long`; escritor unico drena `oRing[256]`
  (`oHead`/`oTail` volatiles).
- Persistencia: serializacion `N idx fol tc label x y w h r g b a abi flg`
  y `V idx c0..c5`, round-trip verificado por prueba.

## Notas LLP64 (Windows x64)

En Windows x64 `long` es de 4 bytes (LLP64) pero los punteros son de 8:
por eso las filas de cache `'g'` usan `long long` (garantizado 64 bits),
no `long`. En Linux (`long` = 8) compila igual.
