# Sesión 2026-11-21-2304 — Auditoría final previa a compilación

Estado: **sin cambios de código** respecto a `2026-11-21-2252` (`main_corregido.cpp` es
byte a byte idéntico, verificado con `cmp`). Este turno es de verificación: el cpp de
esta carpeta es el artefacto auditado y listo para compilar en Dev-C++ / MinGW64
(GCC 9.2, C++11, Win64) con `miniaudio.h` junto al fuente.

## Resultado de la auditoría (sobre este cpp)

- Tokens crudos `malloc/calloc/realloc`: **1 llamada real cada uno**, dentro de las
  definiciones de `xmalloc/xcalloc/xrealloc` (líneas ~3884–3896). `free`: 1 llamada
  real, dentro de `xfree` (línea ~3903). El resto de coincidencias del texto "free"
  son comentarios/strings/palabras sueltas, no llamadas. Regla 1×1 cumplida
  (auxiliares definidas en ~3086–3902, contadores y `xmem_report`).
- Balance de llaves y paréntesis: **0** (escaneado tras eliminar comentarios y
  literales).
- 0 `namespace`, 0 `class`. Única struct propia del modelo de datos: la celda SER
  (`void** v + int* i + volatile int[20] c + volatile int* lk`). Las structs nativas
  restantes son hot paths permitidos (excepción acordada).
- BLOQUE 27: 50 funciones `ser_*` pequeñas y reutilizables (codec único de cadena
  libre `ser_chain_take/give`, escritura/lectura matriz `ser_mat_write/view`,
  limpieza compartida `ser_cell_detach`, staging `ser_scr_begin/rollback`, etc.).
  Sin definiciones duplicadas reales en todo el archivo (los aparentes duplicados
  son prototipos multilínea y forwards marcados `// forward`).
- Nada de headers propios: únicos includes `miniaudio.h`, `windows.h`, `GL/gl.h` y
  estándar C. Un solotranslation unit.

## Verificación del núcleo (harness standalone sobre BLOQUE 27, GCC + ASan/LSan, Linux)

- **13/13 PASS** (anillo FIFO, celdas sin primitivos, reserva/abort con epoch, ABA
  rechazado, cadena libre LIFO, undo de grupo crear+borrar, post_create/post_delete,
  MPSC 4 productores sin pérdidas, roundtrip ShipNode completo, deformación runtime
  LIFO, bloqueos owner/procesante/modo, anidación padre→hijo, crecimiento del pool).
- **Soak 200/200** ejecuciones estables, exit 0.
- **ASan + LSan: 0 errores, 0 fugas**, `live=0` tras shutdown.
- Stats del núcleo tras la batería: `create=16 delete=3 commit_fail=1 aba=0
  lock_fail=1 growth=4 matreuse=17 live=0`.

## Compilación objetivo (en tu máquina)

Dev-C++ / MinGW64, GCC 9.2: proyecto tipo "Win32 Console/GUI" en C++11, constante
`-DUNICODE -D_UNICODE` si aplica, vincular `-lopengl32 -lwinmm -lpthread` (o
equivalente Dev-C++), con `miniaudio.h` en el mismo directorio. Sin dependencias más.
Al arrancar, el juego ejecuta `juegos_nucleo_selftest()` (línea ~24125 en `app_init`)
y deja el resultado en `game.log` (`13/13 PASS` esperado).

## Cómo llegó aquí (ramas, sin merges a main)

`main` → `vibe/fix-fugas-memoria-6a9162` (PR #2, sesiones 2214 y 2221) →
`Asteroide3D` (PR #3, sesiones 2226, 2248, 2252 y esta 2304).
