# Sesión 2026-11-21-2221 — memoria centralizada + fix de hash espacial

Línea de desarrollo continua — rama `vibe/fix-fugas-memoria-6a9162`, sin merge a main.
Base: `sesiones/2026-11-21-2214/main_corregido.cpp`.

## 1. Auxiliares de memoria (uno por caso en todo el código)

Antes: 72 `malloc/calloc` vs 55 `free` dispersos en 25.000 líneas. Ahora
`malloc / calloc / realloc / free` aparecen **exactamente una vez cada uno**,
dentro de los auxiliares (`xmalloc`, `xcalloc`, `xrealloc`, `xfree`) y el resto
del código (137 call sites) usa solo `x*`. Los auxiliares:

- aceptan tamaño 0 sin UB (`n ? n : 1`);
- cuentan `live / total_alloc / total_free` en tiempo de ejecución;
- `xmem_report(tag)` escribe el balance en `game.log` (llamado en `app_shutdown`).

## 2. Paredes invisibles en modo walking + cuadrados flotantes (causa raíz común)

`emit_world_3d()` (exterior + interior de naves) y toda la percepción IA,
combate, minería, explosiones y creatures dependen de `sh_query_sphere()`
(hash espacial). Pero `sh_rebuild()` solo se llamaba en `entity_pool_init()`,
cuando aún no hay entidades: el hash quedaba vacío **toda la partida**.

Resultado exacto de los síntomas reportados:
- `sh_query_sphere` devolvía 0 → `ship_publish_exterior/interior` jamás se
  ejecutaban → **sin paredes/suelo/techo de la nave en modo walking**.
- Las naves cercanas (< 500 u) se saltaban del loop de billboards
  (`node_count > 0 && dist < 500 → continue`) y no tenían geometría en
  ningún otro sitio → el mundo se veía como **cuadrados/billboards
  flotantes** solo con proxies.
- Idem para IA ciega, explosiones sin radio, patrols sin spawns.

Fix: `sh_rebuild()` se llama una vez por cada sub-step de `sim_tick()` antes de
`sim_pipeline()`; render y AI leen posiciones del sub-step vigente.

## 3. Verificación

- Todos los tokens crudos verificados: 1 `malloc`, 1 `calloc`, 1 `realloc`,
  1 `free` (dentro de los auxiliares), cero fuera.
- Balance de llaves/paréntesis intacto respecto a la sesión previa.
- En Windows (MinGW64/GCC 9.2): `g++ -O2 -std=c++11 sesiones/2026-11-21-2221/main_corregido.cpp -o game.exe -lopengl32 -lgdi32 -lwinmm`
  (requiere `miniaudio.h` junto al fuente).
- Confirmación visual esperada en `game.log` cada 300 frames:
  `cells: interior=N exterior=M` con N,M > 0 (antes `interior=0 exterior=0`).
