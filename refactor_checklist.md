# Refactor Checklist

This checklist turns the architecture in `../diagram.md` into an implementation sequence for `gamer_time`.

## Phase 1: Runtime Skeleton

Goal: replace the current monolithic app shell with a coordinator that owns subsystem lifetime only.

- [x] Create `src/app/application.h`
- [x] Create `src/app/application.cpp`
- [x] Create `src/app/runtime_config.h`
- [x] Update `src/main.cpp` to build a `RuntimeConfig` and construct `Application`
- [x] Move current startup and shutdown flow out of `App` into `Application`
- [x] Define `Application::initialize()`
- [x] Define `Application::shutdown()`
- [x] Define `Application::tick_frame()`
- [x] Keep Vulkan calls out of `Application`
- [x] Keep SDL event parsing out of `Application`
- [ ] Preserve current behavior: open window, render frame, clean shutdown

Exit criteria:

- [x] Program still builds
- [ ] Program still opens a window
- [ ] Program still renders the current output
- [x] `Application` coordinates subsystems instead of doing their work directly

## Phase 2: Platform And Input Boundary

Goal: isolate SDL, normalized input state, and camera handling.

- [x] Create `src/platform/sdl_platform.h`
- [x] Create `src/platform/sdl_platform.cpp`
- [x] Create `src/platform/input_state.h`
- [x] Create `src/platform/camera_controller.h`
- [x] Create `src/platform/camera_controller.cpp`
- [x] Move SDL init and quit into `SdlPlatform`
- [x] Move window creation and destruction into `SdlPlatform`
- [x] Move SDL event polling into `SdlPlatform::poll_input()`
- [x] Define `InputState` with quit, resize, keyboard, mouse, and wheel fields
- [x] Convert raw `SDL_Event` values into `InputState`
- [x] Add `CameraState`
- [x] Add `CameraController::update(const InputState&, float dt)`
- [x] Wire resize events so the renderer still gets resize requests
- [x] Replace direct event handling in old `App` flow with `InputState`

Exit criteria:

- [x] `Application` sees `InputState`, not raw SDL events
- [ ] Quit still works
- [ ] Resize still works
- [ ] Simple zoom works
- [ ] Simple pan works

## Phase 3: Game-State Framework

Goal: create the gameplay-side data model and update order implied by the diagram.

- [x] Create `src/game/world.h`
- [x] Create `src/game/world.cpp`
- [x] Create `src/game/components.h`
- [x] Create `src/game/command_queue.h`
- [x] Create `src/game/command_queue.cpp`
- [x] Create `src/game/selection_system.h`
- [x] Create `src/game/selection_system.cpp`
- [x] Create `src/game/navigation_system.h`
- [x] Create `src/game/navigation_system.cpp`
- [x] Create `src/game/fog_of_war_system.h`
- [x] Create `src/game/fog_of_war_system.cpp`
- [x] Define `UnitId`
- [x] Define `TransformComponent`
- [x] Define `RenderComponent`
- [x] Define `VisionComponent`
- [x] Define `UnitComponent`
- [x] Define `MoveCommand`
- [x] Implement a `World` that owns units and component storage
- [x] Add a selected-unit collection to `World`
- [x] Add a command queue to `World`
- [x] Add CPU fog-of-war storage to `World`
- [x] Seed the world with a small set of test units
- [x] Implement `SelectionSystem`
- [x] Implement `CommandQueue` processing
- [x] Implement `NavigationSystem` with a simple placeholder movement/pathing step
- [x] Implement `FogOfWarSystem` to update visibility from unit positions
- [x] Establish main-thread update order:
- [x] `SelectionSystem`
- [x] `CommandQueue`
- [x] `NavigationSystem`
- [x] `FogOfWarSystem`

Exit criteria:

- [x] A real `World` exists
- [x] Units can be selected
- [x] Move commands can be issued
- [x] Units can move in a basic way
- [x] Fog-of-war data exists in CPU memory

