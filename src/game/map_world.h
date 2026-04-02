#pragma once

#include "assets/tmx_map_loader.h"
#include "core/types.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

constexpr std::uint32_t kEmptyAtlasIndex = UINT32_MAX;

struct MapProperty {
    std::string name;
    std::string type = "string";
    std::string value;

    bool as_bool(bool fallback = false) const;
    std::int32_t as_int(std::int32_t fallback = 0) const;
    float as_float(float fallback = 0.0f) const;
};

struct MapPolygon {
    std::vector<Vec2f> points;
};

enum class MapObjectShape {
    None,
    Point,
    Rectangle,
    Polygon,
};

struct MapObject {
    std::uint32_t id = 0;
    std::string name;
    std::string type;
    Vec2f position{};
    Vec2f size{};
    float rotation = 0.0f;
    bool visible = true;
    std::uint32_t gid = 0;
    MapObjectShape shape = MapObjectShape::None;
    bool has_polygon = false;
    bool is_point = false;
    MapPolygon polygon;
    std::vector<MapProperty> properties;

    const MapProperty * find_property(std::string_view name) const;
    bool property_as_bool(std::string_view name, bool fallback = false) const;
    std::optional<std::int32_t> property_as_int(std::string_view name) const;
    std::optional<float> property_as_float(std::string_view name) const;
    std::string_view property_value(std::string_view name) const;
};

struct TileLayer {
    std::uint32_t id = 0;
    std::string name;
    bool visible = true;
    bool renderable = true;
    float opacity = 1.0f;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint32_t> atlas_indices;
    std::vector<MapProperty> properties;

    std::size_t tile_count() const {
        return atlas_indices.size();
    }

    std::uint32_t atlas_index_at(std::uint32_t x, std::uint32_t y, std::uint32_t fallback = 0) const;
};

struct ObjectLayer {
    std::uint32_t id = 0;
    std::string name;
    bool visible = true;
    float opacity = 1.0f;
    std::vector<MapObject> objects;
    std::vector<MapProperty> properties;

    const MapProperty * find_property(std::string_view name) const;
    bool property_as_bool(std::string_view name, bool fallback = false) const;
    std::optional<std::int32_t> property_as_int(std::string_view name) const;
    std::optional<float> property_as_float(std::string_view name) const;
    std::string_view property_value(std::string_view name) const;
};

struct CollisionPolygon {
    std::uint32_t source_object_id = 0;
    std::string source_layer_name;
    std::vector<Vec2f> points;
};

class MapWorld {
public:
    MapWorld() = default;

    static MapWorld from_tmx(const TmxMapAsset & map_asset);

    bool empty() const;
    std::uint32_t width() const { return width_; }
    std::uint32_t height() const { return height_; }
    Vec2f tile_size() const { return tile_size_; }
    Vec2f origin() const { return origin_; }

    const std::vector<MapProperty> & properties() const { return properties_; }
    const std::vector<TileLayer> & tile_layers() const { return tile_layers_; }
    const std::vector<ObjectLayer> & object_layers() const { return object_layers_; }
    const std::vector<CollisionPolygon> & collision_polygons() const { return collision_polygons_; }

    const TileLayer * find_tile_layer(std::string_view name) const;
    const ObjectLayer * find_object_layer(std::string_view name) const;
    std::size_t total_tile_count() const;

private:
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    Vec2f tile_size_{16.0f, 16.0f};
    Vec2f origin_{};
    std::vector<MapProperty> properties_;
    std::vector<TileLayer> tile_layers_;
    std::vector<ObjectLayer> object_layers_;
    std::vector<CollisionPolygon> collision_polygons_;
};
