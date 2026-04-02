#include "game/collision_world.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

namespace {

constexpr float kParallelEpsilon = 0.0001f;

float cross(const Vec2f & lhs, const Vec2f & rhs) {
    return lhs.x * rhs.y - lhs.y * rhs.x;
}

CollisionBounds compute_bounds(const std::vector<Vec2f> & points) {
    CollisionBounds bounds{};
    bounds.min = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
    };
    bounds.max = {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
    };

    for (const Vec2f & point : points) {
        bounds.min.x = std::min(bounds.min.x, point.x);
        bounds.min.y = std::min(bounds.min.y, point.y);
        bounds.max.x = std::max(bounds.max.x, point.x);
        bounds.max.y = std::max(bounds.max.y, point.y);
    }

    return bounds;
}

std::vector<Vec2f> make_rectangle_points(const MapObject & object) {
    const Vec2f top_left = object.position;
    const Vec2f top_right = {object.position.x + object.size.x, object.position.y};
    const Vec2f bottom_right = {object.position.x + object.size.x, object.position.y - object.size.y};
    const Vec2f bottom_left = {object.position.x, object.position.y - object.size.y};
    return {
        top_left,
        top_right,
        bottom_right,
        bottom_left,
    };
}

bool point_in_bounds(const CollisionBounds & bounds, const Vec2f & point) {
    return point.x >= bounds.min.x && point.x <= bounds.max.x &&
           point.y >= bounds.min.y && point.y <= bounds.max.y;
}

bool point_in_polygon(const std::vector<Vec2f> & polygon, const Vec2f & point) {
    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const Vec2f & a = polygon[i];
        const Vec2f & b = polygon[j];
        const bool intersects = ((a.y > point.y) != (b.y > point.y)) &&
                                (point.x < (b.x - a.x) * (point.y - a.y) / ((b.y - a.y) + kParallelEpsilon) + a.x);
        if (intersects) {
            inside = !inside;
        }
    }
    return inside;
}

bool segments_intersect(const Vec2f & p, const Vec2f & p2, const Vec2f & q, const Vec2f & q2) {
    const Vec2f r = p2 - p;
    const Vec2f s = q2 - q;
    const float rxs = cross(r, s);
    const float q_pxr = cross(q - p, r);

    if (std::fabs(rxs) <= kParallelEpsilon && std::fabs(q_pxr) <= kParallelEpsilon) {
        const float rr = dot(r, r);
        if (rr <= kParallelEpsilon) {
            return length_squared(q - p) <= kParallelEpsilon;
        }

        const float t0 = dot(q - p, r) / rr;
        const float t1 = t0 + dot(s, r) / rr;
        const float min_t = std::min(t0, t1);
        const float max_t = std::max(t0, t1);
        return max_t >= 0.0f && min_t <= 1.0f;
    }

    if (std::fabs(rxs) <= kParallelEpsilon) {
        return false;
    }

    const float t = cross(q - p, s) / rxs;
    const float u = cross(q - p, r) / rxs;
    return t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f;
}

bool blocks_movement(const ObjectLayer & layer, const MapObject & object) {
    if (object.property_as_bool("engine.blocks_movement")) {
        return true;
    }

    const std::string_view object_semantic = object.property_value("engine.semantic");
    if (object_semantic == "collision" || object_semantic == "boundary") {
        return true;
    }

    if (layer.property_as_bool("engine.blocks_movement")) {
        return true;
    }

    const std::string_view layer_semantic = layer.property_value("engine.semantic");
    if (layer_semantic == "collision" || layer_semantic == "boundary") {
        return true;
    }

    return layer.name == "collision" || object.name == "collision" || object.type == "collision" ||
           layer.name == "boundary" || object.name == "boundary" || object.type == "boundary";
}

std::vector<Vec2f> build_collision_points(const MapObject & object) {
    if (object.shape == MapObjectShape::Polygon && object.polygon.points.size() >= 3) {
        return object.polygon.points;
    }
    if (object.shape == MapObjectShape::Rectangle && object.size.x > 0.0f && object.size.y > 0.0f) {
        return make_rectangle_points(object);
    }
    return {};
}

} // namespace

bool CollisionBounds::intersects_segment(const Vec2f & start, const Vec2f & end) const {
    const float min_x = std::min(start.x, end.x);
    const float min_y = std::min(start.y, end.y);
    const float max_x = std::max(start.x, end.x);
    const float max_y = std::max(start.y, end.y);
    return max_x >= min.x && min_x <= max.x && max_y >= min.y && min_y <= max.y;
}

bool CollisionBounds::intersects(const CollisionBounds & other) const {
    return max.x >= other.min.x && min.x <= other.max.x &&
           max.y >= other.min.y && min.y <= other.max.y;
}

CollisionWorld CollisionWorld::from_map(const MapWorld & map) {
    CollisionWorld collision_world{};

    for (const ObjectLayer & layer : map.object_layers()) {
        for (const MapObject & object : layer.objects) {
            if (!blocks_movement(layer, object)) {
                continue;
            }

            std::vector<Vec2f> points = build_collision_points(object);
            if (points.size() < 3) {
                continue;
            }

            CollisionShape shape{};
            shape.source_object_id = object.id;
            shape.source_layer_name = layer.name;
            shape.bounds = compute_bounds(points);
            shape.points = std::move(points);
            collision_world.shapes_.push_back(std::move(shape));
        }
    }

    return collision_world;
}

bool CollisionWorld::blocks_segment(std::span<const CollisionShape * const> candidates, const Vec2f & start, const Vec2f & end) const {
    for (const CollisionShape * shape_ptr : candidates) {
        if (!shape_ptr) {
            continue;
        }

        const CollisionShape & shape = *shape_ptr;
        if (!shape.bounds.intersects_segment(start, end) && !point_in_bounds(shape.bounds, end)) {
            continue;
        }

        if (point_in_polygon(shape.points, end)) {
            return true;
        }

        for (std::size_t i = 0; i < shape.points.size(); ++i) {
            const Vec2f & a = shape.points[i];
            const Vec2f & b = shape.points[(i + 1) % shape.points.size()];
            if (segments_intersect(start, end, a, b)) {
                return true;
            }
        }
    }

    return false;
}

bool CollisionWorld::blocks_segment(const Vec2f & start, const Vec2f & end) const {
    std::vector<const CollisionShape *> candidates;
    candidates.reserve(shapes_.size());
    for (const CollisionShape & shape : shapes_) {
        candidates.push_back(&shape);
    }
    return blocks_segment(candidates, start, end);
}

bool CollisionWorld::blocks_point(std::span<const CollisionShape * const> candidates, const Vec2f & point) const {
    for (const CollisionShape * shape_ptr : candidates) {
        if (!shape_ptr) {
            continue;
        }

        const CollisionShape & shape = *shape_ptr;
        if (!point_in_bounds(shape.bounds, point)) {
            continue;
        }
        if (point_in_polygon(shape.points, point)) {
            return true;
        }
    }

    return false;
}

bool CollisionWorld::blocks_point(const Vec2f & point) const {
    std::vector<const CollisionShape *> candidates;
    candidates.reserve(shapes_.size());
    for (const CollisionShape & shape : shapes_) {
        candidates.push_back(&shape);
    }
    return blocks_point(candidates, point);
}

std::vector<const CollisionShape *> CollisionWorld::query_bounds(const CollisionBounds & bounds) const {
    std::vector<const CollisionShape *> candidates;
    for (const CollisionShape & shape : shapes_) {
        if (shape.bounds.intersects(bounds)) {
            candidates.push_back(&shape);
        }
    }
    return candidates;
}
