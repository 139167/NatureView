#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace natureview {

constexpr float kPi = 3.14159265358979323846F;

struct Vec2 {
    float x = 0.0F;
    float z = 0.0F;
};

struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

inline Vec3 operator+(const Vec3& lhs, const Vec3& rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

inline Vec3 operator-(const Vec3& lhs, const Vec3& rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

inline Vec3 operator*(const Vec3& value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

inline float dot(const Vec3& lhs, const Vec3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

inline float lengthSquared(const Vec3& value) {
    return dot(value, value);
}

inline float length(const Vec3& value) {
    return std::sqrt(lengthSquared(value));
}

inline Vec3 normalize(const Vec3& value) {
    const float squaredLength = lengthSquared(value);
    if (squaredLength <= 1.0e-12F) {
        return {0.0F, 0.0F, 1.0F};
    }
    return value * (1.0F / std::sqrt(squaredLength));
}

inline float clamp(float value, float minimum, float maximum) {
    return std::max(minimum, std::min(maximum, value));
}

inline float lerp(float begin, float end, float amount) {
    return begin + (end - begin) * amount;
}

inline float smoothstep(float edge0, float edge1, float value) {
    if (edge0 == edge1) {
        return value < edge0 ? 0.0F : 1.0F;
    }
    const float amount = clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
    return amount * amount * (3.0F - 2.0F * amount);
}

inline std::uint64_t mixBits(std::uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

inline void hashCombine(std::uint64_t& hash, std::uint64_t value) {
    hash ^= value;
    hash *= 0x100000001b3ULL;
}

inline void hashCombine(std::uint64_t& hash, float value) {
    hashCombine(hash, static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(value)));
}

}  // namespace natureview
