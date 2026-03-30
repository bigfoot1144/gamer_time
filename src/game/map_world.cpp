#include "game/map_world.h"

#include "common.h"

namespace {

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
        result.points.push_back(origin + Vec2f{point.x, point.y});
    }
    return result;
}

} // namespace

std::uint32_t TileLayer::atlas_index_at(std::uint32_t x, std::uint32_t y, std::uint32_t fallback) const {
    if (x >= width || y >= height) {
        return fallback;
    }

    const std::size_t offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
    return offset < atlas_indices.size() ? atlas_indices[offset] : fallback;
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
            object.position = {source_object.x, source_object.y};
            object.size = {source_object.width, source_object.height};
            object.rotation = source_object.rotation;
            object.visible = source_object.visible;
            object.gid = source_object.gid;
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
