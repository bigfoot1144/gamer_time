#include "render/render_extractor.h"

#include "game/world.h"

RenderWorld RenderExtractor::build(
    const World & world,
    const CameraState & camera,
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

    return render_world;
}
