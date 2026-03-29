#include "game/navigation_system.h"

#include "game/world.h"

void NavigationSystem::update(World & world, float dt_seconds) const {
    constexpr float kMoveSpeed = 160.0f;
    constexpr float kArrivalDistance = 6.0f;

    for (UnitId unit_id : world.unit_ids()) {
        TransformComponent * transform = world.try_transform(unit_id);
        UnitComponent * unit = world.try_unit(unit_id);
        if (!transform || !unit || !unit->has_move_target) {
            continue;
        }

        const Vec2f to_target = unit->move_target - transform->position;
        const float remaining_distance = length(to_target);
        if (remaining_distance <= kArrivalDistance) {
            transform->position = unit->move_target;
            unit->has_move_target = false;
            continue;
        }

        transform->position += normalize_or_zero(to_target) * (kMoveSpeed * dt_seconds);
    }
}
