# Next Agent Handoff

Current state of the codebase:

- The renderer is now atlas-backed and no longer depends on the old generated scene colors.
- The temporary placeholder terrain seed has been removed from startup wiring.
- The app currently loads `assets/maps/grass_tileset_map.tmx` by default.
- The app currently resolves the tileset image from the TMX and loads `assets/tiles/grass_tileset_16x16.png`.
- There is an initial TMX loader in:
  `src/assets/tmx_map_loader.h`
  `src/assets/tmx_map_loader.cpp`
- That loader currently parses:
  map metadata
  one tileset
  CSV tile layers
- The current runtime path still uses a flattened tile-map conversion for rendering.
- The current TMX map now includes:
  multiple tile layers
  an object group named `collision`
  a polygon object inside that group
- Camera movement is now correct.
- Right-click move targeting now uses corrected screen-to-world Y conversion.
- Fog sampling was corrected to use world-space coverage instead of texture-size coverage.

Important architectural intent from the user:

- Do not hardcode tile layer names like `Paths`, `Grass Normal`, `Grass Dark`, or `Grass Bright`.
- Treat tile layers generically by authored order:
  layer 0 draws first
  layer 1 draws on top of layer 0
  layer 2 draws on top of layer 1
  and so on
- Object groups and objects may have specific authored names like:
  `collision`
  `water`
  `lava`
  `boundary`
- Objects and object groups may also carry custom properties.
- The loader should preserve unknown content by default and do nothing with it unless the engine implements behavior for it.

What the next agent should do first:

1. Replace the current flattened TMX runtime shortcut with a true ordered multi-layer runtime map model.
2. Extend the TMX loader to parse object groups, objects, polygons, and generic properties.
3. Add runtime map types in `game/` that preserve ordered tile layers and object layers separately.
4. Update render extraction and batching so all tile layers render in authored order without relying on layer names.
5. Add collision polygon ingestion from the `collision` object group, but keep unknown object content inert.

Files the next agent should inspect first:

- `src/assets/tmx_map_loader.h`
- `src/assets/tmx_map_loader.cpp`
- `src/app/application.cpp`
- `src/game/world.h`
- `src/game/world.cpp`
- `src/game/tile_map.h`
- `src/render/render_world.h`
- `src/render/render_extractor.cpp`
- `src/render/batch_builder.cpp`
- `src/render/scene_renderer.cpp`
- `assets/maps/grass_tileset_map.tmx`

# TMX Integration Roadmap

This checklist replaces the older tile-atlas checklist.

The engine is now moving from:

- a placeholder or flattened tile map path

to:

- a real TMX-first world asset pipeline
- multi-layer tile rendering
- generic TMX object ingestion
- optional engine-side interpretation of known content
- safe preservation of unknown content for future systems

The current example map at `assets/maps/grass_tileset_map.tmx` already demonstrates the shape the engine needs to support:

- multiple tile layers authored in order
- one object group:
  `collision`
- one collision object containing a polygon
- tileset image:
  `assets/tiles/grass_tileset_16x16.png`

## Architectural Intent

The engine should treat TMX as the default authored world format.

Design requirements:

- Load TMX by default during startup.
- Support multiple tile layers as first-class data, not as a flattened fallback.
- Preserve all TMX content even if the engine does not yet implement behavior for it.
- Interpret known layer/object/property types through explicit engine systems.
- Ignore unknown data safely at runtime while keeping it available in memory for future use.
- Keep ownership boundaries clean:
  `assets/` parses and owns raw TMX asset data
  `game/` owns runtime world state derived from TMX
  `render/` consumes renderable layers only
  `gpu/` remains unaware of TMX semantics

## Core Principle

The loader should be permissive and data-preserving.

That means:

- tile layers are loaded and preserved by authored order, not interpreted by name
- unknown object types are loaded, stored, and ignored by systems that do not use them
- unknown properties are loaded, stored, and ignored by systems that do not use them
- only known engine integrations produce behavior

This is the key scalability rule for future map concepts such as:

- water
- lava
- boundaries
- spawn zones
- triggers
- patrol paths
- resource nodes
- scripted objects

## Phase 1: Replace Flattened TMX With Full TMX Asset Model

Goal: parse TMX as structured authored data instead of flattening to a single visible tile per cell.

- [x] Replace the current flattened TMX runtime shortcut with a full `TmxMapAsset`
- [x] Add support for these TMX concepts in `src/assets/tmx_map_loader.*`
- [x] Parse map metadata
- [x] Parse tilesets
- [x] Parse tile layers
- [x] Parse object groups
- [x] Parse objects
- [x] Parse object polygons
- [x] Parse generic properties on map, layer, tileset, and object nodes
- [x] Preserve raw names, ids, visibility flags, opacity, and custom properties
- [x] Preserve layer ordering exactly as authored in TMX