## Phase 4: Renderer Decomposition

Goal: split the monolithic Vulkan renderer by ownership boundary without losing current output.

- [x] Create `src/gpu/vulkan_context.h`
- [x] Create `src/gpu/vulkan_context.cpp`
- [x] Create `src/gpu/swapchain_manager.h`
- [x] Create `src/gpu/swapchain_manager.cpp`
- [x] Create `src/gpu/gpu_resources.h`
- [x] Create `src/gpu/gpu_resources.cpp`
- [x] Create `src/render/scene_renderer.h`
- [x] Create `src/render/scene_renderer.cpp`
- [x] Create `src/render/text_overlay_renderer.h`
- [x] Create `src/render/text_overlay_renderer.cpp`
- [ ] Move Vulkan instance creation into `VulkanContext`
- [ ] Move surface creation into `VulkanContext`
- [ ] Move physical/logical device creation into `VulkanContext`
- [ ] Move queue discovery into `VulkanContext`
- [ ] Extend queue discovery to support optional compute queue family
- [ ] Move swapchain creation and recreation into `SwapchainManager`
- [ ] Move image views and framebuffers into `SwapchainManager`
- [ ] Move persistent GPU assets into `GpuResources`
- [ ] Move pipeline and command recording logic into `SceneRenderer`
- [ ] Move temporary text rendering path into `TextOverlayRenderer`
- [ ] Keep the old output visually working during the split

Exit criteria:

- [ ] No single class owns all Vulkan handles
- [ ] Old output still renders
- [ ] Resize and swapchain recreation still work
- [ ] API exposes graphics, present, and optional compute queues

## Phase 5: Render Pipeline Stages

Goal: make the main-thread render prep match the staged pipeline in the diagram.

- [x] Create `src/render/render_world.h`
- [x] Create `src/render/render_extractor.h`
- [x] Create `src/render/render_extractor.cpp`
- [x] Create `src/render/frustum_culler.h`
- [x] Create `src/render/frustum_culler.cpp`
- [x] Create `src/render/projection_system.h`
- [x] Create `src/render/projection_system.cpp`
- [x] Create `src/render/depth_sorter.h`
- [x] Create `src/render/depth_sorter.cpp`
- [x] Create `src/render/batch_builder.h`
- [x] Create `src/render/batch_builder.cpp`
- [x] Define `RenderUnit`
- [x] Define `RenderWorld`
- [x] Define `ProjectedUnit`
- [x] Define `InstanceData`
- [x] Implement `RenderExtractor::build(world, camera, ai_events)`
- [x] Implement `FrustumCuller`
- [x] Implement orthographic projection in `ProjectionSystem`
- [x] Implement bottom-anchor depth keys in `DepthSorter`
- [x] Implement instance generation in `BatchBuilder`
- [x] Route overlay text through `RenderWorld`
- [x] Stop letting the renderer read gameplay state directly

Exit criteria:

- [x] Renderer consumes `RenderWorld` or a built batch, not `World`
- [x] Culling is a distinct stage
- [x] Projection is a distinct stage
- [x] Y-sort is a distinct stage
- [x] Batch building is a distinct stage

## Phase 6: GPU Resource Model

Goal: mirror the diagram's VRAM layout with explicit persistent and dynamic resources.

- [x] Create `src/gpu/buffer_utils.h`
- [x] Create `src/gpu/buffer_utils.cpp`
- [x] Create `src/gpu/texture_utils.h`
- [x] Create `src/gpu/texture_utils.cpp`
- [x] Add a static quad vertex buffer to `GpuResources`
- [x] Add a static quad index buffer to `GpuResources`
- [x] Add texture atlas image/view/sampler to `GpuResources`
- [x] Add dynamic instance storage buffer to `GpuResources`
- [x] Add fog-of-war texture to `GpuResources`
- [x] Add staging/upload helpers to `GpuResources`
- [x] Implement instance buffer uploads from `BatchBuilder` output
- [x] Implement fog mask uploads from `World` CPU data
- [ ] Update descriptors and shaders as needed for instance and fog data
- [x] Keep allocator ownership contained to `gpu/` even if not using VMA yet

