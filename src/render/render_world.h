#pragma once

#include "core/types.h"
#include "game/components.h"
#include "platform/camera_controller.h"

#include <cstdint>
#include <string>
#include <vector>

struct RenderUnit {
    UnitId id = 0;
    Vec2f world_pos{};
    Vec2f size{24.0f, 24.0f};
    std::uint32_t sprite_index = 0;
    bool visible_in_fog = true;
    bool selected = false;
};

struct RenderTile {
    Vec2f world_pos{};
    Vec2f size{24.0f, 24.0f};
    std::uint32_t atlas_index = 0;
};

struct ProjectedUnit {
    RenderUnit source{};
    Vec2f screen_pos{};
    float depth_key = 0.0f;
};

struct InstanceData {
    Vec2f world_pos{};
    Vec2f size{24.0f, 24.0f};
    std::uint32_t sprite_index = 0;
    std::uint32_t flags = 0;
};

struct RenderBatch {
    std::vector<InstanceData> instances;
    std::uint32_t terrain_instance_offset = 0;
    std::uint32_t terrain_instance_count = 0;
    std::uint32_t unit_instance_offset = 0;
    std::uint32_t unit_instance_count = 0;
};

struct RenderWorld {
    CameraState camera{};
    std::vector<RenderTile> terrain_tiles;
    std::vector<RenderUnit> units;
    std::vector<ProjectedUnit> projected_units;
    std::string overlay_text;
};
