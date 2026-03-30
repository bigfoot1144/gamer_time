#pragma once

#include "game/command_queue.h"
#include "game/components.h"
#include "game/tile_map.h"

#include <cstddef>
#include <cstdint>
#include <vector>

class World {
public:
    World();

    void seed_test_units();
    void set_terrain(TileMap terrain);

    UnitId create_unit(
        TransformComponent transform,
        RenderComponent render,
        VisionComponent vision,
        UnitComponent unit
    );

    std::size_t unit_count() const {
        return transforms_.size();
    }

    const std::vector<UnitId> & unit_ids() const {
        return unit_ids_;
    }

    const std::vector<UnitId> & selected_units() const {
        return selected_units_;
    }

    void select_single(UnitId unit_id);
    void clear_selection();

    TransformComponent * try_transform(UnitId unit_id);
    const TransformComponent * try_transform(UnitId unit_id) const;
    RenderComponent * try_render(UnitId unit_id);
    const RenderComponent * try_render(UnitId unit_id) const;
    VisionComponent * try_vision(UnitId unit_id);
    const VisionComponent * try_vision(UnitId unit_id) const;
    UnitComponent * try_unit(UnitId unit_id);
    const UnitComponent * try_unit(UnitId unit_id) const;

    CommandQueue & command_queue() {
        return command_queue_;
    }

    const std::vector<std::uint8_t> & fog_mask() const {
        return fog_mask_;
    }

    std::vector<std::uint8_t> & fog_mask() {
        return fog_mask_;
    }

    std::uint32_t fog_width() const {
        return fog_width_;
    }

    std::uint32_t fog_height() const {
        return fog_height_;
    }

    const TileMap & terrain() const {
        return terrain_;
    }

    TileMap & terrain() {
        return terrain_;
    }

private:
    std::size_t to_index(UnitId unit_id) const;

    std::vector<UnitId> unit_ids_;
    std::vector<TransformComponent> transforms_;
    std::vector<RenderComponent> renders_;
    std::vector<VisionComponent> visions_;
    std::vector<UnitComponent> units_;
    std::vector<UnitId> selected_units_;
    CommandQueue command_queue_;
    std::vector<std::uint8_t> fog_mask_;
    std::uint32_t fog_width_ = 64;
    std::uint32_t fog_height_ = 64;
    TileMap terrain_;
};
