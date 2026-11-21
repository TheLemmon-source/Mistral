# Sesión 2026-11-21-2323 — Arreglo visual/UX + menú principal

Base: `../2026-11-21-2316/main_corregido.cpp` (26.879 líneas → 26.907).

## Causas raíz encontradas y corregidas

### (a) Cuadrados amarillos y violetas — `thing_publish_circular`
- **Causa:** los cuerpos circulares sin material (`ET_ANOMALY` violeta, `ET_FIELD`
  azul, `ET_STAR` amarillo, `ET_ALIEN`, `ET_CAPSULE`) usaban UV 0..1 = el atlas
  entero (2048x2048 con decenas de tiles) en el billboard central.
- **Fix:** slot 0 del atlas reservado para un **disco blanco procedural con alfa
  suave** (`atlas_disc_pregenerar`, tint por `rcmd_color`); nuevo auxiliar único
  `atlas_tile_uv_rect` (reutilizado por `material_tile_uv` y `atlas_disc_uv`).
- **Reutilización:** eliminado el bucle muerto de semi-arcos (`RCK_LINE3D` no se
  renderiza en ninguna parte del pipeline: 20 rcmd muertos por entidad/frame) y
  la macro `PUB_ARC_SEGS` era código muerto.

### (b) Menú principal ni aparece — `menu_render` era código muerto
- **Causa raíz real:** `menu_render()` existía y funcionaba, pero **no estaba
  registrado en `g_hud_sections`** → ningún menú se dibujó jamás (ni siquiera la
  pausa).
- **Fix:** `menu_render` registrado (14→15 secciones); nuevo `MENU_MAIN`
  ("ASTEROIDE 3D": COMENZAR / SALIR); arranque en `CTX_MENU` + `MENU_MAIN` con
  sim pausado (el universo ya no se simula detrás del menú); `menu_cancel` y
  `act_pause` guardan el menú principal.
- **Ratón para menús:** `render_pick_test` y `pick_push` también eran código
  muerto (hover/click nunca conectado). Ahora: hover selecciona, click confirma
  en `menu_repeat_tick`; `g_pick_n` reseteado por frame (crecía sin tope útil).

### (c) Panel de computadoras no reacciona
- **Causa:** en PILOT el ratón está capturado y re-centrado en WM_MOUSEMOVE →
  `touch_focus` siempre falso → pestañas inclicables; y el ciclo de página por
  `ACT_TABLET` era inalcanzable (ctrl+tab cambia de contexto y limpia los flags
  `pressed` antes de que el tick del panel los lea: doble disparo potencial o
  acción muerta según orden).
- **Fix:** teclas dedicadas `F7` (página anterior) / `F8` (página siguiente) vía
  acciones nuevas `ACT_PANEL_PREV`/`ACT_PANEL_NEXT` con gate por contexto/modo;
  eliminado el ciclo por `ACT_TABLET`; hint en el footer del panel.

### (d) Paredes de nave invisibles en walking — hardening de publicación
- La cadena de publicación interior era correcta en estático, pero dependía de
  que `sh_query_sphere` devolviera la nave propia (hash espacial + tope de 32
  cosas más cercanas: si el campo de asteroides llena la query, la nave propia
  se cae de la lista y **no se publica ni exterior recortado ni interior**).
- **Fix:** la nave propia se publica **directamente e incondicionalmente** en
  `emit_world_3d` cuando el jugador está ON_FOOT/PLATFORM (independiente del
  hash); el resto de naves siguen por hash. Diagnóstico en juego: F3 (overlay
  `CELLS int/ext/world`).

### (e) Asteroides OK
- Confirman atlas/tiles correctos cuando `mid` está mapeado (no tocados).

## Verificación
- `g++ -std=c++11 -fsyntax-only` con shim Win32: **0 errores**.
- Harness BLOQUE 27 (22708-24136 + prelude): **13/13 PASS**, `live=0`,
  ASan/LSan limpios (BLOQUE 27 sin tocar).
- Tokens de memoria 1×1 verificado: `malloc` (3892), `calloc` (3898),
  `realloc` (3904), `free` (3911) solo dentro de `x*`.

## Reglas respetadas
- Un solo cpp por turno, rama `Asteroide3D`, sin merge a `main`, sin repos nuevos.
- Hot paths nativos intactos; sin dependencias nuevas; C++11/MinGW64.
- Números de línea citados tras las ediciones de esta sesión.
