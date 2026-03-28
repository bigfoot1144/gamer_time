#include "app.h"

#include <SDL3/SDL.h>

#include <stdexcept>
#include <string>
#include <utility>

App::App(std::string model_path, std::string shader_dir)
    : model_path_(std::move(model_path)),
      shader_dir_(std::move(shader_dir)) {
}

App::~App() {
    cleanup();
}

int App::run() {
    init();

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            handle_event(event, running);
        }

        if (!submitted_demo_prompt_ &&
            llama_worker_.is_ready() &&
            !model_path_.empty()) {
            LlamaWorker::Job job{};
            job.prompt = "Write one sentence about keeping a renderer smooth while inference runs on a worker thread.";
            job.max_tokens = 48;
            llama_worker_.submit(job);
            submitted_demo_prompt_ = true;
        }

        renderer_.set_overlay_text(build_overlay_text());
        renderer_.draw_frame();
        update_window_title();
    }

    cleanup();
    return 0;
}

void App::init() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error("SDL_Init failed");
    }

    window_ = SDL_CreateWindow(
        "SDL3 + Vulkan triangle",
        1280,
        720,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    if (!window_) {
        throw std::runtime_error("SDL_CreateWindow failed");
    }

    renderer_.init(window_, shader_dir_);

    if (!model_path_.empty()) {
        llama_worker_.start(model_path_);
    }
}

void App::handle_event(const SDL_Event & event, bool & running) {
    if (event.type == SDL_EVENT_QUIT) {
        running = false;
        return;
    }

    if (event.type == SDL_EVENT_WINDOW_RESIZED) {
        renderer_.request_resize();
        return;
    }

    if (event.type == SDL_EVENT_KEY_DOWN) {
        if (event.key.key == SDLK_ESCAPE) {
            running = false;
            return;
        }

        if (event.key.key == SDLK_SPACE) {
            LlamaWorker::Job job{};
            job.prompt = "Give me a short status line for a Vulkan app with a worker-thread llama model.";
            job.max_tokens = 32;
            llama_worker_.submit(job);
            return;
        }
    }
}

void App::update_window_title() {
    if (!window_) {
        return;
    }

    std::string status = "idle";
    if (llama_worker_.is_loading()) {
        status = "loading";
    } else if (llama_worker_.is_ready()) {
        status = "ready";
    }

    std::string title = "SDL3 + Vulkan triangle | llama: " + status;

    const std::string result = llama_worker_.last_result();
    if (!result.empty()) {
        title += " | ";
        title += result.substr(0, 80);
    }

    SDL_SetWindowTitle(window_, title.c_str());
}

std::string App::build_overlay_text() const {
    std::string overlay = "LLAMA + VULKAN HUD\n";
    overlay += "ESC quit | SPACE rerun prompt\n\n";

    if (model_path_.empty()) {
        overlay += "No model path provided. Pass the GGUF path on the command line to enable NPC chat text.\n";
        return overlay;
    }

    overlay += "Worker: ";
    const std::string status = llama_worker_.last_status();
    overlay += status.empty() ? std::string("idle") : status;
    overlay += "\n\n";

    const std::string result = llama_worker_.last_result();
    if (!result.empty()) {
        overlay += result;
        return overlay;
    }

    if (llama_worker_.is_loading()) {
        overlay += "Loading model...\n";
    } else if (submitted_demo_prompt_) {
        overlay += "Waiting for the sample prompt output...\n";
    } else {
        overlay += "Waiting for llama.cpp to become ready...\n";
    }

    return overlay;
}

void App::cleanup() {
    renderer_.shutdown();
    llama_worker_.shutdown();

    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
}