Exit criteria:

- [ ] Unit rendering uses instance data as the primary scene path
- [ ] Fog-of-war has a real GPU texture
- [x] Persistent GPU resources are owned in one place
- [x] Upload logic is separate from scene extraction

## Phase 7: AI Controller And Event Channel

Goal: replace string polling with a real controller and event stream.

- [x] Create `src/ai/llama_controller.h`
- [x] Create `src/ai/llama_controller.cpp`
- [x] Create `src/ai/token_stream.h`
- [x] Create `src/ai/token_stream.cpp`
- [x] Define `AiEventType`
- [x] Define `AiEvent`
- [x] Implement thread-safe event queueing from worker to main thread
- [x] Implement `LlamaController::start()`
- [x] Implement `LlamaController::shutdown()`
- [x] Implement `LlamaController::submit_prompt()`
- [x] Implement `LlamaController::drain_events()`
- [x] Port worker-thread logic from `LlamaWorker` into `LlamaController`
- [ ] Stream partial token output as `AiEventType::Token`
- [x] Stream status updates as `AiEventType::Status`
- [x] Stream completion and error events
- [x] Remove correctness dependence on `last_result()` and `last_status()`
- [x] Keep AI code isolated from SDL and Vulkan object access

Exit criteria:

- [x] Main thread drains AI events once per frame
- [ ] Partial token streaming works
- [x] AI status updates work
- [x] Renderer no longer depends on shared mutable string state from the worker

## Phase 8: Diagram Convergence

Goal: wire the full runtime so each box in `diagram.md` has a concrete code owner.

- [x] Finalize per-frame order in `Application`
- [x] Poll input through `SdlPlatform`
- [x] Update selection
- [x] Update command queue
- [x] Update navigation
- [x] Update fog of war
- [x] Drain AI events
- [x] Build `RenderWorld`
- [x] Run frustum culling
- [x] Run orthographic projection
- [x] Run y-sort
- [x] Build instance batches
- [x] Upload instance data
- [x] Upload fog mask
- [x] Record and submit graphics work
- [ ] Route AI token output into overlay text or unit barks
- [x] Remove the old `App` class
- [ ] Remove or reduce the old monolithic `VulkanRenderer`
- [x] Update `CMakeLists.txt` to stop relying on broad source globbing once layout stabilizes
- [x] Add subsystem targets:
- [x] `gt_core`
- [x] `gt_platform`
- [x] `gt_game`
- [x] `gt_render`
- [x] `gt_gpu`
- [x] `gt_ai`

Exit criteria:

- [ ] The runtime structure follows the diagram closely
- [x] Main thread owns SDL, simulation, visibility, render extraction, and frame submission
- [ ] Worker thread owns inference only
- [ ] Renderer consumes extracted frame data, not game state
- [ ] GPU module owns Vulkan handles and persistent VRAM resources

## Recommended Commit Boundaries

- [ ] Commit Phase 1 after the app shell compiles and behavior is unchanged
- [ ] Commit Phase 2 after SDL and camera are isolated
- [ ] Commit Phase 3 after a basic world, selection, movement, and CPU fog exist
- [ ] Commit Phase 4 after Vulkan ownership is split but output is preserved
- [ ] Commit Phase 5 after render extraction and batch building are explicit
- [ ] Commit Phase 6 after instance and fog uploads are live
- [ ] Commit Phase 7 after AI events stream through the controller
- [ ] Commit Phase 8 after cleanup and target layout stabilization

## Guardrails

- [ ] Do not build a generic ECS before the data model proves it is needed
- [ ] Do not let render code read mutable gameplay state directly
- [ ] Do not let AI code touch SDL or live Vulkan objects
- [ ] Do not let `Application` regain subsystem-specific logic
- [ ] Keep the program building at the end of every phase
