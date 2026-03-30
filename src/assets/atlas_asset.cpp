#include "assets/atlas_asset.h"

AtlasAsset AtlasAsset::from_grid_image(
    std::string image_path_value,
    std::uint32_t tile_width_value,
    std::uint32_t tile_height_value,
    std::unordered_map<std::string, std::uint32_t> tile_ids_by_name_value
) {
    AtlasAsset asset{};
    asset.image_path = std::move(image_path_value);
    asset.tile_width = tile_width_value;
    asset.tile_height = tile_height_value;
    asset.tile_ids_by_name = std::move(tile_ids_by_name_value);
    return asset;
}

std::uint32_t AtlasAsset::tile_count() const {
    return columns * rows;
}

bool AtlasAsset::is_valid() const {
    return !image_path.empty() && tile_width > 0 && tile_height > 0;
}

std::uint32_t AtlasAsset::tile_index(std::string_view tile_name, std::uint32_t fallback) const {
    const auto found = tile_ids_by_name.find(std::string(tile_name));
    if (found == tile_ids_by_name.end()) {
        return fallback;
    }
    return found->second;
}
