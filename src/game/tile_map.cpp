#include "game/tile_map.h"

TileMap::TileMap(std::uint32_t width, std::uint32_t height, Vec2f tile_size, Vec2f origin)
    : width_(width),
      height_(height),
      tile_size_(tile_size),
      origin_(origin),
      cells_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
}

bool TileMap::empty() const {
    return cells_.empty();
}

bool TileMap::contains(std::uint32_t x, std::uint32_t y) const {
    return x < width_ && y < height_;
}

std::size_t TileMap::tile_count() const {
    return cells_.size();
}

const TileCell * TileMap::try_cell(std::uint32_t x, std::uint32_t y) const {
    if (!contains(x, y)) {
        return nullptr;
    }
    return &cells_[to_offset(x, y)];
}

TileCell * TileMap::try_cell(std::uint32_t x, std::uint32_t y) {
    if (!contains(x, y)) {
        return nullptr;
    }
    return &cells_[to_offset(x, y)];
}

std::uint32_t TileMap::atlas_index_at(std::uint32_t x, std::uint32_t y, std::uint32_t fallback) const {
    const TileCell * cell = try_cell(x, y);
    return cell == nullptr ? fallback : cell->atlas_index;
}

bool TileMap::set_atlas_index(std::uint32_t x, std::uint32_t y, std::uint32_t atlas_index) {
    TileCell * cell = try_cell(x, y);
    if (cell == nullptr) {
        return false;
    }
    cell->atlas_index = atlas_index;
    return true;
}

Vec2f TileMap::cell_center_world(std::uint32_t x, std::uint32_t y) const {
    return {
        origin_.x + (static_cast<float>(x) + 0.5f) * tile_size_.x,
        origin_.y + (static_cast<float>(y) + 0.5f) * tile_size_.y,
    };
}

std::size_t TileMap::to_offset(std::uint32_t x, std::uint32_t y) const {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x);
}
