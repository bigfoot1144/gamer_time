#include "app/application.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace {

AtlasAsset build_default_scene_atlas() {
    return AtlasAsset::from_grid_image(
        "assets/tiles/sample_scene_atlas.png",
        24,
        24,
        {
            {"grass_a", 0},
            {"grass_b", 1},
            {"grass_c", 2},
            {"wall", 3},
            {"rock", 4},
            {"water", 5},
            {"unit_orange", 12},
            {"unit_teal", 13},
            {"unit_green", 14},
            {"unit_red", 15},
        }
    );
}

} // namespace

Application::Application(RuntimeConfig config)
    : config_(std::move(config)) {
}

Application::~Application() {
    shutdown();
}

int Application::run() {
    initialize();
    running_ = true;
    last_tick_ = std::chrono::steady_clock::now();

    while (running_) {
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<float> delta = now - last_tick_;
        last_tick_ = now;
        tick_frame(std::clamp(delta.count(), 0.0f, 0.05f));
    }

    shutdown();
    return 0;
}

void Application::initialize() {
    if (initialized_) {
        return;
    }

    platform_.initialize(config_);
    scene_renderer_.initialize(platform_.window(), config_.shader_dir);
    scene_atlas_ = build_default_scene_atlas();
    scene_atlas_image_ = assets::load_png_rgba(scene_atlas_.image_path);
    scene_atlas_.columns = scene_atlas_image_.width / scene_atlas_.tile_width;
    scene_atlas_.rows = scene_atlas_image_.height / scene_atlas_.tile_height;
    scene_renderer_.initialize_scene_atlas(scene_atlas_, scene_atlas_image_);
    world_.seed_test_terrain();
    world_.seed_test_units();

    if (!config_.model_path.empty()) {
        llama_controller_.start(config_.model_path);
    }

    initialized_ = true;
}

void Application::shutdown() {
    if (!initialized_) {
        return;
    }

    scene_renderer_.shutdown();
    llama_controller_.shutdown();
    platform_.shutdown();

    initialized_ = false;
    running_ = false;
}

void Application::tick_frame(float dt_seconds) {
    const InputState input = platform_.poll_input();
    if (input.quit_requested || input.escape_pressed) {
        running_ = false;
        return;
    }

    if (input.resized) {
        scene_renderer_.request_resize();
    }

    camera_controller_.update(input, dt_seconds);
    selection_system_.update(world_, input, camera_controller_.state());

    if (input.right_mouse_pressed && !world_.selected_units().empty()) {
        MoveCommand command{};
        command.units = world_.selected_units();
        command.destination = SelectionSystem::screen_to_world(input, camera_controller_.state());
        world_.command_queue().push(std::move(command));
    }

    world_.command_queue().apply(world_);
    navigation_system_.update(world_, dt_seconds);
    fog_of_war_system_.update(world_);
    maybe_submit_prompt(input);
    recent_ai_events_ = collect_ai_events();

    RenderWorld render_world = render_extractor_.build(
        world_,
        camera_controller_.state(),
        recent_ai_events_,
        build_overlay_text()
    );
    frustum_culler_.run(render_world, input.window_width, input.window_height);
    projection_system_.run(render_world, input.window_width, input.window_height);
    depth_sorter_.run(render_world);
    RenderBatch batch = batch_builder_.build(render_world);

    scene_renderer_.upload_frame_resources(
        batch,
        world_.fog_mask(),
        world_.fog_width(),
        world_.fog_height(),
        camera_controller_.state()
    );
    scene_renderer_.set_overlay_text(render_world.overlay_text);
    scene_renderer_.draw_frame();
    update_window_title(batch);
}

void Application::update_window_title(const RenderBatch & batch) const {
    std::string status = last_ai_status_.empty() ? std::string("idle") : last_ai_status_;
    if (llama_controller_.is_loading()) {
        status = "loading";
    } else if (llama_controller_.is_ready() && status == "idle") {
        status = "ready";
    }

    std::string title = "gamer_time | units: ";
    title += std::to_string(world_.unit_count());
    title += " | tiles: ";
    title += std::to_string(world_.terrain().tile_count());
    title += " | visible: ";
    title += std::to_string(batch.unit_instance_count);
    title += " | selected: ";
    title += std::to_string(world_.selected_units().size());
    title += " | llama: ";
    title += status;

    if (!last_ai_result_.empty()) {
        title += " | ";
        title += last_ai_result_.substr(0, 80);
    }

    platform_.set_window_title(title.c_str());
}

