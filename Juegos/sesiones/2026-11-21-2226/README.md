# Sesión 2026-11-21-2226 — Juegos núcleo: escritor único, anillo MPSC CAS, pool con epoch y undo por grupos

Línea de desarrollo continua — rama `Asteroide3D` (creada desde la sesión 2221), sin merge a main.
Base: `Juegos/sesiones/2026-11-21-2221/main_corregido.cpp`.
Organización: directorio `Juegos/` con las sesiones movidas dentro (`git mv`, sin cambios de contenido).

## 1. Arquitectura: escritor único

El árbol de juego solo lo toca un hilo (el escritor, el main). Cualquier otro
hilo **no** toca estado: encola una orden en un anillo MPSC y lleva consigo un
snapshot (clon de la base) dentro de la orden. El escritor drena el anillo en
`app_step` (`juegos_nucleo_drain(64)`). Objetivo cumplido: cero condición de
carrera sobre estado — solo hay 3 puntos de sincronización, todos CAS.

```
hilos productores ──(order + snapshot clonada)──► anillo MPSC (CAS en tail)
                                                      │
escritor (main, app_step) ◄───── jng_ring_pop (CAS head)
     │
     ├─ fase RESERVED: el índice ya está reservado desde jng_post_create
     ├─ commit/epoch válido  → instala nodo (payload zerado + snapshot)
     ├─ epoch inválido (ABA) → fallback: crea nodo nuevo desde la snapshot
     └─ NG_GROUP_CLOSE → cierra el grupo de undo
```

## 2. Pool de nodos con CAS, fases y epoch (`NgPool`, 256 nodos)

- `jng_pool_reserve()` saca un índice de la freelist con **CAS** y entrega
  `epoch = epoch_actual + 1`. Fases: `IDLE → RESERVED → COMMIT | ABORTED`.
- `jng_pool_commit()` exige `nd->epoch + 1 == epoch_reservado`: un commit
  obsoleto (reuso/ABA) es rechazado y contado (`stat_commit_fail`).
- `jng_read(idx, epoch)` valida epoch antes de devolver payload: los lectores
  nunca ven un nodo "fantasma" reciclado (`stat_aba_guard`).
- Freelist empaquetada en un `LONG64` (`top:32 | idx+1`), con push CAS al
  liberar: el epoch del nodo se incrementa en cada liberación, así el ABA
  clásico de la freelist queda descartado.
- **Reuso sin fantasmas**: en `commit` el payload se `memset` a 0 antes de
  copiar la snapshot (canales zerados al reusar).
- Tamaño de payload: `sizeof(ShipNode)` redondeado a 8 (160 B) — un nodo del
  árbol de nave cabe completo como snapshot.

## 3. Deshacer real por grupos

Cada grupo (`NgGroup`, hasta 64 apilados) acumula inversos en orden:
- un `create` graba inverso `DELETE`; un `delete` graba inverso `CREATE` con
  la snapshot completa del nodo. Creados y borrados mezclados se sostienen.
- `juegos_nucleo_undo()` repite los inversos del grupo actual **al revés**
  (los recreados vuelven con su payload; los creados se borran) y cierra.
- Nueva acción distinta → grupo nuevo (`jng_ensure_group_open` / orden
  `NG_GROUP_CLOSE` por el anillo).

## 4. Correcciones respecto al borrador intermedio

- `NgPool.freelist` existía como concepto pero faltaba como campo → añadido.
- `NG_PAYLOAD_BYTES` / `NG_INVERSE_CREATE/DELETE` sin definir → definidos
  antes de las estructuras.
- `NgCellHold` (typedef fantasma en `jng_ring_push`) → eliminado.
- `jng_pool_reserve()` escribía `*out_epoch` sin guard null → añadido guard.
- `juegos_nucleo_init()` tenía doble construcción de freelist y no activaba
  `g_jng_inited` → una sola pasada inversa y bandera al final.
- **Bugs reales detectados al ejecutar** (harness de pruebas): la `seq` del
  anillo de Vyukov debe arrancar en `seq[i] = i`, no en 0 (con calloc el
  anillo se bloqueaba tras el primer pop) y el aserto ABA de la prueba
  comparaba epochs mal. Ambos corregidos.

## 5. Conexión (3 llamadas, escritor único preservado)

| Llamada | Sitio |
|---|---|
| `juegos_nucleo_init(); juegos_nucleo_selftest();` | `app_init`, tras `log_open("game.log")` |
| `juegos_nucleo_drain(64);` | `app_step`, tras `input_process_queue()` (hilo escritor) |
| `juegos_nucleo_shutdown();` | `app_shutdown`, **antes** de `xmem_report` (sus frees entran en el balance) |

## 6. Autoprueba (9 objetivos, estilo N/N PASS en `game.log`)

| # | Objetivo |
|---|---|
| 1 | Anillo FIFO con un hilo (16 órdenes ordenadas) |
| 2 | Reserva/commit/lectura con epoch (payload intacto, zona no escrita zerada) |
| 3 | Fase `ABORTED` tras abortar una reserva |
| 4 | Commit con epoch ajeno rechazado (ABA) y el índice re-reservable |
| 5 | Reuso sin fantasmas (canales zerados al reciclar) |
| 6 | Reservas agotadas y liberadas (pool seco y recuperación) |
| 7 | Undo de grupo (crear+borrar mezclados) re-vive nodos |
| 8 | `post_create`/`post_delete`/cierre de grupo por órdenes del anillo |
| 9 | Anillo MPSC con 4 productores (512 órdenes) sin pérdidas |

## 7. Verificación

- Sintaxis C++11 pura del módulo compilada sin avisos (`-Wall -Wextra`,
  g++ syntax-only con stubs de los tipos/API Win32) y **ejecutada** con
  equivalencias pthreads/GCC: `9/9 PASS`, y el balance vía auxiliares `x*`
  cierra: `live=0` tras `juegos_nucleo_shutdown()`.
- Estabilidad: 200 corridas consecutivas de la autoprueba sin fallos.
- Todos los tokens crudos siguen en 1: `malloc`/`calloc`/`realloc`/`free`
  solo dentro de `xmalloc/xcalloc/xrealloc/xfree`; el módulo usa solo `x*`.
- Balance de llaves y paréntesis: 0/0 fuera de strings.
- Compilación real (Win64, MinGW64/GCC 9.2, Dev-C++):
  `g++ -O2 -std=c++11 main_corregido.cpp -o game.exe -lopengl32 -lgdi32 -lwinmm`
  (requiere `miniaudio.h` junto al fuente). Esperado en `game.log`:
  `juegos_nucleo_init: pool=256 ring=1024 payload=160`, `jng test: ... -> PASS`
  × 9, `juegos_nucleo_selftest: 9/9 PASS`.

## 8. Pendiente conocido (fuera de alcance de esta sesión)

- "Cuadrados flotantes" residuales: `thing_publish_circular` con `mid==0`
  publica el UV del atlas completo (0..1 = basura visual) y el marcador de
  nave lejana sigue en blanco crudo; revisar en próxima sesión si el fix del
  hash espacial (2221) no los elimina del todo.
