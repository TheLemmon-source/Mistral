# Sesión 2026-11-21-2248 — BLOQUE 27 v2: celda universal SER (una única struct emula cualquier struct)

Línea de desarrollo continua — rama `Asteroide3D` (actualiza el PR existente en sitio), sin merge a main.
Base: `Juegos/sesiones/2026-11-21-2226/main_corregido.cpp`.
Objetivo del turno: rediseño estructural antes de compilar. Todo el estado del
núcleo vive ahora en **un único tipo de datos** que emula cualquier struct,
fin de los nodos de payload plano (`NgNode`) del turno anterior.

## 1. La celda universal (`Ser`, única struct propia del sistema)

```
Ser = void**         v   (ramas: matrices dinámicas alojadas en los voids)
    + int*           i   (índices/enteros: matriz dinámica)
    + volatile int   c[20]   (conteos y bloqueos: lo único fijo)
    + volatile int*  lk      (bloqueos secundarios por rama/void)
```

C[0..4]   tabla v: usados, asignados (cap), tope, primerlibre, ultimolibre
C[5..9]   tabla i: idem
C[10..14] tabla lk: idem (paralela a v; comparte su cadena libre)
C[15] owner · C[16] procesante (quien la tiene abierta) · C[17] modo de bloqueo
C[18] tipo · C[19] fase (IDLE / RESERVADO / COMPROMETIDO / ABORTADO)

Invariante central: **ninguna primitiva vive nativa dentro de los voids.** Todo
elemento es una matriz dinámica con cabecera de 4 ints
`[tipo, elemsize, count, cap]` y datos. Tipos de matriz: `SER_TM_INT`,
`SER_TM_FLOAT`, `SER_TM_BYTES`, `SER_TM_CELL`. Las ramificaciones anidan
serializadores (celdas dentro de celdas) hasta la profundidad que se pida
(`SER_SER_DEPTH_MAX`), con guarda de ciclos. `ser_branch_dim` es la operación
"deformar en runtime": la celda crece una rama nueva con la forma pedida o
reusa un hueco de la cadena libre (LIFO).

## 2. Pool de celdas (CAS + epoch, matrices dinámicas)

- Freelist empaquetada en `g_ser_top` (`stamp<<32 | idx+1`): cada cambio hace
  CAS con incremento de stamp → anti-ABA.
- Handles de 64 bits `SER_REF(idx, epoch)`; `ser_at()` valida epoch y fase
  antes de devolver la celda.
- Pool que crece en x2 bajo demanda (`ser_pool_grow`), todo con
  `xmalloc/xrealloc` (matrices dinámicas, cero arrays estáticos).
- Reuso sin fantasmas: `ser_commit` zeró C[] y libera restos; `release` zeró
  canales y suelta las matrices de cada rama.
- Reservas/fases: `IDLE → RESERVADO → COMPROMET / ABORTADO`; un commit con
  epoch ajeno es rechazado y contado.

## 3. Anillo MPSC (igual que 2226 pero con matrices dinámicas)

`g_seq` (volatile LONG64*, `seq[i]=i` al init), `g_meta` (6 ints/orden) y
`g_pay` (matriz serializada que el productor asigna y el escritor libera).
CAS en tail (push) / head (pop). Capacidad 1024, single consumer (escritor).
El pop reconstruye el **handle completo** (`epoch<<32 | idx`), no solo el epoch.
El group id de cada orden llega al undo para agrupar correctamente.

## 4. Deshacer real por grupos

Pila global de inversos `g_u_meta` (8 ints/rec) + `g_u_pay` (payload
serializado o NULL), agrupados por id de grupo (escaneo hacia atrás):
- `create` aplicado → inverso `DELETE`; `delete` aplicado → inverso `CREATE`
  con la celda serializada completa (revive idéntica, bytes incluidos).
- `juegos_nucleo_undo()` deshace el grupo abierto al revés.
- Compactación a mitad más vieja al rebasar `SER_RECS_MAX` (4096).

## 5. Bloqueos (owner / procesante / modo)

- `SER_LOCK_NONE/READ/WRITE/EXCL` vía CAS32 en `c[SER_C_LOCK]` +
  `c[SER_C_OWNER]`; `c[SER_C_PROC]` registra el procesante abierto.
- `lk[k]` por rama. Codificación de la cadena libre: `lk[k] < 0` ⇒ rama libre
  (`-1` fin, `-(next+2)` enlace) → tomar bloqueo sobre rama liberada es
  imposible y `LOCK_NONE (0)` no colisiona con la cadena.

## 6. Excepción hotpaths (regla del turno)

Las 137 structs legacy del juego (Camera, ShipNode, ShipEdge, Thing, Humanoid*,
Npc*, Crew*, Knowledge*, …) siguen intactas **solo** en hot paths y secuencias
de iteración de velocidad (render/IA). El BLOQUE 27 v2 ya no define ninguna
struct propia además de `Ser`; la prueba 9 serializa un `ShipNode` completo a
través de la celda (roundtrip byte a byte) como puente hacia la migración
gradual por serializadores.

## 7. Autoprueba (13 objetivos, N/N PASS en `game.log`)

