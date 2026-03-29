#include "platform/camera_controller.h"

#include <algorithm>

void CameraController::update(const InputState & input, float dt_seconds) {
    constexpr float kPanSpeed = 320.0f;
    constexpr float kZoomStep = 0.12f;
    constexpr float kMinZoom = 0.35f;
    constexpr float kMaxZoom = 3.0f;

    Vec2f delta{};
    if (input.move_left) {
        delta.x -= 1.0f;
    }
    if (input.move_right) {
        delta.x += 1.0f;
    }
    if (input.move_up) {
        delta.y -= 1.0f;
    }
    if (input.move_down) {
        delta.y += 1.0f;
    }

    state_.world_center += delta * (kPanSpeed * dt_seconds / state_.zoom);
    state_.zoom = std::clamp(state_.zoom + input.wheel_delta * kZoomStep, kMinZoom, kMaxZoom);
}
