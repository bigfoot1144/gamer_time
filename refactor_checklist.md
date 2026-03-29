# Next Agent Handoff

Current state of the codebase:

- The old `VulkanRenderer` has been fully deleted.
- `SceneRenderer` now owns the full frame lifecycle in `src/render/scene_renderer.h` and `src/render/scene_renderer.cpp`.
- `gpu::VulkanContext` owns instance/surface/device/queues.
- `gpu::SwapchainManager` owns swapchain/images/views/framebuffers.
- `gpu::GpuResources` owns persistent GPU resources including:
  static quad buffers
  instance buffer
  fog texture
- `TextOverlayRenderer` owns the text overlay Vulkan path and is compiled as part of `gt_gpu`.
- The scene path already renders instanced quads using:
  `RenderBatch`
  `GpuResources`
  scene descriptors
  fog texture
- The current scene shader is still temporary:
  it uses generated colors from `sprite_index`
  it does not sample a real atlas yet
- True AI token streaming is already implemented:
  `LlamaWorker` emits `status:`, `token:`, `completed:`, and `error:` events directly
  `LlamaController` is event-driven and no longer depends on polling `last_status()` / `last_result()`
- The user manually verified runtime behavior after the refactor:
  window opens
  rendering works
  quit works
  resize works
  pan works
  zoom works

Important architectural intent:

- The user wants a tile-based system next.
- The user explicitly wants atlas work deferred until after the refactor/cleanup, and that cleanup is now done.
- The next major milestone is adding a tile-based terrain layer backed by an atlas.
- Do not reintroduce `VulkanRenderer`.
- Keep the current ownership split:
  `gpu/` owns Vulkan context, swapchain, and persistent GPU resources
  `render/` owns frame orchestration and scene/text rendering policy

Recommended starting point for the next agent:

1. Start with Phase 1 and Phase 2 in this checklist.
2. Add:
   `src/assets/atlas_asset.h/.cpp`
   `src/assets/image_loader.h/.cpp`
   `src/game/tile_map.h/.cpp`
3. Add tile map ownership to `World`.
4. Seed a simple test terrain map before touching atlas sampling in shaders.
5. After that, extend `RenderWorld`, `RenderExtractor`, and `BatchBuilder` to emit terrain batches.

Files the next agent should inspect first:

- `src/render/scene_renderer.h`
- `src/render/scene_renderer.cpp`
- `src/gpu/gpu_resources.h`
- `src/gpu/gpu_resources.cpp`
- `src/render/render_world.h`
- `src/render/render_extractor.cpp`
- `src/render/batch_builder.cpp`
- `src/game/world.h`
- `src/game/world.cpp`
- `shaders/triangle.vert`
- `shaders/triangle.frag`

# Tile Atlas Implementation Checklist

This checklist replaces the completed refactor roadmap. It tracks the next concrete milestone: add a tile-based terrain system backed by a texture atlas, while keeping the current renderer architecture intact.

## Phase 1: Asset Metadata And Image Loading

Goal: define atlas metadata and load atlas image pixels on CPU.

- [ ] Create `src/assets/atlas_asset.h`
- [ ] Create `src/assets/atlas_asset.cpp`
- [ ] Create `src/assets/image_loader.h`
- [ ] Create `src/assets/image_loader.cpp`
- [ ] Define an `AtlasAsset` type with image path, tile size, columns, and rows
- [ ] Add optional tile id/name mapping support
- [ ] Define a `LoadedImage` type with width, height, and RGBA pixel storage
- [ ] Load a PNG atlas image into CPU memory
- [ ] Keep asset loading isolated from SDL and Vulkan objects

Exit criteria:

- [ ] Atlas metadata can be loaded or constructed cleanly
- [ ] Atlas pixels can be loaded into CPU memory
- [ ] Tile dimensions and atlas grid dimensions are available at runtime

## Phase 2: Tile Map Data Model

Goal: add gameplay-side ownership for a tile terrain layer.

- [ ] Create `src/game/tile_map.h`
- [ ] Create `src/game/tile_map.cpp`
- [ ] Define `TileCell`
- [ ] Define `TileMap`
- [ ] Add width/height storage
- [ ] Add tile size storage
- [ ] Add per-cell atlas index storage
- [ ] Add safe tile lookup helpers
- [ ] Add `TileMap` ownership to `World`
- [ ] Seed the world with a small test terrain map

Exit criteria:

- [ ] `World` owns a terrain map
- [ ] Terrain tiles can be authored as atlas indices
- [ ] A test map exists at runtime

