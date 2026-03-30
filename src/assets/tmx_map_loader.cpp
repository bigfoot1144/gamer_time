#include "assets/tmx_map_loader.h"

#include "common.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
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

std::string extract_tag_block(const std::string & text, std::string_view tag_name, std::size_t start_pos = 0) {
    const std::string open_tag = "<" + std::string(tag_name);
    const std::size_t open_pos = text.find(open_tag, start_pos);
    if (open_pos == std::string::npos) {
        return {};
    }

    const std::size_t close_pos = text.find('>', open_pos);
    if (close_pos == std::string::npos) {
        fail("Malformed TMX XML for tag: " + std::string(tag_name));
    }

    if (text[close_pos - 1] == '/') {
        return text.substr(open_pos, close_pos - open_pos + 1);
    }

    const std::string end_tag = "</" + std::string(tag_name) + ">";
    const std::size_t end_pos = text.find(end_tag, close_pos);
    if (end_pos == std::string::npos) {
        fail("Missing TMX closing tag: " + std::string(tag_name));
    }

    return text.substr(open_pos, end_pos + end_tag.size() - open_pos);
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

std::vector<std::string> extract_layer_blocks(const std::string & xml) {
    std::vector<std::string> blocks;
    std::size_t search_pos = 0;

    while (true) {
        const std::size_t layer_pos = xml.find("<layer", search_pos);
        if (layer_pos == std::string::npos) {
            break;
        }

        const std::size_t layer_end = xml.find("</layer>", layer_pos);
        if (layer_end == std::string::npos) {
            fail("Malformed TMX layer block");
        }

        const std::size_t block_end = layer_end + std::string("</layer>").size();
        blocks.push_back(xml.substr(layer_pos, block_end - layer_pos));
        search_pos = block_end;
    }

    return blocks;
}

} // namespace

TmxMapAsset load_tmx_map(const std::string & map_path) {
    const std::string xml = read_text_file(map_path);
    const std::string map_tag = extract_tag_block(xml, "map");
    const std::string tileset_tag = extract_tag_block(xml, "tileset");
    const std::string image_tag = extract_tag_block(tileset_tag, "image");

    TmxMapAsset map{};
    map.map_path = map_path;
    map.width = parse_u32_attribute(map_tag, "width");
    map.height = parse_u32_attribute(map_tag, "height");
    map.tile_width = parse_u32_attribute(map_tag, "tilewidth");
    map.tile_height = parse_u32_attribute(map_tag, "tileheight");

    map.tileset.first_gid = parse_u32_attribute(tileset_tag, "firstgid");
    map.tileset.tile_width = parse_u32_attribute(tileset_tag, "tilewidth");
    map.tileset.tile_height = parse_u32_attribute(tileset_tag, "tileheight");
    map.tileset.tile_count = parse_u32_attribute(tileset_tag, "tilecount");
    map.tileset.columns = parse_u32_attribute(tileset_tag, "columns");
    map.tileset.image_source = extract_attribute(image_tag, "source");
    map.tileset.image_width = parse_u32_attribute(image_tag, "width");
    map.tileset.image_height = parse_u32_attribute(image_tag, "height");

    for (const std::string & layer_block : extract_layer_blocks(xml)) {
        TmxLayerAsset layer{};
        layer.name = extract_attribute(layer_block, "name");
        layer.width = parse_u32_attribute(layer_block, "width");
        layer.height = parse_u32_attribute(layer_block, "height");

        const std::string data_block = extract_tag_block(layer_block, "data");
        if (extract_attribute(data_block, "encoding") != "csv") {
            fail("Only CSV-encoded TMX layer data is supported");
        }

        const std::size_t csv_start = data_block.find('>');
        const std::size_t csv_end = data_block.rfind("</data>");
        if (csv_start == std::string::npos || csv_end == std::string::npos || csv_end <= csv_start) {
            fail("Malformed TMX layer data block");
        }

        layer.gids = parse_csv_gids(std::string_view(data_block).substr(csv_start + 1, csv_end - csv_start - 1));
        if (layer.gids.size() != static_cast<std::size_t>(layer.width) * static_cast<std::size_t>(layer.height)) {
            fail("TMX layer size does not match width*height for layer: " + layer.name);
        }

        map.layers.push_back(std::move(layer));
    }

    return map;
}

std::string resolve_tmx_tileset_image_path(const TmxMapAsset & map) {
    const std::filesystem::path map_dir = std::filesystem::path(map.map_path).parent_path();
    const std::filesystem::path relative_candidate = map_dir / map.tileset.image_source;
    if (std::filesystem::exists(relative_candidate)) {
        return relative_candidate.string();
    }

    const std::filesystem::path tiles_candidate = map_dir.parent_path() / "tiles" / map.tileset.image_source;
    if (std::filesystem::exists(tiles_candidate)) {
        return tiles_candidate.string();
    }

    fail("Failed to resolve TMX tileset image: " + map.tileset.image_source);
}

AtlasAsset build_atlas_from_tmx(const TmxMapAsset & map, const std::string & image_path) {
    AtlasAsset atlas = AtlasAsset::from_grid_image(
        image_path,
        map.tileset.tile_width,
        map.tileset.tile_height
    );
    atlas.columns = map.tileset.columns;
    atlas.rows = map.tileset.tile_count / std::max(1u, map.tileset.columns);
    return atlas;
}

TileMap build_flattened_tile_map_from_tmx(const TmxMapAsset & map) {
    const Vec2f tile_size{
        static_cast<float>(map.tile_width),
        static_cast<float>(map.tile_height),
    };
    const Vec2f origin{
        -static_cast<float>(map.width) * tile_size.x * 0.5f,
        -static_cast<float>(map.height) * tile_size.y * 0.5f,
    };

    TileMap tile_map(map.width, map.height, tile_size, origin);
    for (std::uint32_t y = 0; y < map.height; ++y) {
        for (std::uint32_t x = 0; x < map.width; ++x) {
            const std::size_t offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(map.width) + static_cast<std::size_t>(x);
            std::uint32_t visible_gid = 0;

            for (const TmxLayerAsset & layer : map.layers) {
                if (offset >= layer.gids.size()) {
                    continue;
                }
                const std::uint32_t gid = layer.gids[offset];
                if (gid != 0) {
                    visible_gid = gid;
                }
            }

            if (visible_gid == 0 || visible_gid < map.tileset.first_gid) {
                tile_map.set_atlas_index(x, y, 0);
            } else {
                tile_map.set_atlas_index(x, y, visible_gid - map.tileset.first_gid);
            }
        }
    }

    return tile_map;
}

} // namespace assets
