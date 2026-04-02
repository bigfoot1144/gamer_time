#include "game/map_world.h"

#include "common.h"

#include <charconv>
#include <string_view>
#include <system_error>

namespace {

const MapProperty * find_property_by_name(const std::vector<MapProperty> & properties, std::string_view name) {
    for (const MapProperty & property : properties) {
        if (property.name == name) {
            return &property;
        }
    }
    return nullptr;
}

bool is_truthy_string(std::string_view value) {
    return value == "1" || value == "true" || value == "True" || value == "TRUE";
}

std::optional<std::int32_t> parse_int_value(std::string_view value) {
    std::int32_t parsed = 0;
    const char * begin = value.data();
    const char * end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc() || result.ptr != end) {
        return std::nullopt;
    }
    return parsed;
}

std::optional<float> parse_float_value(std::string_view value) {
    try {
        std::size_t parsed_characters = 0;
        const float parsed = std::stof(std::string(value), &parsed_characters);
        if (parsed_characters != value.size()) {
            return std::nullopt;
        }
        return parsed;
    } catch (...) {
        return std::nullopt;
    }
}

MapProperty to_map_property(const TmxProperty & property) {
    return {
        .name = property.name,
        .type = property.type,
        .value = property.value,
    };
}

std::vector<MapProperty> to_map_properties(const std::vector<TmxProperty> & properties) {
    std::vector<MapProperty> result;
    result.reserve(properties.size());
    for (const TmxProperty & property : properties) {
        result.push_back(to_map_property(property));
    }
    return result;
}

MapPolygon to_map_polygon(const TmxPolygon & polygon, const Vec2f & origin) {
    MapPolygon result{};
    result.points.reserve(polygon.points.size());
    for (const TmxPolygonPoint & point : polygon.points) {
        result.points.push_back(origin + Vec2f{point.x, -point.y});
    }
    return result;
}

Vec2f to_world_object_position(const Vec2f & map_origin, float map_pixel_height, const TmxObjectAsset & object) {
    return {
        map_origin.x + object.x,
        map_origin.y + map_pixel_height - object.y,
    };
}

} // namespace

bool MapProperty::as_bool(bool fallback) const {
    if (type == "bool" || type == "boolean") {
        return is_truthy_string(value);
    }
    if (value == "0" || value == "false" || value == "False" || value == "FALSE") {
        return false;
    }
    if (is_truthy_string(value)) {
        return true;
    }
    return fallback;
}

std::int32_t MapProperty::as_int(std::int32_t fallback) const {
    const std::optional<std::int32_t> parsed = parse_int_value(value);
    return parsed.value_or(fallback);
}

float MapProperty::as_float(float fallback) const {
    const std::optional<float> parsed = parse_float_value(value);
    return parsed.value_or(fallback);
}

std::uint32_t TileLayer::atlas_index_at(std::uint32_t x, std::uint32_t y, std::uint32_t fallback) const {
    if (x >= width || y >= height) {
        return fallback;
    }

    const std::size_t offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
    return offset < atlas_indices.size() ? atlas_indices[offset] : fallback;
}

const MapProperty * MapObject::find_property(std::string_view name) const {
    return find_property_by_name(properties, name);
}

bool MapObject::property_as_bool(std::string_view name, bool fallback) const {
    const MapProperty * property = find_property(name);
    return property ? property->as_bool(fallback) : fallback;
}

std::optional<std::int32_t> MapObject::property_as_int(std::string_view name) const {
    const MapProperty * property = find_property(name);
    return property ? parse_int_value(property->value) : std::nullopt;
}

std::optional<float> MapObject::property_as_float(std::string_view name) const {
    const MapProperty * property = find_property(name);
    return property ? parse_float_value(property->value) : std::nullopt;
}

std::string_view MapObject::property_value(std::string_view name) const {
    const MapProperty * property = find_property(name);
    return property ? std::string_view{property->value} : std::string_view{};
}

const MapProperty * ObjectLayer::find_property(std::string_view name) const {
    return find_property_by_name(properties, name);
}

bool ObjectLayer::property_as_bool(std::string_view name, bool fallback) const {
    const MapProperty * property = find_property(name);
    return property ? property->as_bool(fallback) : fallback;
}

std::optional<std::int32_t> ObjectLayer::property_as_int(std::string_view name) const {
    const MapProperty * property = find_property(name);
    return property ? parse_int_value(property->value) : std::nullopt;
}

