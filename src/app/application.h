#pragma once

#include "ai/llama_controller.h"
#include "app/runtime_config.h"
#include "assets/atlas_asset.h"
#include "assets/image_loader.h"
#include "assets/tmx_map_loader.h"
#include "game/fog_of_war_system.h"
#include "game/navigation_system.h"
#include "game/selection_system.h"
#include "game/world.h"
#include "platform/camera_controller.h"
#include "platform/sdl_platform.h"
#include "render/batch_builder.h"
#include "render/depth_sorter.h"
#include "render/frustum_culler.h"
#include "render/projection_system.h"
#include "render/render_extractor.h"
#include "render/render_world.h"
#include "render/scene_renderer.h"

#include <chrono>
#include <string>
#include <vector>

class Application {
public:
    explicit Application(RuntimeConfig config);
    ~Application();

    int run();

private:
    void initialize();
    void shutdown();
    void tick_frame(float dt_seconds);
    void update_window_title(const RenderBatch & batch) const;
    std::string build_overlay_text() const;
    void maybe_submit_prompt(const InputState & input);
    std::vector<std::string> collect_ai_events();

    RuntimeConfig config_;
    SdlPlatform platform_;
    CameraController camera_controller_;
    World world_;
    SelectionSystem selection_system_;
    NavigationSystem navigation_system_;
    FogOfWarSystem fog_of_war_system_;
    RenderExtractor render_extractor_;
    FrustumCuller frustum_culler_;
    ProjectionSystem projection_system_;
    DepthSorter depth_sorter_;
    BatchBuilder batch_builder_;
    SceneRenderer scene_renderer_;
    LlamaController llama_controller_;
    bool running_ = false;
    bool initialized_ = false;
    bool submitted_demo_prompt_ = false;
    bool show_collision_debug_ = true;
    AtlasAsset scene_atlas_;
    LoadedImage scene_atlas_image_;
    std::string last_ai_status_;
    std::string last_ai_result_;
    std::vector<std::string> recent_ai_events_;
    std::chrono::steady_clock::time_point last_tick_{};
};
