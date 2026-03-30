#include "render/batch_builder.h"

RenderBatch BatchBuilder::build(const RenderWorld & render_world) const {
    RenderBatch batch{};
    batch.instances.reserve(render_world.terrain_tiles.size() + render_world.projected_units.size());

    batch.terrain_instance_offset = 0;
    batch.terrain_instance_count = static_cast<std::uint32_t>(render_world.terrain_tiles.size());
    for (const RenderTile & tile : render_world.terrain_tiles) {
        InstanceData instance{};
        instance.world_pos = tile.world_pos;
        instance.size = tile.size;
        instance.sprite_index = tile.atlas_index;
        batch.instances.push_back(instance);
    }

    batch.unit_instance_offset = static_cast<std::uint32_t>(batch.instances.size());
    batch.unit_instance_count = static_cast<std::uint32_t>(render_world.projected_units.size());
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
