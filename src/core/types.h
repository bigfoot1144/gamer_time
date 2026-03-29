#pragma once

#include <cmath>

struct Vec2f {
    float x = 0.0f;
    float y = 0.0f;
};

inline Vec2f operator+(const Vec2f & lhs, const Vec2f & rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y};
}

inline Vec2f operator-(const Vec2f & lhs, const Vec2f & rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y};
}

inline Vec2f operator*(const Vec2f & value, float scalar) {
    return {value.x * scalar, value.y * scalar};
}

inline Vec2f operator/(const Vec2f & value, float scalar) {
    return {value.x / scalar, value.y / scalar};
}

inline Vec2f & operator+=(Vec2f & lhs, const Vec2f & rhs) {
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    return lhs;
}

inline float dot(const Vec2f & lhs, const Vec2f & rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

inline float length_squared(const Vec2f & value) {
    return dot(value, value);
}

inline float length(const Vec2f & value) {
    return std::sqrt(length_squared(value));
}

inline Vec2f normalize_or_zero(const Vec2f & value) {
    const float value_length = length(value);
    if (value_length <= 0.0001f) {
        return {};
    }
    return value / value_length;
}