## Phase 3: Render Extraction For Terrain

Goal: extract tile terrain into render-side data alongside units.

- [ ] Extend `src/render/render_world.h` with terrain render data
- [ ] Define `RenderTile`
- [ ] Update `src/render/render_extractor.cpp` to emit terrain tiles
- [ ] Compute tile world positions from map coordinates and tile size
- [ ] Preserve existing unit extraction
- [ ] Keep terrain extraction separate from gameplay mutation

Exit criteria:

- [ ] `RenderWorld` contains terrain tiles
- [ ] Terrain tile positions are expressed in world space
- [ ] Unit extraction still works

## Phase 4: Terrain Batch Building

Goal: turn extracted terrain and units into explicit instance batches.

- [ ] Extend `RenderBatch` for terrain data
- [ ] Decide whether terrain and units share `InstanceData` or use separate structs
- [ ] Update `src/render/batch_builder.cpp` to emit terrain instances
- [ ] Keep terrain instances in a separate batch or range from units
- [ ] Preserve unit batching and unit selection flags

Exit criteria:

- [ ] Terrain instances are built explicitly
- [ ] Unit instances are still built explicitly
- [ ] Render batches contain enough information for separate terrain and unit draws

## Phase 5: GPU Atlas Resource

Goal: upload a scene atlas into VRAM and expose it through `GpuResources`.

- [ ] Extend `src/gpu/gpu_resources.h` with a scene atlas texture
- [ ] Extend `src/gpu/gpu_resources.cpp` with scene atlas upload logic
- [ ] Add a `scene_atlas_texture()` accessor
- [ ] Add a `upload_scene_atlas(...)` or `initialize_scene_atlas(...)` path
- [ ] Create atlas image, image view, and sampler in GPU memory
- [ ] Keep atlas upload separate from fog upload

Exit criteria:

- [ ] A real scene atlas texture exists in `GpuResources`
- [ ] Atlas image/view/sampler are available to the renderer
- [ ] Fog texture path still works

## Phase 6: Scene Shader Atlas Sampling

Goal: replace temporary generated unit colors with real atlas sampling.

- [ ] Update `shaders/triangle.vert`
- [ ] Update `shaders/triangle.frag`
- [ ] Bind atlas texture in the scene descriptor set
- [ ] Keep fog texture in the scene descriptor set
- [ ] Pass atlas layout information to shaders
- [ ] Compute atlas UVs from tile or sprite index
- [ ] Sample the atlas texture in the fragment shader
- [ ] Preserve fog modulation

Exit criteria:

- [ ] Scene color comes from the atlas, not generated fallback colors
- [ ] Fog modulation still works
- [ ] Tile and/or unit indices map to atlas UVs correctly

## Phase 7: Terrain Rendering Path

Goal: draw the terrain layer from batches before units.

- [ ] Update `src/render/scene_renderer.cpp` to bind atlas and fog descriptors
- [ ] Add terrain draw call(s)
- [ ] Keep unit draw call(s) after terrain draw call(s)
- [ ] Preserve text overlay draw after scene rendering
- [ ] Ensure tile quads use correct world size and placement

Exit criteria:

- [ ] Terrain renders from the atlas
- [ ] Units still render on top of terrain
- [ ] Overlay text still renders correctly

## Phase 8: App Wiring And Validation

Goal: load assets at startup, wire terrain rendering end-to-end, and verify behavior.

- [ ] Load atlas metadata during application startup
- [ ] Load atlas image during application startup
- [ ] Upload atlas to `GpuResources`
- [ ] Seed test tile indices that match atlas contents
- [ ] Confirm the program still opens a window
- [ ] Confirm terrain is visible
- [ ] Confirm units still render
- [ ] Confirm fog still affects visible output
- [ ] Confirm pan still works over the tile map
- [ ] Confirm zoom still works over the tile map
- [ ] Confirm resize still works
- [ ] Confirm quit still works

Exit criteria:

- [ ] Atlas-backed terrain renders end-to-end
- [ ] Existing runtime behavior still works
- [ ] Renderer remains data-driven through extracted batches

## Deferred For Later

These are intentionally out of scope for the first tile-atlas pass.

- [ ] Atlas-driven unit art
- [ ] Animated tiles
- [ ] Autotiling or terrain transitions
- [ ] Multiple terrain layers
- [ ] Chunk streaming
- [ ] Asset editor tooling
- [ ] Map file serialization format
