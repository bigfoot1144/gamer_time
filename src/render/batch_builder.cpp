#include "render/batch_builder.h"

RenderBatch BatchBuilder::build(const RenderWorld & render_world) const {
    RenderBatch batch{};
    std::size_t terrain_tile_count = 0;
    for (const RenderTileLayer & layer : render_world.terrain_layers) {
        terrain_tile_count += layer.tiles.size();
    }
    batch.instances.reserve(terrain_tile_count + render_world.projected_units.size());

    batch.terrain_instance_offset = 0;
    for (const RenderTileLayer & layer : render_world.terrain_layers) {
        if (!layer.renderable || !layer.visible || layer.opacity <= 0.0f) {
            continue;
        }

        RenderTileLayerRange range{};
        range.instance_offset = static_cast<std::uint32_t>(batch.instances.size());
        range.opacity = layer.opacity;

        for (const RenderTile & tile : layer.tiles) {
            InstanceData instance{};
            instance.world_pos = tile.world_pos;
            instance.size = tile.size;
            instance.sprite_index = tile.atlas_index;
            instance.opacity = layer.opacity;
            batch.instances.push_back(instance);
        }

        range.instance_count = static_cast<std::uint32_t>(batch.instances.size()) - range.instance_offset;
        batch.terrain_layer_ranges.push_back(range);
    }
    batch.terrain_instance_count = static_cast<std::uint32_t>(batch.instances.size()) - batch.terrain_instance_offset;

    batch.unit_instance_offset = static_cast<std::uint32_t>(batch.instances.size());
    batch.unit_instance_count = static_cast<std::uint32_t>(render_world.projected_units.size());
    for (const ProjectedUnit & projected : render_world.projected_units) {
        InstanceData instance{};
        instance.world_pos = projected.source.world_pos;
        instance.size = projected.source.size;
        instance.sprite_index = projected.source.sprite_index;
        instance.flags = projected.source.selected ? 1u : 0u;
        instance.opacity = 1.0f;
        batch.instances.push_back(instance);
    }

    return batch;
}
