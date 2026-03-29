#include "render/projection_system.h"

void ProjectionSystem::run(RenderWorld & render_world, int window_width, int window_height) const {
    render_world.projected_units.clear();
    render_world.projected_units.reserve(render_world.units.size());

    const float half_width = static_cast<float>(window_width) * 0.5f;
    const float half_height = static_cast<float>(window_height) * 0.5f;

    for (const RenderUnit & unit : render_world.units) {
        ProjectedUnit projected{};
        projected.source = unit;
        projected.screen_pos = {
            (unit.world_pos.x - render_world.camera.world_center.x) * render_world.camera.zoom + half_width,
            (unit.world_pos.y - render_world.camera.world_center.y) * render_world.camera.zoom + half_height,
        };
        projected.depth_key = unit.world_pos.y + unit.size.y * 0.5f;
        render_world.projected_units.push_back(projected);
    }
}
