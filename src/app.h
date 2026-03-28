#pragma once

#include "llama_worker.h"
#include "vulkan_renderer.h"

#include <string>

struct SDL_Window;
union SDL_Event;

class App {
public:
    App(std::string model_path, std::string shader_dir);
    ~App();

    int run();

private:
    void init();
    void handle_event(const SDL_Event & event, bool & running);
    void update_window_title();
    std::string build_overlay_text() const;
    void cleanup();

    SDL_Window * window_ = nullptr;
    VulkanRenderer renderer_;
    LlamaWorker llama_worker_;
    std::string model_path_;
    std::string shader_dir_;
    bool submitted_demo_prompt_ = false;
};
