#pragma once

#include "core/types.h"
#include "platform/input_state.h"

struct CameraState {
    Vec2f world_center{0.0f, 0.0f};
    float zoom = 1.0f;
};

class CameraController {
public:
    void update(const InputState & input, float dt_seconds);

    const CameraState & state() const {
        return state_;
    }

private:
    CameraState state_{};
};
