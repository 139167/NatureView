#pragma once

#include <array>
#include <cstdint>

#include "nature/core/math.hpp"

namespace natureview {

class Noise2D {
public:
    explicit Noise2D(std::uint64_t seed);

    [[nodiscard]] float sample(float x, float z) const;
    [[nodiscard]] float fbm(
        float x,
        float z,
        int octaves = 5,
        float lacunarity = 2.03F,
        float gain = 0.5F) const;

private:
    [[nodiscard]] Vec2 gradient(int x, int z) const;

    std::array<std::uint8_t, 256> permutation_{};
    std::array<Vec2, 256> gradients_{};
};

}  // namespace natureview
