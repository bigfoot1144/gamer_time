#include "render/frustum_culler.h"

#include <algorithm>

void FrustumCuller::run(RenderWorld & render_world, int window_width, int window_height) const {
    const float half_width = std::max(1.0f, static_cast<float>(window_width) / render_world.camera.zoom) * 0.5f;
    const float half_height = std::max(1.0f, static_cast<float>(window_height) / render_world.camera.zoom) * 0.5f;
    const float min_x = render_world.camera.world_center.x - half_width;
    const float max_x = render_world.camera.world_center.x + half_width;
    const float min_y = render_world.camera.world_center.y - half_height;
    const float max_y = render_world.camera.world_center.y + half_height;

    std::vector<RenderTileLayer> visible_layers;
    visible_layers.reserve(render_world.terrain_layers.size());
    for (const RenderTileLayer & layer : render_world.terrain_layers) {
        RenderTileLayer visible_layer = layer;
        visible_layer.tiles.clear();
        visible_layer.tiles.reserve(layer.tiles.size());

        for (const RenderTile & tile : layer.tiles) {
            if (tile.world_pos.x + tile.size.x < min_x || tile.world_pos.x - tile.size.x > max_x) {
                continue;
            }
            if (tile.world_pos.y + tile.size.y < min_y || tile.world_pos.y - tile.size.y > max_y) {
                continue;
            }
            visible_layer.tiles.push_back(tile);
        }

        visible_layers.push_back(std::move(visible_layer));
    }

    std::vector<RenderUnit> visible_units;
    visible_units.reserve(render_world.units.size());

    for (const RenderUnit & unit : render_world.units) {
        if (unit.world_pos.x + unit.size.x < min_x || unit.world_pos.x - unit.size.x > max_x) {
            continue;
        }
        if (unit.world_pos.y + unit.size.y < min_y || unit.world_pos.y - unit.size.y > max_y) {
            continue;
        }
        visible_units.push_back(unit);
    }

    render_world.terrain_layers = std::move(visible_layers);
    render_world.units = std::move(visible_units);
}
