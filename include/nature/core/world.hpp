#pragma once

#include <cstdint>
#include <vector>

#include "nature/core/math.hpp"

namespace natureview {

struct RiverSample {
    Vec2 center;
    float waterHeight = 0.0F;
};

struct WorldDefinition {
    std::uint64_t seed = 0;
    float sizeMeters = 0.0F;
    float latitudeDegrees = 64.0F;
    std::uint32_t resolution = 0;
    float waterLevelMeters = 0.0F;
    std::vector<float> heights;
    std::vector<RiverSample> river;
    std::uint64_t fingerprint = 0;

    [[nodiscard]] float sampleHeight(float x, float z) const;
    [[nodiscard]] float riverXAt(float z) const;
    [[nodiscard]] float riverHeightAt(float z) const;
    [[nodiscard]] float riverHalfWidthAt(float z) const;
};

[[nodiscard]] WorldDefinition generateHeroWorld(std::uint64_t seed);

}  // namespace natureview
