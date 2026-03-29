#pragma once

#include "app/runtime_config.h"
#include "platform/input_state.h"

struct SDL_Window;

class SdlPlatform {
public:
    SdlPlatform() = default;
    ~SdlPlatform();

    void initialize(const RuntimeConfig & config);
    void shutdown();

    InputState poll_input();
    SDL_Window * window() const {
        return window_;
    }

    void set_window_title(const char * title) const;

private:
    SDL_Window * window_ = nullptr;
};