| # | Objetivo |
|---|---|
| 1 | Anillo FIFO 1 hilo (16 órdenes, orden exacto con handle completo) |
| 2 | Celdas: rama BYTES y FLOAT con cabecera válida, celda vecina intacta |
| 3 | Reserva y abort (liberación con epoch++) |
| 4 | Commit con epoch ajeno rechazado (ABA) e índice re-reservable |
| 5 | Cadena libre v: primerlibre/ultimolibre y reuso LIFO |
| 6 | Undo de grupo (crear+borrar) re-vive con bytes idénticos |
| 7 | post_create/post_delete/cierre de grupo por órdenes del anillo |
| 8 | Anillo MPSC 4 productores (512 órdenes) sin pérdidas |
| 9 | Serializador plano: ShipNode completo roundtrip + cabeceras válidas |
| 10 | Deformación runtime: ramas LIFO + islots (i-tabla) con reuso |
| 11 | Bloqueos owner/procesante/modo + lk por rama (también sobre rama libre) |
| 12 | Anidación de serializadores: padre→hijo roundtrip + payload embebido |
| 13 | Pool agotado: crecimiento dinámico x2 y recuperación |

## 8. Correcciones respecto al borrador del módulo (detectadas al ejecutar)

- `ser_pool_grow`: la rama exitosa **no asignaba** `g_ser_pool/epoch/next`
  (pool NULL con cap avanzada) → asignaciones restauradas.
- `ser_commit`: tras zerar C[], los centinelas de cadena libre quedaban a 0
  (rebote contra la rama 0) → `FREE1/FREETOP` v/i/lk a `-1`.
- Decodificación de la cadena libre: pop devolvía el valor codificado
  (`-1-next`) sin decodificar → cadena corrupta al 2º reuso.
- `ser_grow`: condiciones `which` invertidas (crecía la tabla i en vez de v en
  `ser_branch_dim` → `k=-1` y derreferencia fuera de rango).
- `ser_post_create`: el body se alojaba como matriz BYTES (esz=1) pero se
  escribía como ints+payload → **heap-buffer-overflow**; ahora esz=sizeof(int).
- `ser_pool_release_idx`: liberaba los arrays v/i/lk pero **filtraba las
  matrices apuntadas dentro de v[]** (la fuga del módulo anterior: `live=20`
  tras shutdown ha quedado reparado).
- `ser_ring_pop` perdía el idx al reconstruir el handle (bits bajos con el
  epoch) → FIFO/MPSC/drain fallaban; ahora devuelve `SER_REF(idx, epoch)`.
- `juegos_nucleo_drain` descartaba el group id del pop → el undo agrupaba
  mal; propagado a `ser_apply_create/delete`.
- Prueba de anidación liberaba el padre antes de capturar el hijo → fuga;
  ahora se captura `c2h` y se libera hijo antes que padre.
- Prueba MPSC: `seen[4]` con 128 índices distintos → `seen[128]` (4 por
  índice con 4 productores).
- `ser_deser_branch`: validación de `bk` (índice de rama de origen) restaurada
  (`0 <= bk < v_top` de la fuente).
- Getters/operaciones sin usar ejercitados en la autoprueba: `ser_ihdr_cap`,
  `ser_set_floats/ser_get_floats`, `ser_ifree`, `ser_child_nested`.

## 9. Conexión (3 llamadas, sin cambios respecto a 2226)

| Llamada | Sitio |
|---|---|
| `juegos_nucleo_init(); juegos_nucleo_selftest();` | `app_init`, tras `log_open("game.log")` |
| `juegos_nucleo_drain(64);` | `app_step`, tras `input_process_queue()` (hilo escritor) |
| `juegos_nucleo_shutdown();` | `app_shutdown`, **antes** de `xmem_report` |

## 10. Verificación

- Ejecución real del módulo con equivalencias pthreads/GCC de la API Win32:
  **13/13 PASS**, y balance de memoria cierra: `live=0` tras
  `juegos_nucleo_shutdown()`.
- Estabilidad: 200 corridas consecutivas → **200/200** sin fallos.
- AddressSanitizer + LeakSanitizer (build `-O0 -g`): 0 errores, 0 fugas.
- ThreadSanitizer: solo señala las lecturas `seq` no atómicas del anillo de
  Vyukov, revalidadas por CAS (patrón benigno intentional del algoritmo;
  un solo consumidor).
- Tokens crudos en 1 dentro de `xmalloc/xcalloc/xrealloc/xfree`
  (líneas 3883/3889/3895/3902); el módulo usa solo `x*`.
- Balance de llaves y paréntesis: 0/0 fuera de strings.
- Sin structs propias fuera de `Ser`; sin clases; sin namespaces; un solo
  archivo; sin headers propios.
- Compilación real (Win64, MinGW64/GCC 9.2, Dev-C++):
  `g++ -O2 -std=c++11 main_corregido.cpp -o game.exe -lopengl32 -lgdi32 -lwinmm`
  (requiere `miniaudio.h` junto al fuente). Esperado en `game.log`:
  `juegos_nucleo_init: pool=64 ring=1024`, `ser test: ... -> PASS` × 13,
  `juegos_nucleo_selftest: 13/13 PASS`, `live=0`.

## 11. Pendiente conocido (fuera de alcance de esta sesión)

- "Cuadrados flotantes" residuales: `thing_publish_circular` con `mid==0`
  publica el UV del atlas completo (0..1 = basura visual).
- Migración gradual de structs legacy → celdas por serializador (la excepción
  hotpaths se mantiene hasta migrar render/IA).
