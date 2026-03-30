# Tile And Entity Asset Workflow

This project now renders a single scene atlas for both terrain and entities.

## 1. Atlas Image

Put your atlas PNG at a path like:

`assets/tiles/my_scene_atlas.png`

Current assumptions:

- The atlas is a regular grid.
- Every tile/sprite cell is the same size.
- Terrain tiles and entity sprites can live in the same atlas.
- Transparent pixels in entity sprites are respected by the shader.

The sample atlas uses:

- 24x24 cells
- 4 columns
- 4 rows

## 2. Atlas Metadata

Set the atlas path and tile size in [application.cpp](/workspace/gamer_time/src/app/application.cpp).

Update `build_default_scene_atlas()`:

```cpp
return AtlasAsset::from_grid_image(
    "assets/tiles/my_scene_atlas.png",
    24,
    24,
    {
        {"grass", 0},
        {"dirt", 1},
        {"water", 2},
        {"unit_knight", 12},
    }
);
```

The named map is optional, but it gives you a stable place to document atlas indices.

## 3. Tile Map Data

Terrain lives in [world.h](/workspace/gamer_time/src/game/world.h) and [world.cpp](/workspace/gamer_time/src/game/world.cpp) through `World::terrain()`.

To author a map in code:

```cpp
terrain_ = TileMap(64, 64, {24.0f, 24.0f}, {-768.0f, -768.0f});
terrain_.set_atlas_index(10, 12, 0);
terrain_.set_atlas_index(11, 12, 1);
terrain_.set_atlas_index(12, 12, 2);
```

Key points:

- `TileMap(width, height, tile_size, origin)` defines the grid.
- `tile_size` is the world-space quad size.
- `origin` is the top-left world position of the map.
- `set_atlas_index(x, y, index)` chooses which atlas cell to draw.

If you want to load an external tile map file later, the integration point is `World::seed_test_terrain()` or a replacement loader that fills `terrain_`.

## 4. Entity Sprites

Units already render from atlas indices through `RenderComponent.sprite_index` in [components.h](/workspace/gamer_time/src/game/components.h).

Example:

```cpp
create_unit({{60.0f, 70.0f}}, {14, {24.0f, 24.0f}}, {112.0f}, {});
```

That means:

- `14` is the atlas cell for the unit art.
- `{24.0f, 24.0f}` is the rendered world size.

To use larger entity art, change the footprint:

```cpp
create_unit({{60.0f, 70.0f}}, {20, {48.0f, 48.0f}}, {112.0f}, {});
```

## 5. Rendering Path

The terrain render path is automatic once `World::terrain()` is populated:

- [render_extractor.cpp](/workspace/gamer_time/src/render/render_extractor.cpp) converts map cells into `RenderTile`.
- [batch_builder.cpp](/workspace/gamer_time/src/render/batch_builder.cpp) emits terrain instances first, then unit instances.
- [scene_renderer.cpp](/workspace/gamer_time/src/render/scene_renderer.cpp) draws terrain before units.
- [triangle.vert](/workspace/gamer_time/shaders/triangle.vert) and [triangle.frag](/workspace/gamer_time/shaders/triangle.frag) compute atlas UVs and sample the PNG.

You do not need to add separate rendering code for a new tile map if you keep using `TileMap` and atlas indices.

## 6. Camera Movement

Camera pan and zoom already apply to both terrain and entities because both are rendered in world space.

Current controls:

- `WASD` or arrow keys: pan
- Mouse wheel: zoom
- Left click: select unit
- Right click: move selected unit

If you add a larger map, camera movement keeps working without renderer changes.

## 7. If You Want File-Driven Maps Next

The next concrete extension is:

1. Load a map file into `TileMap`.
2. Load atlas metadata from a data file instead of hardcoding `build_default_scene_atlas()`.
3. Keep entity sprite indices in gameplay data or unit definitions.

The current code already supports the rendering side of that. The missing piece would just be parsing your external map format and filling `TileMap`.
