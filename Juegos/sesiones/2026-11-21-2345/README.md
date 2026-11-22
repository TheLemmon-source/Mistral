# Sesión 2026-11-21-2345 — Plataforma de creación de naves + economía + forja/ensamblaje (BLOQUE 17e)

Base: `../2026-11-21-2323/main_corregido.cpp`. Único archivo de este turno:
`main_corregido.cpp`. Reglas respetadas: rama `Asteroide3D` (sin merge a main,
sin repos nuevos), C++11/MinGW64, sin dependencias nuevas, hot paths nativos.

## Boot lanzadera-tutorial
- El juego arranca el jugador en la **plataforma-lanzadera** (`ship_gen_lanzadera_platform`),
  modo de arranque tipo tutorial: caminar, cargar recursos, producir, ensamblar.

## Economía (turno 8, ya integrado aquí)
- Paga por recursos (tecla V: entrega y cobro), minería compartida desde torres
  de la lanzadera (drone pesca-asteroides, 75 % al operador).
- `sh_query` sin topes hardcodeados: scratch dinámico (`sh_query_dyn`), el hash
  espacial ya no deja fuera lo que no entre en una lista fija.

## Sala radar grande + panel
- Panel PC navegable (F7/F8 de la sesión previa); blips de radar con raid
  temprano visible.

## BLOQUE 17e — Producción en plataforma (forja → partes → carga → ensamblaje)
- **K** (`ACT_FORGE_QUEUE`): junto a la forja consume `FORGE_PART_COST` (8) de
  materias primas y encola un trabajo; la forja (`forge_step_process_queue`)
  al terminar **produce un cajón de parte** (`part_crate_spawn`, pickup
  `PICKUP_TYPE_PART` con kind cíclico y calidad por dado Q1/Q2/Q3).
- Recoger el crate (**E**/contacto → `pickup_collect`) lo mete en **carga del
  jugador** (`PartCrate g_player_cargo`, push crece x2).
- **J** (`ACT_ASSEMBLE`): cerca de la nave propia instala la mejor parte:
  - componente faltante → `component_spawn` + `node_attach_comp` a nodo walkable
    ("PART INSTALLED");
  - componente existente → blindaje (`maxhp += 25*calidad`, boost de HP, estado
    reparado si estaba a media vida) ("PART BOLTED (+HP)").
  Autorun de ensamblaje por pipeline (`sim_stage_assembly`, etapa 42, gate 0.5 s).
- `platform_forge_ensure`: la plataforma siempre tiene su forja propia
  (`ForgeData.owner_slot` enlazado a la nave-plataforma).
- 2 **operarios defensores** (`crew_spawn`) en la plataforma desde el boot
  (sala radar + storage).
- **Raids a la plataforma** (`pirate_wave_tick`): objetivo preferente la
  plataforma con oleadas escaladas (`1 + rng%3 + wave/2`), notice
  "RAID ON THE PLATFORM".

## Fugas residuales: causa raíz y fixes
- **OOB `g_params[48][P][E]` en `mat_pool_add`** (global-buffer-overflow de ASan):
  el pool de materiales puede crecer más allá de `MATERIAL_COUNT`; el write de
  `g_params` ahora está guardado por `idx < MATERIAL_COUNT` (los materiales
  extensionales usan fallback procedural por seed en el emisor).
- **Side pools con stride incoherente** (ética estructural): `side_pool_free` /
  `side_pool_alloc` indexaban los slots con paso 8 B mientras `side_pool_at`
  indexaba con `sizeof(struct)` → para idx≥1 lecturas desalineadas, punteros
  basura y liberaciones equivocadas, en **todos** los pools de punteros (ships,
  componentes, armas, AI, missions, forjas, regimes...).
  Fix: `SidePool.data_stride` (8 para pools de punteros, `elem_size` para pools
  planos) usado coherentemente en init/crecimiento/at/free/destroy; los slots
  de punteros llevan su struct xcallocado real.
- **Patrón roto de spawn en 5 tipos** (`void** sp = entity_attach_side(...)` +
  xcalloc + `*sp = td`): `entity_attach_side` devuelve el *valor* del puntero
  del slot, no un `void**` del slot → el struct nuevo quedaba huérfano (fuga) y
  el pool liberaba el equivocado. Corregido al patrón directo (como
  `ship_spawn`) en `ideology_spawn`, `regime_spawn`, `social_spawn`,
  `contract_spawn`, `tournament_spawn`.
- **Live=2 final identificado**: era la **morfología de los 2 operarios**
  (`body_gen` → `body_block_push`) viva en `g_crew[]` sin liberar en shutdown.
  Fix: auxiliar `crew_release_all()` (libera morfologías + reset) reutilizado
  por `crew_init` y llamado en `app_shutdown`.

## Verificación
- Sintaxis `g++ -std=c++11 -fsyntax-only` con shim Win32 en Linux: **0 errores**.
- Smoke ASan 400 frames headless: **RC=0**, `xmem: live=0 alloc=237 free=237`,
  ASan/LSan sin reportes.
- Harness BLOQUE 27 (autónomo): **13/13 PASS**, `live=0` (BLOQUE 27 sin tocar;
  los fixes de side pools son de la capa legacy del juego).
- Tokens 1×1 intactos: `malloc`/`calloc`/`realloc`/`free` solo dentro de
  `x*` con contadores (`xmem` con registry/dladdr usado solo como
  instrumentación de diagnóstico y **retirado antes del commit**).

## Transitorios / limitaciones conocidas
- Carga de partes (cajones) y cargo del jugador es **transitorio** (no entra
  todavía en save v3): al recargar se reinician. Queda como pendiente.

## Roadmap (visión original, próximos turnos)
- **Herramientas de mano** construidas desde materias primas y energías:
  generar / refaccionar / destruir / descomponer / cambiar comportamiento de
  otras cosas; resultantes desastrosas o espectaculares.
- **Comerciantes aliens** con intercambio real (economía extendida a facciones).
- **Comportamientos de materia/energía**: viscosos, líquidos, gaseosos, inertes,
  reactivos, radiaciones/aislaciones, afecciones microbianas (bacterias que
  sobrecrecen enormes, tentáculos, hoyos de succión/exhalación). Base existente:
  `tag_mask` / `afflictions` parcial en materiales y salas.
- **Constituciones por patrones de continuidad**: brazos → apéndices
  articulados encadenados; amorfos/insectoides/crustáceos/plantas/robots
  (`HumanoidMorph` ya soporta SPECIES_* → extensible a armas, trajes,
  herramientas).
- **Calidad de modelos por composición**: manos con dedos contables, cabinas
  con asientos y parabrisas (el camino es composición de partes, no sprites).
- **Transporte de mayor porte**: mover zonas/salas enteras al área de ensamblado
  final (hoy J instala partes chicas; falta el equivalente "grúa").
- Drones pesca de asteroides ya parcial (`share_mined`); falta zona tipo
  "lluvia de asteroides" señalizada en el radar.
