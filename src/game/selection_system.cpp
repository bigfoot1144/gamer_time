#include "game/selection_system.h"

#include "game/world.h"

#include <limits>

void SelectionSystem::update(World & world, const InputState & input, const CameraState & camera) const {
    if (!input.left_mouse_pressed || input.window_width <= 0 || input.window_height <= 0) {
        return;
    }

    const Vec2f world_point = screen_to_world(input, camera);

    float best_distance = std::numeric_limits<float>::max();
    UnitId best_unit = static_cast<UnitId>(-1);

    for (UnitId unit_id : world.unit_ids()) {
        const TransformComponent * transform = world.try_transform(unit_id);
        if (!transform) {
            continue;
        }

        const float distance = length_squared(transform->position - world_point);
        if (distance < best_distance) {
            best_distance = distance;
            best_unit = unit_id;
        }
    }

    if (best_unit != static_cast<UnitId>(-1) && best_distance <= 48.0f * 48.0f) {
        world.select_single(best_unit);
    } else {
        world.clear_selection();
    }
}

Vec2f SelectionSystem::screen_to_world(const InputState & input, const CameraState & camera) {
    if (input.window_width <= 0 || input.window_height <= 0) {
        return camera.world_center;
    }

    const float normalized_x = (static_cast<float>(input.mouse_x) / static_cast<float>(input.window_width)) - 0.5f;
    const float normalized_y = (static_cast<float>(input.mouse_y) / static_cast<float>(input.window_height)) - 0.5f;

    const float view_width = static_cast<float>(input.window_width) / camera.zoom;
    const float view_height = static_cast<float>(input.window_height) / camera.zoom;

    return {
        camera.world_center.x + normalized_x * view_width,
        camera.world_center.y - normalized_y * view_height,
    };
}
