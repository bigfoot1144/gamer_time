#pragma once

#include "assets/atlas_asset.h"

#include <cstdint>
#include <string>
#include <vector>

struct TmxProperty {
    std::string name;
    std::string type = "string";
    std::string value;
};

struct TmxPolygonPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct TmxPolygon {
    std::vector<TmxPolygonPoint> points;
};

struct TmxTilesetAsset {
    std::uint32_t first_gid = 1;
    std::string name;
    std::uint32_t tile_width = 0;
    std::uint32_t tile_height = 0;
    std::uint32_t tile_count = 0;
    std::uint32_t columns = 0;
    std::string image_source;
    std::uint32_t image_width = 0;
    std::uint32_t image_height = 0;
    std::vector<TmxProperty> properties;
};

struct TmxLayerAsset {
    std::uint32_t id = 0;
    std::string name;
    bool visible = true;
    float opacity = 1.0f;
    std::vector<TmxProperty> properties;
};

struct TmxTileLayerAsset : TmxLayerAsset {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint32_t> gids;
};

struct TmxObjectAsset {
    std::uint32_t id = 0;
    std::string name;
    std::string type;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float rotation = 0.0f;
    bool visible = true;
    std::uint32_t gid = 0;
    bool has_polygon = false;
    bool is_point = false;
    TmxPolygon polygon;
    std::vector<TmxProperty> properties;
};

struct TmxObjectLayerAsset : TmxLayerAsset {
    std::vector<TmxObjectAsset> objects;
};

enum class TmxLayerType {
    Tile,
    Object,
};

struct TmxLayerRef {
    TmxLayerType type = TmxLayerType::Tile;
    std::size_t index = 0;
};

struct TmxMapAsset {
    std::string map_path;
    std::string orientation;
    std::string render_order;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t tile_width = 0;
    std::uint32_t tile_height = 0;
    std::uint32_t next_layer_id = 0;
    std::uint32_t next_object_id = 0;
    std::vector<TmxProperty> properties;
    std::vector<TmxTilesetAsset> tilesets;
    std::vector<TmxTileLayerAsset> tile_layers;
    std::vector<TmxObjectLayerAsset> object_layers;
    std::vector<TmxLayerRef> layer_order;
};

namespace assets {

TmxMapAsset load_tmx_map(const std::string & map_path);
std::string resolve_tmx_tileset_image_path(const TmxMapAsset & map, std::size_t tileset_index = 0);
AtlasAsset build_atlas_from_tmx(const TmxMapAsset & map, const std::string & image_path, std::size_t tileset_index = 0);

} // namespace assets
