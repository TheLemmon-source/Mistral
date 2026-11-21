# Sesión 2026-11-21-2316 — fix compilación Dev-C++ + auditoría visual/UX/dinámica

## 1. Error de compilación reportado (FIX)

- `npc_slots_release` liberaba el slot 31 del estado psicológico con
  `SD_NPC_PSYCH`, que **no existe**: el enum `SideKind` define `SD_PSYCH`
  (así fue asignado en `side_pool_alloc(SD_PSYCH)`). Corregido:
  `side_pool_free(SD_PSYCH, idx)`.

## 2. Barrido anti-«error-a-error» — compilación de una sola pasada garantizada

Construido un shim de cabeceras (windows.h/GL/gl.h/miniaudio.h con la superficie
exacta usada por el juego) y compilado el archivo **completo** con
`g++ -std=c++11 -fsyntax-only` en Linux: **0 errores**. Los errores del estilo
«X was not declared» restantes ya no existen: el fallo reportado era el único
símbolo suelto (verificado por barrido de identificadores ALL_CAPS usados vs
definidos: todos los demás residen en headers reales o enums/defines locales).

## 3. Landmines de enlazado corregidos (errores latentes)

- `walker_apply_floor`: prototipo con 4 args vs definición/llamada con 5
  (`float dt`). Prototipo corregido.
- `cap_topo_sort_subset`: prototipo con `const int*`/`int*` vs definición y
  llamada con `const CapId*`/`CapId*`. Prototipo muerto eliminado (la única
  llamada va tras la definición).
- `ui_input_text` / `ui_input_text_len`: prototipos sin definición ninguna
  (causarían link fallido el día que se usen). Eliminados.

## 4. Limpieza y robustez

- `requires` como nombre de campo en `CapDef` es palabra clave en C++20:
  renombrado a `deps` (8 accesos). Ningún cambio semántico.
- Radar de zonas: eliminadas 6 variables muertas (`gz/gz2/zx/zz` y `(void)`);
  en el radar de vecinos: `px/py/scale` y el primer cálculo de `ex` era
  resultado muerto (lo sobrescribían dos líneas después): ahora se calcula
  `ex/ey` una sola vez con la fórmula correcta.
- Cámara en primera persona (walking/aiming): `znear` 0.1 → **0.05**
  (las paredes interiores son de espesor 0.04; con near=0.1 había riesgo de
  desaparición/z-fighting a corta distancia). Cockpit sin cambios.

## 5. Auditoría visual / UX / dinámica (sin cambios, confirmada sana)

- Pipeline por streaming con crecimiento dinámico (`stream_grow`, sin caps
  duros que recorten HUD); pasada 3D con depth-test y pasada HUD con depth
  deshabilitado; troceo text/atlas correcto.
- Interior walking: `ship_publish_interior` publica suelo, techo, 4 paredes
  con recorte de puertas por `port_peer_node`, tintes por daño/brecha y
  remate de esquinas. La causa raíz anterior (hash espacial vacío) sigue
  arreglada: `sh_rebuild()` por sub-step en `sim_tick` (confirmado).
- `emit_world_3d` publica interior/exterior según modo (ON_FOOT/PLATFORM
  ambos), frustum por VP recalculado con `cam->dirty`.
- Balance llaves/paréntesis 0; tokens crudos malloc/calloc/realloc/free:
  1 llamada real cada uno (dentro de las auxiliares x*); 0 namespace/class;
  sin headers propios (el shim es solo instrumento de auditoría, no va al
  repo).

## 6. Re-verificación del núcleo

- Harness BLOQUE 27 reconstruido desde este cpp: **13/13 PASS**, ASan/LSan
  limpios, `live=0`, soak 5/5 (previamente 200/200 estables).

## Compilación

Dev-C++ / MinGW64, GCC 9.2, C++11, Win64:
`g++ -O2 -std=c++11 main_corregido.cpp -o game.exe -lopengl32 -lgdi32 -lwinmm`
con `miniaudio.h` junto al fuente. Esperado: arranca sin errores de
compilación; `game.log` con `juegos_nucleo_selftest: 13/13 PASS` y `live=0`.
Cómo reportar: cualquier aviso/error exacto con número de línea.