std::string Application::build_overlay_text() const {
    std::ostringstream overlay;
    const CameraState & camera = camera_controller_.state();

    overlay << "GAMER_TIME RTS FRAMEWORK\n";
    overlay << "ESC quit | SPACE rerun prompt | click select | right click move | WASD/Arrows pan | wheel zoom\n\n";
    overlay << "Units: " << world_.unit_count() << '\n';
    overlay << "Terrain tiles: " << world_.terrain().tile_count() << '\n';
    overlay << "Selected: " << world_.selected_units().size() << '\n';
    overlay << "Camera: (" << static_cast<int>(camera.world_center.x) << ", " << static_cast<int>(camera.world_center.y) << ") zoom " << camera.zoom << "\n";
    overlay << "Fog cells visible: " << std::count(world_.fog_mask().begin(), world_.fog_mask().end(), static_cast<std::uint8_t>(255)) << "\n";
    overlay << "Uploaded instances: " << scene_renderer_.resources().staged_instances().size() << "\n";
    overlay << "Scene atlas grid: " << scene_atlas_.columns << "x" << scene_atlas_.rows << "\n";
    overlay << "Fog texture size: " << scene_renderer_.resources().fog_texture().width << "x" << scene_renderer_.resources().fog_texture().height << "\n\n";

    if (config_.model_path.empty()) {
        overlay << "No model path provided. Pass a GGUF path on the command line to enable worker-thread output.\n";
        return overlay.str();
    }

    overlay << "Worker: ";
    overlay << (last_ai_status_.empty() ? std::string("idle") : last_ai_status_) << "\n\n";

    if (!last_ai_result_.empty()) {
        overlay << last_ai_result_;
    } else if (llama_controller_.is_loading()) {
        overlay << "Loading model...\n";
    } else if (submitted_demo_prompt_) {
        overlay << "Waiting for sample prompt output...\n";
    } else {
        overlay << "Waiting for llama.cpp to become ready...\n";
    }

    return overlay.str();
}

void Application::maybe_submit_prompt(const InputState & input) {
    if (!submitted_demo_prompt_ && llama_controller_.is_ready() && !config_.model_path.empty()) {
        last_ai_result_.clear();
        llama_controller_.submit_prompt(
            "Write one sentence about keeping an RTS renderer smooth while gameplay and inference run concurrently.",
            48
        );
        submitted_demo_prompt_ = true;
        return;
    }

    if (input.space_pressed && llama_controller_.is_ready()) {
        last_ai_result_.clear();
        llama_controller_.submit_prompt(
            "Give me a short status line for an RTS framework with Vulkan rendering and a worker-thread llama model.",
            32
        );
    }
}

std::vector<std::string> Application::collect_ai_events() {
    std::vector<std::string> events_for_render;

    for (const AiEvent & event : llama_controller_.drain_events()) {
        switch (event.type) {
        case AiEventType::Status:
            last_ai_status_ = event.text;
            if (event.text == "Running inference...") {
                last_ai_result_.clear();
            }
            if (!event.text.empty()) {
                events_for_render.push_back("status: " + event.text);
            }
            break;
        case AiEventType::Completed:
            last_ai_result_ = event.text;
            if (!event.text.empty()) {
                events_for_render.push_back(event.text.substr(0, 80));
            }
            break;
        case AiEventType::Error:
            last_ai_status_ = "error";
            last_ai_result_ = event.text;
            events_for_render.push_back("error: " + event.text);
            break;
        case AiEventType::Token:
            last_ai_result_ += event.text;
            events_for_render.push_back(event.text);
            break;
        }
    }

    if (events_for_render.size() > 4) {
        events_for_render.erase(events_for_render.begin(), events_for_render.end() - 4);
    }

    return events_for_render;
}
