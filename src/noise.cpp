#include "nature/core/noise.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace natureview {
namespace {

float fade(float value) {
    return value * value * value * (value * (value * 6.0F - 15.0F) + 10.0F);
}

float dotGradient(const Vec2& gradient, float x, float z) {
    return gradient.x * x + gradient.z * z;
}

}  // namespace

Noise2D::Noise2D(std::uint64_t seed) {
    std::iota(permutation_.begin(), permutation_.end(), std::uint8_t{0});

    std::uint64_t state = mixBits(seed);
    for (int index = 255; index > 0; --index) {
        state = mixBits(state);
        const int swapIndex = static_cast<int>(state % static_cast<std::uint64_t>(index + 1));
        std::swap(permutation_[static_cast<std::size_t>(index)],
                  permutation_[static_cast<std::size_t>(swapIndex)]);
    }

    for (std::size_t index = 0; index < gradients_.size(); ++index) {
        const float angle = static_cast<float>(index) * 2.399963229728653F;
        gradients_[index] = {std::cos(angle), std::sin(angle)};
    }
}

Vec2 Noise2D::gradient(int x, int z) const {
    const auto permutedX = permutation_[static_cast<std::size_t>(x & 255)];
    const auto permutedZ = permutation_[static_cast<std::size_t>((permutedX + z) & 255)];
    return gradients_[permutedZ];
}

float Noise2D::sample(float x, float z) const {
    const float floorX = std::floor(x);
    const float floorZ = std::floor(z);
    const int x0 = static_cast<int>(floorX);
    const int z0 = static_cast<int>(floorZ);

    const float localX = x - floorX;
    const float localZ = z - floorZ;
    const float weightX = fade(localX);
    const float weightZ = fade(localZ);

    const float n00 = dotGradient(gradient(x0, z0), localX, localZ);
    const float n10 = dotGradient(gradient(x0 + 1, z0), localX - 1.0F, localZ);
    const float n01 = dotGradient(gradient(x0, z0 + 1), localX, localZ - 1.0F);
    const float n11 = dotGradient(gradient(x0 + 1, z0 + 1), localX - 1.0F, localZ - 1.0F);

    return lerp(lerp(n00, n10, weightX), lerp(n01, n11, weightX), weightZ);
}

float Noise2D::fbm(float x, float z, int octaves, float lacunarity, float gain) const {
    float value = 0.0F;
    float amplitude = 0.5F;
    float frequency = 1.0F;

    for (int octave = 0; octave < octaves; ++octave) {
        value += sample(x * frequency, z * frequency) * amplitude;
        frequency *= lacunarity;
        amplitude *= gain;
    }

    return value;
}

}  // namespace natureview
