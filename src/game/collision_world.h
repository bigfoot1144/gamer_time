#pragma once

#include "core/types.h"
#include "game/map_world.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

struct CollisionBounds {
    Vec2f min{};
    Vec2f max{};

    bool intersects_segment(const Vec2f & start, const Vec2f & end) const;
    bool intersects(const CollisionBounds & other) const;
};

struct CollisionShape {
    std::uint32_t source_object_id = 0;
    std::string source_layer_name;
    CollisionBounds bounds{};
    std::vector<Vec2f> points;
};

class CollisionWorld {
public:
    CollisionWorld() = default;

    static CollisionWorld from_map(const MapWorld & map);

    bool blocks_segment(const Vec2f & start, const Vec2f & end) const;
    bool blocks_point(const Vec2f & point) const;
    bool blocks_segment(std::span<const CollisionShape * const> candidates, const Vec2f & start, const Vec2f & end) const;
    bool blocks_point(std::span<const CollisionShape * const> candidates, const Vec2f & point) const;
    std::vector<const CollisionShape *> query_bounds(const CollisionBounds & bounds) const;
    std::size_t polygon_count() const { return shapes_.size(); }
    const std::vector<CollisionShape> & shapes() const { return shapes_; }

private:
    std::vector<CollisionShape> shapes_;
};
