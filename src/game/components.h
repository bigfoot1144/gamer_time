#pragma once

#include "core/types.h"

#include <cstdint>
#include <vector>

using UnitId = std::uint32_t;

struct TransformComponent {
    Vec2f position{};
};

struct RenderComponent {
    std::uint32_t sprite_index = 0;
    Vec2f footprint{24.0f, 24.0f};
};

struct VisionComponent {
    float radius = 96.0f;
};

struct UnitComponent {
    bool selected = false;
    bool has_move_target = false;
    Vec2f move_target{};
};

struct MoveCommand {
    std::vector<UnitId> units;
    Vec2f destination{};
};
