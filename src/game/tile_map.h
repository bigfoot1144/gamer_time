#pragma once

#include "core/types.h"

#include <cstddef>
#include <cstdint>
#include <vector>

struct TileCell {
    std::uint32_t atlas_index = 0;
};

class TileMap {
public:
    TileMap() = default;
    TileMap(std::uint32_t width, std::uint32_t height, Vec2f tile_size, Vec2f origin = {});

    bool empty() const;
    std::uint32_t width() const { return width_; }
    std::uint32_t height() const { return height_; }
    Vec2f tile_size() const { return tile_size_; }
    Vec2f origin() const { return origin_; }

    bool contains(std::uint32_t x, std::uint32_t y) const;
    std::size_t tile_count() const;
    const TileCell * try_cell(std::uint32_t x, std::uint32_t y) const;
    TileCell * try_cell(std::uint32_t x, std::uint32_t y);
    std::uint32_t atlas_index_at(std::uint32_t x, std::uint32_t y, std::uint32_t fallback = 0) const;
    bool set_atlas_index(std::uint32_t x, std::uint32_t y, std::uint32_t atlas_index);
    Vec2f cell_center_world(std::uint32_t x, std::uint32_t y) const;

private:
    std::size_t to_offset(std::uint32_t x, std::uint32_t y) const;

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    Vec2f tile_size_{24.0f, 24.0f};
    Vec2f origin_{};
    std::vector<TileCell> cells_;
};