Recommended data types:

- `TmxProperty`
- `TmxLayerAsset`
- `TmxTileLayerAsset`
- `TmxObjectLayerAsset`
- `TmxObjectAsset`
- `TmxPolygon`
- `TmxMapAsset`

Exit criteria:

- [x] The full authored TMX structure is available in memory
- [x] No map data is lost just because the engine does not use it yet
- [x] Tile layers are preserved as ordered tile layers, not collapsed by names
- [x] The loader can read the current `collision` polygon object group without special-casing it away

## Phase 2: Introduce Runtime Map Domain Types

Goal: separate parsed TMX data from runtime world data.

- [x] Add `src/game/map_world.h/.cpp`
- [x] Define runtime map types distinct from raw TMX XML types
- [x] Add `MapWorld`
- [x] Add `TileLayer`
- [x] Add `ObjectLayer`
- [x] Add `MapObject`
- [x] Add `CollisionPolygon`
- [x] Add generic property storage
- [x] Add a conversion step from `TmxMapAsset` to `MapWorld`
- [x] Store unknown layers and unknown objects in `MapWorld` even if no system consumes them yet

Recommended shape:

- `MapWorld` owns ordered tile layers
- `MapWorld` owns ordered object layers
- `MapWorld` stores map tile size and world origin
- `MapWorld` exposes lookup by layer name and object layer name

Exit criteria:

- [x] The game runtime owns a full map domain model
- [x] Runtime systems no longer depend on a flattened tile map shortcut
- [x] Tile layers remain generic ordered layers
- [x] Known and unknown TMX content are both preserved

## Phase 3: World Ownership And Startup Wiring

Goal: make TMX the default world source.

- [x] Add `MapWorld` ownership to `World`
- [x] Remove placeholder terrain seeding completely
- [x] Load the default TMX map in application startup
- [x] Resolve the default tileset image from TMX rather than from hardcoded sample atlas paths
- [x] Keep startup resilient: fail loudly for malformed required content, ignore unknown optional content
- [x] Keep unit seeding separate from map loading for now unless units are also moved into TMX later

Exit criteria:

- [x] The engine boots from TMX by default
- [x] The map is no longer authored in C++ loops
- [x] The placeholder map path is gone

## Phase 4: Multi-Layer Terrain Rendering

Goal: render tile layers as authored instead of flattening them.

- [x] Extend render-side data structures to support multiple tile layers
- [x] Preserve authoring order from TMX
- [x] Add a renderable flag per tile layer
- [x] Add a visibility flag per tile layer
- [x] Add per-layer opacity support
- [ ] Add optional future layer tint support
- [x] Update extraction to emit tiles per layer
- [x] Update batching to preserve layer ranges
- [x] Draw tile layers in authored order before units unless a future explicit property changes that

Recommended render model:

- one `RenderTileLayer` per map tile layer
- each layer owns an instance range
- renderer loops over layer ranges in order

Exit criteria:

- [x] All authored tile layers render as separate generic layers
- [x] Layer 0 draws first, then layer 1, then layer 2, and so on
- [x] Later tile layers draw on top of earlier tile layers
- [x] Layer order matches Tiled authoring order
- [x] The renderer no longer depends on a topmost-nonzero flattening rule

## Phase 5: Generic Layer Semantics

Goal: let TMX content exist without forcing hardcoded tile-layer names into the renderer or gameplay.

- [x] Treat tile layers generically by authored order, not by name
- [x] Default tile layers to renderable ordered content
- [x] Do not require tile layers to have semantic names
- [x] Do not derive tile rendering behavior from names like `Paths` or `Grass Dark`
- [x] Derive gameplay semantics from object groups, objects, and explicit properties instead
- [x] Never require every authored layer to have engine behavior

Recommended approach:

- tile layers:
  ordered render content unless an explicit future property overrides that
- object groups and objects:
  property-driven or name-driven gameplay semantics such as
  `collision`
  `water`
  `lava`
  `boundary`
  plus explicit properties like
  `engine.blocks_movement = true`
  `engine.damage_per_second = 10`

Exit criteria:

- [x] Tile layer names are not required for rendering behavior
- [x] Unknown tile layer names still load safely
- [x] Rendering remains stable even if tile layers are renamed in Tiled
- [x] Future map content can be added without loader rewrites

## Phase 6: Object Layer And Polygon Support

Goal: ingest object layers generically and support the current collision polygon case.

