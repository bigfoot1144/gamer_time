#include "render/render_extractor.h"

#include "game/map_world.h"
#include "game/world.h"

#include <cmath>

RenderWorld RenderExtractor::build(
    const World & world,
    const CameraState & camera,
    bool show_collision_debug,
    const std::vector<std::string> & ai_events,
    std::string overlay_text
) const {
    RenderWorld render_world{};
    render_world.camera = camera;
    render_world.overlay_text = std::move(overlay_text);

    if (!ai_events.empty()) {
        render_world.overlay_text += "\n\nRecent AI Events:\n";
        for (const std::string & event : ai_events) {
            render_world.overlay_text += "- ";
            render_world.overlay_text += event;
            render_world.overlay_text += '\n';
        }
    }

    const MapWorld & map = world.map();
    render_world.terrain_layers.reserve(map.tile_layers().size());
    for (const TileLayer & layer : map.tile_layers()) {
        RenderTileLayer render_layer{};
        render_layer.name = layer.name;
        render_layer.visible = layer.visible;
        render_layer.renderable = layer.renderable;
        render_layer.opacity = layer.opacity;
        render_layer.tiles.reserve(layer.tile_count());

        for (std::uint32_t y = 0; y < layer.height; ++y) {
            for (std::uint32_t x = 0; x < layer.width; ++x) {
                const std::uint32_t atlas_index = layer.atlas_index_at(x, y, kEmptyAtlasIndex);
                if (atlas_index == kEmptyAtlasIndex) {
                    continue;
                }

                const float world_y = map.origin().y + (static_cast<float>(layer.height - 1 - y) + 0.5f) * map.tile_size().y;
                RenderTile tile{};
                tile.world_pos = {
                    map.origin().x + (static_cast<float>(x) + 0.5f) * map.tile_size().x,
                    world_y,
                };
                tile.size = map.tile_size();
                tile.atlas_index = atlas_index;
                render_layer.tiles.push_back(tile);
            }
        }

        render_world.terrain_layers.push_back(std::move(render_layer));
    }

    render_world.units.reserve(world.unit_count());
    for (UnitId unit_id : world.unit_ids()) {
        const TransformComponent * transform = world.try_transform(unit_id);
        const RenderComponent * render = world.try_render(unit_id);
        const UnitComponent * unit = world.try_unit(unit_id);
        if (!transform || !render || !unit) {
            continue;
        }

        RenderUnit render_unit{};
        render_unit.id = unit_id;
        render_unit.world_pos = transform->position;
        render_unit.size = render->footprint;
        render_unit.sprite_index = render->sprite_index;
        render_unit.selected = unit->selected;
        render_world.units.push_back(render_unit);
    }

    if (show_collision_debug) {
        constexpr float kDebugLineThickness = 2.0f;

        for (const CollisionShape & shape : world.collision().shapes()) {
            if (shape.points.size() < 2) {
                continue;
            }

            for (std::size_t i = 0; i < shape.points.size(); ++i) {
                const Vec2f & start = shape.points[i];
                const Vec2f & end = shape.points[(i + 1) % shape.points.size()];
                const Vec2f edge = end - start;
                const float edge_length = length(edge);
                if (edge_length <= 0.001f) {
                    continue;
                }

                RenderDebugQuad quad{};
                quad.world_pos = (start + end) * 0.5f;
                quad.size = {edge_length, kDebugLineThickness};
                quad.rotation_radians = std::atan2(edge.y, edge.x);
                quad.flags = kInstanceFlagDebugCollision | kInstanceFlagIgnoreFog;
                quad.opacity = 0.9f;
                render_world.debug_quads.push_back(quad);
            }
        }
    }

    return render_world;
}