std::optional<float> ObjectLayer::property_as_float(std::string_view name) const {
    const MapProperty * property = find_property(name);
    return property ? parse_float_value(property->value) : std::nullopt;
}

std::string_view ObjectLayer::property_value(std::string_view name) const {
    const MapProperty * property = find_property(name);
    return property ? std::string_view{property->value} : std::string_view{};
}

MapWorld MapWorld::from_tmx(const TmxMapAsset & map_asset) {
    if (map_asset.tilesets.empty()) {
        fail("TMX map must contain at least one tileset before conversion to MapWorld");
    }

    const TmxTilesetAsset & primary_tileset = map_asset.tilesets.front();

    MapWorld map{};
    map.width_ = map_asset.width;
    map.height_ = map_asset.height;
    map.tile_size_ = {
        static_cast<float>(map_asset.tile_width),
        static_cast<float>(map_asset.tile_height),
    };
    map.origin_ = {
        -static_cast<float>(map_asset.width) * map.tile_size_.x * 0.5f,
        -static_cast<float>(map_asset.height) * map.tile_size_.y * 0.5f,
    };
    const float map_pixel_height = static_cast<float>(map_asset.height) * map.tile_size_.y;
    map.properties_ = to_map_properties(map_asset.properties);

    for (const TmxLayerRef & layer_ref : map_asset.layer_order) {
        if (layer_ref.type == TmxLayerType::Tile) {
            const TmxTileLayerAsset & source = map_asset.tile_layers.at(layer_ref.index);
            TileLayer layer{};
            layer.id = source.id;
            layer.name = source.name;
            layer.visible = source.visible;
            layer.renderable = true;
            layer.opacity = source.opacity;
            layer.width = source.width;
            layer.height = source.height;
            layer.properties = to_map_properties(source.properties);
            layer.atlas_indices.reserve(source.gids.size());

            for (std::uint32_t gid : source.gids) {
                if (gid == 0 || gid < primary_tileset.first_gid) {
                    layer.atlas_indices.push_back(kEmptyAtlasIndex);
                } else {
                    layer.atlas_indices.push_back(gid - primary_tileset.first_gid);
                }
            }

            map.tile_layers_.push_back(std::move(layer));
            continue;
        }

        const TmxObjectLayerAsset & source = map_asset.object_layers.at(layer_ref.index);
        ObjectLayer layer{};
        layer.id = source.id;
        layer.name = source.name;
        layer.visible = source.visible;
        layer.opacity = source.opacity;
        layer.properties = to_map_properties(source.properties);
        layer.objects.reserve(source.objects.size());

        for (const TmxObjectAsset & source_object : source.objects) {
            MapObject object{};
            object.id = source_object.id;
            object.name = source_object.name;
            object.type = source_object.type;
            object.position = to_world_object_position(map.origin_, map_pixel_height, source_object);
            object.size = {source_object.width, source_object.height};
            object.rotation = source_object.rotation;
            object.visible = source_object.visible;
            object.gid = source_object.gid;
            object.is_point = source_object.is_point;
            object.shape = source_object.has_polygon ? MapObjectShape::Polygon
                           : source_object.is_point  ? MapObjectShape::Point
                                                     : MapObjectShape::Rectangle;
            object.has_polygon = source_object.has_polygon;
            object.properties = to_map_properties(source_object.properties);
            if (source_object.has_polygon) {
                object.polygon = to_map_polygon(source_object.polygon, object.position);
            }

            if (layer.name == "collision" && object.has_polygon) {
                CollisionPolygon collision{};
                collision.source_object_id = object.id;
                collision.source_layer_name = layer.name;
                collision.points = object.polygon.points;
                map.collision_polygons_.push_back(collision);
            }

            layer.objects.push_back(std::move(object));
        }

        map.object_layers_.push_back(std::move(layer));
    }

    return map;
}

bool MapWorld::empty() const {
    return width_ == 0 || height_ == 0;
}

const TileLayer * MapWorld::find_tile_layer(std::string_view name) const {
    for (const TileLayer & layer : tile_layers_) {
        if (layer.name == name) {
            return &layer;
        }
    }
    return nullptr;
}

const ObjectLayer * MapWorld::find_object_layer(std::string_view name) const {
    for (const ObjectLayer & layer : object_layers_) {
        if (layer.name == name) {
            return &layer;
        }
    }
    return nullptr;
}

std::size_t MapWorld::total_tile_count() const {
    std::size_t total = 0;
    for (const TileLayer & layer : tile_layers_) {
        total += layer.tile_count();
    }
    return total;
}
