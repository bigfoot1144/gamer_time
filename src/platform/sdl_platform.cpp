#include "platform/sdl_platform.h"

#include <SDL3/SDL.h>

#include <stdexcept>

SdlPlatform::~SdlPlatform() {
    shutdown();
}

void SdlPlatform::initialize(const RuntimeConfig & config) {
    shutdown();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error("SDL_Init failed");
    }

    window_ = SDL_CreateWindow(
        "gamer_time",
        config.initial_width,
        config.initial_height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    if (!window_) {
        SDL_Quit();
        throw std::runtime_error("SDL_CreateWindow failed");
    }
}

void SdlPlatform::shutdown() {
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
}

InputState SdlPlatform::poll_input() {
    InputState input;

    if (!window_) {
        return input;
    }

    SDL_GetWindowSizeInPixels(window_, &input.window_width, &input.window_height);

    float mouse_x = 0.0f;
    float mouse_y = 0.0f;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    input.mouse_x = static_cast<int>(mouse_x);
    input.mouse_y = static_cast<int>(mouse_y);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            input.quit_requested = true;
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            input.resized = true;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                input.left_mouse_pressed = true;
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                input.right_mouse_pressed = true;
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            input.mouse_x = static_cast<int>(event.motion.x);
            input.mouse_y = static_cast<int>(event.motion.y);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            input.wheel_delta += event.wheel.y;
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.key == SDLK_ESCAPE) {
                input.escape_pressed = true;
            }
            if (event.key.key == SDLK_SPACE) {
                input.space_pressed = true;
            }
            if (event.key.key == SDLK_A || event.key.key == SDLK_LEFT) {
                input.move_left = true;
            }
            if (event.key.key == SDLK_D || event.key.key == SDLK_RIGHT) {
                input.move_right = true;
            }
            if (event.key.key == SDLK_W || event.key.key == SDLK_UP) {
                input.move_up = true;
            }
            if (event.key.key == SDLK_S || event.key.key == SDLK_DOWN) {
                input.move_down = true;
            }
            break;
        default:
            break;
        }
    }

    const bool * keyboard_state = SDL_GetKeyboardState(nullptr);
    if (keyboard_state) {
        input.move_left = input.move_left || keyboard_state[SDL_SCANCODE_A] || keyboard_state[SDL_SCANCODE_LEFT];
        input.move_right = input.move_right || keyboard_state[SDL_SCANCODE_D] || keyboard_state[SDL_SCANCODE_RIGHT];
        input.move_up = input.move_up || keyboard_state[SDL_SCANCODE_W] || keyboard_state[SDL_SCANCODE_UP];
        input.move_down = input.move_down || keyboard_state[SDL_SCANCODE_S] || keyboard_state[SDL_SCANCODE_DOWN];
    }

    return input;
}

void SdlPlatform::set_window_title(const char * title) const {
    if (window_) {
        SDL_SetWindowTitle(window_, title);
    }
}
