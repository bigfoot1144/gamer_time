#include "game/world.h"

namespace {

constexpr UnitId kInvalidUnitId = static_cast<UnitId>(-1);
constexpr std::uint32_t kFogWidth = 64;
constexpr std::uint32_t kFogHeight = 64;

} // namespace

World::World()
    : fog_mask_(static_cast<std::size_t>(kFogWidth * kFogHeight), 0) {
}

void World::set_map(MapWorld map) {
    map_ = std::move(map);
}

void World::seed_test_units() {
    if (!unit_ids_.empty()) {
        return;
    }

    create_unit({{-160.0f, -80.0f}}, {49, {16.0f, 16.0f}}, {96.0f}, {});
    create_unit({{-60.0f, 0.0f}}, {50, {16.0f, 16.0f}}, {96.0f}, {});
    create_unit({{60.0f, 70.0f}}, {98, {16.0f, 16.0f}}, {112.0f}, {});
    create_unit({{150.0f, -20.0f}}, {116, {16.0f, 16.0f}}, {112.0f}, {});
}

UnitId World::create_unit(
    TransformComponent transform,
    RenderComponent render,
    VisionComponent vision,
    UnitComponent unit
) {
    const UnitId unit_id = static_cast<UnitId>(unit_ids_.size());
    unit_ids_.push_back(unit_id);
    transforms_.push_back(transform);
    renders_.push_back(render);
    visions_.push_back(vision);
    units_.push_back(unit);
    return unit_id;
}

void World::select_single(UnitId unit_id) {
    clear_selection();
    if (UnitComponent * unit = try_unit(unit_id)) {
        unit->selected = true;
        selected_units_.push_back(unit_id);
    }
}

void World::clear_selection() {
    for (UnitId unit_id : selected_units_) {
        if (UnitComponent * unit = try_unit(unit_id)) {
            unit->selected = false;
        }
    }
    selected_units_.clear();
}

TransformComponent * World::try_transform(UnitId unit_id) {
    const std::size_t index = to_index(unit_id);
    return index == static_cast<std::size_t>(kInvalidUnitId) ? nullptr : &transforms_[index];
}

const TransformComponent * World::try_transform(UnitId unit_id) const {
    const std::size_t index = to_index(unit_id);
    return index == static_cast<std::size_t>(kInvalidUnitId) ? nullptr : &transforms_[index];
}

RenderComponent * World::try_render(UnitId unit_id) {
    const std::size_t index = to_index(unit_id);
    return index == static_cast<std::size_t>(kInvalidUnitId) ? nullptr : &renders_[index];
}

const RenderComponent * World::try_render(UnitId unit_id) const {
    const std::size_t index = to_index(unit_id);
    return index == static_cast<std::size_t>(kInvalidUnitId) ? nullptr : &renders_[index];
}

VisionComponent * World::try_vision(UnitId unit_id) {
    const std::size_t index = to_index(unit_id);
    return index == static_cast<std::size_t>(kInvalidUnitId) ? nullptr : &visions_[index];
}

const VisionComponent * World::try_vision(UnitId unit_id) const {
    const std::size_t index = to_index(unit_id);
    return index == static_cast<std::size_t>(kInvalidUnitId) ? nullptr : &visions_[index];
}

UnitComponent * World::try_unit(UnitId unit_id) {
    const std::size_t index = to_index(unit_id);
    return index == static_cast<std::size_t>(kInvalidUnitId) ? nullptr : &units_[index];
}

const UnitComponent * World::try_unit(UnitId unit_id) const {
    const std::size_t index = to_index(unit_id);
    return index == static_cast<std::size_t>(kInvalidUnitId) ? nullptr : &units_[index];
}

std::size_t World::to_index(UnitId unit_id) const {
    if (unit_id >= unit_ids_.size()) {
        return static_cast<std::size_t>(kInvalidUnitId);
    }
    return static_cast<std::size_t>(unit_id);
}
