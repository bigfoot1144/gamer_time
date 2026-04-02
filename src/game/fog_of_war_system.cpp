#include "game/fog_of_war_system.h"

#include "game/world.h"

#include <algorithm>
#include <cmath>

void FogOfWarSystem::update(World & world) const {
    std::fill(world.fog_mask().begin(), world.fog_mask().end(), static_cast<std::uint8_t>(0));

    const float cell_width = 16.0f;
    const float cell_height = 16.0f;
    const float half_width = static_cast<float>(world.fog_width()) * cell_width * 0.5f;
    const float half_height = static_cast<float>(world.fog_height()) * cell_height * 0.5f;

    for (UnitId unit_id : world.unit_ids()) {
        const TransformComponent * transform = world.try_transform(unit_id);
        const VisionComponent * vision = world.try_vision(unit_id);
        if (!transform || !vision) {
            continue;
        }

        const CollisionBounds vision_bounds{
            .min = {transform->position.x - vision->radius, transform->position.y - vision->radius},
            .max = {transform->position.x + vision->radius, transform->position.y + vision->radius},
        };
        const std::vector<const CollisionShape *> nearby_colliders = world.collision().query_bounds(vision_bounds);

        const int min_x = std::max(0, static_cast<int>(std::floor((transform->position.x - vision->radius + half_width) / cell_width)));
        const int max_x = std::min(static_cast<int>(world.fog_width()) - 1, static_cast<int>(std::ceil((transform->position.x + vision->radius + half_width) / cell_width)));
        const int min_y = std::max(0, static_cast<int>(std::floor((transform->position.y - vision->radius + half_height) / cell_height)));
        const int max_y = std::min(static_cast<int>(world.fog_height()) - 1, static_cast<int>(std::ceil((transform->position.y + vision->radius + half_height) / cell_height)));

        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                const float cell_center_x = (static_cast<float>(x) + 0.5f) * cell_width - half_width;
                const float cell_center_y = (static_cast<float>(y) + 0.5f) * cell_height - half_height;
                const Vec2f cell_center{cell_center_x, cell_center_y};
                const Vec2f delta{cell_center_x - transform->position.x, cell_center_y - transform->position.y};
                if (length_squared(delta) > vision->radius * vision->radius) {
                    continue;
                }

                if (!nearby_colliders.empty() && world.collision().blocks_point(nearby_colliders, cell_center)) {
                    continue;
                }

                if (!nearby_colliders.empty() && world.collision().blocks_segment(nearby_colliders, transform->position, cell_center)) {
                    continue;
                }

                world.fog_mask()[static_cast<std::size_t>(y) * world.fog_width() + static_cast<std::size_t>(x)] = 255;
            }
        }
    }
}
