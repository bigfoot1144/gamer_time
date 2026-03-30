#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

struct AtlasAsset {
    std::string image_path;
    std::uint32_t tile_width = 0;
    std::uint32_t tile_height = 0;
    std::uint32_t columns = 0;
    std::uint32_t rows = 0;
    std::unordered_map<std::string, std::uint32_t> tile_ids_by_name;

    static AtlasAsset from_grid_image(
        std::string image_path,
        std::uint32_t tile_width,
        std::uint32_t tile_height,
        std::unordered_map<std::string, std::uint32_t> tile_ids_by_name = {}
    );

    std::uint32_t tile_count() const;
    bool is_valid() const;
    std::uint32_t tile_index(std::string_view tile_name, std::uint32_t fallback = 0) const;
};