- [x] Parse object groups from TMX
- [x] Parse object ids, names, types/classes, visibility, position, size, rotation
- [x] Parse polygons as local point lists
- [x] Convert polygon local points to world-space polygon geometry
- [x] Preserve raw object metadata and properties even when no system consumes them
- [x] Add support for rectangle and point objects as future-ready baseline shapes
- [x] Allow object groups and objects to have specific authored names such as `collision`, `lava`, `water`, or `boundary`
- [x] Let these names and properties drive optional engine behavior
- [x] Do nothing for object groups or objects the engine does not implement yet

For the current map:

- [x] Load object group `collision`
- [x] Load the polygon object inside it
- [x] Convert it into world-space geometry

Exit criteria:

- [x] The current collision polygon loads successfully
- [x] Object layers are available to runtime systems
- [x] Unused object groups still load and do nothing

## Phase 7: Collision Runtime Integration

Goal: make authored collision geometry affect gameplay movement.

- [x] Add `src/game/collision_world.h/.cpp`
- [x] Build collision geometry from map object layers tagged as collidable
- [x] Add world-space polygon storage
- [x] Add broad-phase bounds per polygon
- [x] Integrate collision checks into unit movement
- [x] Start with simple “cannot move through polygon” behavior
- [ ] Do not require pathfinding refactor in the first pass

Recommended rollout:

1. Load and debug-render collision polygons
2. Clamp or reject movement through polygons
3. Later integrate full navigation/pathfinding

Exit criteria:

- [x] Units no longer move through authored collision polygons
- [x] Collision uses TMX-authored geometry, not hardcoded C++ logic

## Phase 8: Property System For Future Gameplay

Goal: make TMX properties the stable contract for future content.

- [ ] Add generic property parsing with support for:
  string
  int
  float
  bool
- [ ] Store properties on:
  map
  tileset
  layer
  object
- [x] Add typed helpers to query properties safely
- [ ] Make systems opt into the properties they understand
- [ ] Make unrecognized properties inert by default

Examples to support later:

- object group name `collision`
- object group name `water`
- object group name `lava`
- `engine.semantic = "water"`
- `engine.damage_per_second = 10`
- `engine.blocks_movement = true`
- `engine.spawn_faction = "neutral"`
- `engine.object_kind = "resource_node"`

Exit criteria:

- [ ] TMX properties are accessible to engine systems
- [ ] The engine can evolve through data, not name-based hacks

## Phase 9: Debugging And Validation Tooling

Goal: make authored TMX content inspectable while the engine grows.

- [x] Add overlay debug stats for:
  map size
  tile layer count
  object layer count
  collision polygon count
- [x] Add optional debug rendering for collision polygons
- [ ] Log unknown semantics at startup without failing
- [ ] Add validation warnings for malformed known content
- [ ] Keep permissive handling for unknown but well-formed content

Exit criteria:

- [ ] You can inspect what the TMX loader understood
- [ ] Unknown content is visible in diagnostics without breaking the run

## Phase 10: Scalable Loading Rules

Goal: define stable rules so future authored content does not force engine churn.

Rules the engine should follow:

- [ ] Load all TMX layers and object groups by default
- [ ] Preserve authoring order by default
- [ ] Store all known and unknown properties by default
- [ ] Render tile layers by authored order by default
- [ ] Do not hardcode tile layer names like `Paths` or `Grass Normal`
- [ ] Only apply gameplay behavior for semantics the engine implements
- [ ] Allow object groups and objects with specific authored names like `collision`, `water`, or `lava`
- [ ] Prefer explicit properties when present, but preserve authored names too
- [ ] Ignore unknown semantics without deleting the data
- [ ] Never flatten away information just because the current engine does not use it

Exit criteria:

- [ ] Future layers like `water`, `lava`, or `boundaries` can exist in TMX immediately
- [ ] The engine can do nothing with them at first without losing them
- [ ] Later systems can start consuming them without changing the asset format

## Recommended Implementation Order

1. Phase 1: full TMX asset model
2. Phase 2: runtime `MapWorld`
3. Phase 3: default startup wiring
4. Phase 4: multi-layer rendering
5. Phase 6: object layer and polygon support
6. Phase 7: collision runtime integration
7. Phase 8: property system
8. Phase 9: validation and debug tools

This order keeps rendering stable first, then adds gameplay interpretation.

## Out Of Scope For The First Pass

- [ ] TMX chunked infinite maps
- [ ] Animated tiles
- [ ] Tile flipping/rotation flags
- [ ] Multiple tilesets per map
- [ ] Isometric or hex maps
- [ ] Navmesh generation from polygons
- [ ] Full authored unit placement from TMX
- [ ] Script/event execution from map objects

These should come after the engine has:

- a full TMX asset model
- multi-layer render support
- object layer ingestion
- property-driven semantics
