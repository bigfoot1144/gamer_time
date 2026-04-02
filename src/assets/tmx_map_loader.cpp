#include "assets/tmx_map_loader.h"

#include "common.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>

namespace assets {

namespace {

std::string read_text_file(const std::string & path) {
    std::ifstream file(path);
    if (!file) {
        fail("Failed to open TMX map: " + path);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool is_tag_name_terminator(char ch) {
    return std::isspace(static_cast<unsigned char>(ch)) != 0 || ch == '>' || ch == '/';
}

std::size_t find_tag_start(const std::string & text, std::string_view tag_name, std::size_t start_pos) {
    const std::string open_tag = "<" + std::string(tag_name);
    std::size_t candidate = start_pos;

    while (true) {
        candidate = text.find(open_tag, candidate);
        if (candidate == std::string::npos) {
            return candidate;
        }

        const std::size_t terminator_index = candidate + open_tag.size();
        if (terminator_index >= text.size() || is_tag_name_terminator(text[terminator_index])) {
            return candidate;
        }

        candidate = terminator_index;
    }
}

std::string extract_tag_block(const std::string & text, std::string_view tag_name, std::size_t start_pos = 0) {
    const std::size_t open_pos = find_tag_start(text, tag_name, start_pos);
    if (open_pos == std::string::npos) {
        return {};
    }

    const std::size_t close_pos = text.find('>', open_pos);
    if (close_pos == std::string::npos) {
        fail("Malformed TMX XML for tag: " + std::string(tag_name));
    }

    if (close_pos > open_pos && text[close_pos - 1] == '/') {
        return text.substr(open_pos, close_pos - open_pos + 1);
    }

    const std::string end_tag = "</" + std::string(tag_name) + ">";
    const std::size_t end_pos = text.find(end_tag, close_pos);
    if (end_pos == std::string::npos) {
        fail("Missing TMX closing tag: " + std::string(tag_name));
    }

    return text.substr(open_pos, end_pos + end_tag.size() - open_pos);
}

std::vector<std::string> extract_tag_blocks(const std::string & text, std::string_view tag_name) {
    std::vector<std::string> blocks;
    std::size_t search_pos = 0;

    while (true) {
        const std::string block = extract_tag_block(text, tag_name, search_pos);
        if (block.empty()) {
            break;
        }

        const std::size_t open_pos = find_tag_start(text, tag_name, search_pos);
        search_pos = open_pos + block.size();
        blocks.push_back(block);
    }

    return blocks;
}

std::string extract_inner_xml(std::string_view tag_block) {
    const std::size_t start = tag_block.find('>');
    if (start == std::string::npos) {
        return {};
    }
    if (start > 0 && tag_block[start - 1] == '/') {
        return {};
    }

    const std::size_t end = tag_block.rfind('<');
    if (end == std::string::npos || end <= start) {
        return {};
    }

    return std::string(tag_block.substr(start + 1, end - start - 1));
}

std::string extract_attribute(std::string_view tag_block, std::string_view attribute_name) {
    const std::string pattern = std::string(attribute_name) + "=\"";
    const std::size_t attr_pos = tag_block.find(pattern);
    if (attr_pos == std::string::npos) {
        return {};
    }

    const std::size_t value_start = attr_pos + pattern.size();
    const std::size_t value_end = tag_block.find('"', value_start);
    if (value_end == std::string::npos) {
        fail("Malformed TMX attribute: " + std::string(attribute_name));
    }

    return std::string(tag_block.substr(value_start, value_end - value_start));
}

std::uint32_t parse_u32_attribute(std::string_view tag_block, std::string_view attribute_name) {
    const std::string value = extract_attribute(tag_block, attribute_name);
    if (value.empty()) {
        fail("Missing TMX attribute: " + std::string(attribute_name));
    }
    return static_cast<std::uint32_t>(std::stoul(value));
}

std::uint32_t parse_u32_attribute_or(std::string_view tag_block, std::string_view attribute_name, std::uint32_t fallback) {
    const std::string value = extract_attribute(tag_block, attribute_name);
    return value.empty() ? fallback : static_cast<std::uint32_t>(std::stoul(value));
}

float parse_float_attribute_or(std::string_view tag_block, std::string_view attribute_name, float fallback) {
    const std::string value = extract_attribute(tag_block, attribute_name);
    return value.empty() ? fallback : std::stof(value);
}

bool parse_bool_attribute_or(std::string_view tag_block, std::string_view attribute_name, bool fallback) {
    const std::string value = extract_attribute(tag_block, attribute_name);
    if (value.empty()) {
        return fallback;
    }
    return value != "0";
}

std::vector<std::uint32_t> parse_csv_gids(std::string_view csv) {
    std::vector<std::uint32_t> gids;
    std::string token;
    std::stringstream stream{std::string(csv)};

    while (std::getline(stream, token, ',')) {
        token.erase(std::remove_if(token.begin(), token.end(), [](unsigned char ch) {
            return std::isspace(ch) != 0;
        }), token.end());
        if (token.empty()) {
            continue;
        }
        gids.push_back(static_cast<std::uint32_t>(std::stoul(token)));
    }

    return gids;
}

std::vector<TmxProperty> parse_properties(const std::string & tag_block) {
    const std::string properties_block = extract_tag_block(tag_block, "properties");
    if (properties_block.empty()) {
        return {};
    }

    std::vector<TmxProperty> properties;
    for (const std::string & property_block : extract_tag_blocks(properties_block, "property")) {
        TmxProperty property{};
        property.name = extract_attribute(property_block, "name");
        property.type = extract_attribute(property_block, "type");
        if (property.type.empty()) {
            property.type = "string";
        }
        property.value = extract_attribute(property_block, "value");
        if (property.value.empty()) {
            property.value = extract_inner_xml(property_block);
        }
        properties.push_back(std::move(property));
    }

    return properties;
}

TmxTilesetAsset parse_tileset(const std::string & tileset_block) {
    const std::string image_tag = extract_tag_block(tileset_block, "image");

    TmxTilesetAsset tileset{};
    tileset.first_gid = parse_u32_attribute(tileset_block, "firstgid");
    tileset.name = extract_attribute(tileset_block, "name");
    tileset.tile_width = parse_u32_attribute(tileset_block, "tilewidth");
    tileset.tile_height = parse_u32_attribute(tileset_block, "tileheight");
    tileset.tile_count = parse_u32_attribute_or(tileset_block, "tilecount", 0);
    tileset.columns = parse_u32_attribute_or(tileset_block, "columns", 0);
    tileset.image_source = extract_attribute(image_tag, "source");
    tileset.image_width = parse_u32_attribute_or(image_tag, "width", 0);
    tileset.image_height = parse_u32_attribute_or(image_tag, "height", 0);
    tileset.properties = parse_properties(tileset_block);
    return tileset;
}

TmxTileLayerAsset parse_tile_layer(const std::string & layer_block) {
    TmxTileLayerAsset layer{};
    layer.id = parse_u32_attribute_or(layer_block, "id", 0);
    layer.name = extract_attribute(layer_block, "name");
    layer.visible = parse_bool_attribute_or(layer_block, "visible", true);
    layer.opacity = parse_float_attribute_or(layer_block, "opacity", 1.0f);
    layer.properties = parse_properties(layer_block);
    layer.width = parse_u32_attribute(layer_block, "width");
    layer.height = parse_u32_attribute(layer_block, "height");

    const std::string data_block = extract_tag_block(layer_block, "data");
    if (extract_attribute(data_block, "encoding") != "csv") {
        fail("Only CSV-encoded TMX layer data is supported");
    }

    layer.gids = parse_csv_gids(extract_inner_xml(data_block));
    if (layer.gids.size() != static_cast<std::size_t>(layer.width) * static_cast<std::size_t>(layer.height)) {
        fail("TMX layer size does not match width*height for layer: " + layer.name);
    }

    return layer;
}

std::vector<TmxPolygonPoint> parse_polygon_points(std::string_view encoded_points) {
    std::vector<TmxPolygonPoint> points;
    std::stringstream stream{std::string(encoded_points)};
    std::string token;

    while (std::getline(stream, token, ' ')) {
        if (token.empty()) {
            continue;
        }

        const std::size_t comma = token.find(',');
        if (comma == std::string::npos) {
            fail("Malformed TMX polygon point list");
        }

        TmxPolygonPoint point{};
        point.x = std::stof(token.substr(0, comma));
        point.y = std::stof(token.substr(comma + 1));
        points.push_back(point);
    }

    return points;
}

TmxObjectAsset parse_object(const std::string & object_block) {
    TmxObjectAsset object{};
    object.id = parse_u32_attribute_or(object_block, "id", 0);
    object.name = extract_attribute(object_block, "name");
    object.type = extract_attribute(object_block, "type");
    object.x = parse_float_attribute_or(object_block, "x", 0.0f);
    object.y = parse_float_attribute_or(object_block, "y", 0.0f);
    object.width = parse_float_attribute_or(object_block, "width", 0.0f);
    object.height = parse_float_attribute_or(object_block, "height", 0.0f);
    object.rotation = parse_float_attribute_or(object_block, "rotation", 0.0f);
    object.visible = parse_bool_attribute_or(object_block, "visible", true);
    object.gid = parse_u32_attribute_or(object_block, "gid", 0);
    object.properties = parse_properties(object_block);

    const std::string polygon_block = extract_tag_block(object_block, "polygon");
    if (!polygon_block.empty()) {
        object.has_polygon = true;
        object.polygon.points = parse_polygon_points(extract_attribute(polygon_block, "points"));
    }

    object.is_point = !extract_tag_block(object_block, "point").empty();

    return object;
}

TmxObjectLayerAsset parse_object_layer(const std::string & object_group_block) {
    TmxObjectLayerAsset layer{};
    layer.id = parse_u32_attribute_or(object_group_block, "id", 0);
    layer.name = extract_attribute(object_group_block, "name");
    layer.visible = parse_bool_attribute_or(object_group_block, "visible", true);
    layer.opacity = parse_float_attribute_or(object_group_block, "opacity", 1.0f);
    layer.properties = parse_properties(object_group_block);

    for (const std::string & object_block : extract_tag_blocks(object_group_block, "object")) {
        layer.objects.push_back(parse_object(object_block));
    }

    return layer;
}

std::vector<TmxLayerRef> parse_layer_order(
    const std::string & map_inner_xml,
    std::vector<TmxTileLayerAsset> & tile_layers,
    std::vector<TmxObjectLayerAsset> & object_layers
) {
    std::vector<TmxLayerRef> order;
    std::size_t search_pos = 0;

    while (true) {
        const std::size_t tile_pos = map_inner_xml.find("<layer", search_pos);
        const std::size_t object_pos = map_inner_xml.find("<objectgroup", search_pos);
        const std::size_t next_pos = std::min(
            tile_pos == std::string::npos ? map_inner_xml.size() : tile_pos,
            object_pos == std::string::npos ? map_inner_xml.size() : object_pos
        );
        if (next_pos == map_inner_xml.size()) {
            break;
        }

        if (tile_pos != std::string::npos && tile_pos == next_pos) {
            const std::string layer_block = extract_tag_block(map_inner_xml, "layer", tile_pos);
            tile_layers.push_back(parse_tile_layer(layer_block));
            order.push_back({TmxLayerType::Tile, tile_layers.size() - 1});
            search_pos = tile_pos + layer_block.size();
            continue;
        }

        const std::string object_group_block = extract_tag_block(map_inner_xml, "objectgroup", object_pos);
        object_layers.push_back(parse_object_layer(object_group_block));
        order.push_back({TmxLayerType::Object, object_layers.size() - 1});
        search_pos = object_pos + object_group_block.size();
    }

    return order;
}

const TmxTilesetAsset & require_tileset(const TmxMapAsset & map, std::size_t tileset_index) {
    if (tileset_index >= map.tilesets.size()) {
        fail("TMX map does not contain the requested tileset");
    }
    return map.tilesets[tileset_index];
}

} // namespace

TmxMapAsset load_tmx_map(const std::string & map_path) {
    const std::string xml = read_text_file(map_path);
    const std::string map_tag = extract_tag_block(xml, "map");

    TmxMapAsset map{};
    map.map_path = map_path;
    map.orientation = extract_attribute(map_tag, "orientation");
    map.render_order = extract_attribute(map_tag, "renderorder");
    map.width = parse_u32_attribute(map_tag, "width");
    map.height = parse_u32_attribute(map_tag, "height");
    map.tile_width = parse_u32_attribute(map_tag, "tilewidth");
    map.tile_height = parse_u32_attribute(map_tag, "tileheight");
    map.next_layer_id = parse_u32_attribute_or(map_tag, "nextlayerid", 0);
    map.next_object_id = parse_u32_attribute_or(map_tag, "nextobjectid", 0);
    map.properties = parse_properties(map_tag);

    for (const std::string & tileset_block : extract_tag_blocks(map_tag, "tileset")) {
        map.tilesets.push_back(parse_tileset(tileset_block));
    }
    if (map.tilesets.empty()) {
        fail("TMX map must contain at least one tileset");
    }

    const std::string map_inner_xml = extract_inner_xml(map_tag);
    map.layer_order = parse_layer_order(map_inner_xml, map.tile_layers, map.object_layers);
    return map;
}

std::string resolve_tmx_tileset_image_path(const TmxMapAsset & map, std::size_t tileset_index) {
    const TmxTilesetAsset & tileset = require_tileset(map, tileset_index);
    const std::filesystem::path map_dir = std::filesystem::path(map.map_path).parent_path();
    const std::filesystem::path relative_candidate = map_dir / tileset.image_source;
    if (std::filesystem::exists(relative_candidate)) {
        return relative_candidate.string();
    }

    const std::filesystem::path tiles_candidate = map_dir.parent_path() / "tiles" / tileset.image_source;
    if (std::filesystem::exists(tiles_candidate)) {
        return tiles_candidate.string();
    }

    fail("Failed to resolve TMX tileset image: " + tileset.image_source);
}

AtlasAsset build_atlas_from_tmx(const TmxMapAsset & map, const std::string & image_path, std::size_t tileset_index) {
    const TmxTilesetAsset & tileset = require_tileset(map, tileset_index);
    AtlasAsset atlas = AtlasAsset::from_grid_image(
        image_path,
        tileset.tile_width,
        tileset.tile_height
    );
    atlas.columns = tileset.columns;
    atlas.rows = tileset.tile_count / std::max(1u, tileset.columns);
    return atlas;
}

} // namespace assets
