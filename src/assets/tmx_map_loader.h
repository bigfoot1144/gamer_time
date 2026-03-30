#pragma once

#include "assets/atlas_asset.h"
#include "game/tile_map.h"

#include <cstdint>
#include <string>
#include <vector>

struct TmxLayerAsset {
    std::string name;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint32_t> gids;
};

struct TmxTilesetAsset {
    std::uint32_t first_gid = 1;
    std::uint32_t tile_width = 0;
    std::uint32_t tile_height = 0;
    std::uint32_t tile_count = 0;
    std::uint32_t columns = 0;
    std::string image_source;
    std::uint32_t image_width = 0;
    std::uint32_t image_height = 0;
};

struct TmxMapAsset {
    std::string map_path;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t tile_width = 0;
    std::uint32_t tile_height = 0;
    TmxTilesetAsset tileset;
    std::vector<TmxLayerAsset> layers;
};

namespace assets {

TmxMapAsset load_tmx_map(const std::string & map_path);
std::string resolve_tmx_tileset_image_path(const TmxMapAsset & map);
AtlasAsset build_atlas_from_tmx(const TmxMapAsset & map, const std::string & image_path);
TileMap build_flattened_tile_map_from_tmx(const TmxMapAsset & map);

} // namespace assets
