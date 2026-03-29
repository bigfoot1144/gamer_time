#include "render/batch_builder.h"

RenderBatch BatchBuilder::build(const RenderWorld & render_world) const {
    RenderBatch batch{};
    batch.instances.reserve(render_world.projected_units.size());

    for (const ProjectedUnit & projected : render_world.projected_units) {
        InstanceData instance{};
        instance.world_pos = projected.source.world_pos;
        instance.size = projected.source.size;
        instance.sprite_index = projected.source.sprite_index;
        instance.flags = projected.source.selected ? 1u : 0u;
        batch.instances.push_back(instance);
    }

    return batch;
}
