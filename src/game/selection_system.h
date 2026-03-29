#pragma once

#include "core/types.h"
#include "platform/camera_controller.h"
#include "platform/input_state.h"

class World;

class SelectionSystem {
public:
    void update(World & world, const InputState & input, const CameraState & camera) const;

    static Vec2f screen_to_world(const InputState & input, const CameraState & camera);
};
