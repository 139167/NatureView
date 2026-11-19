#include "nature/core/world.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "nature/core/noise.hpp"

namespace natureview {
namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr float kWorldSizeMeters = 6000.0F;
constexpr std::uint32_t kTerrainResolution = 257;
constexpr std::uint32_t kRiverSampleCount = 513;

float seedUnit(std::uint64_t seed, std::uint64_t salt) {
    const std::uint64_t value = mixBits(seed ^ salt);
    return static_cast<float>(value >> 40U) * (1.0F / 16777216.0F);
}

struct RiverGeometry {
    float x = 0.0F;
    float waterHeight = 0.0F;
};

class RiverRule {
public:
    explicit RiverRule(std::uint64_t seed)
        : phaseA_(seedUnit(seed, 0x41a7c93d2e5f886bULL) * 2.0F * kPi),
          phaseB_(seedUnit(seed, 0x8f13d9276bb41ac5ULL) * 2.0F * kPi),
          phaseC_(seedUnit(seed, 0xcf364e17a9b80521ULL) * 2.0F * kPi) {}

    [[nodiscard]] RiverGeometry at(float z) const {
        const float normalizedZ = z / (kWorldSizeMeters * 0.5F);
        const float x =
            150.0F * std::sin(normalizedZ * 1.7F + phaseA_) +
            58.0F * std::sin(normalizedZ * 3.9F + phaseB_);
        const float waterHeight =
            18.0F + normalizedZ * 4.0F +
            1.4F * std::sin(z * 0.0027F + phaseC_);
        return {x, waterHeight};
    }

private:
    float phaseA_;
    float phaseB_;
    float phaseC_;
};

}  // namespace

float WorldDefinition::sampleHeight(float x, float z) const {
    if (heights.empty() || resolution < 2 || sizeMeters <= 0.0F) {
        return 0.0F;
    }

    const float halfSize = sizeMeters * 0.5F;
    const float normalizedX = clamp((x + halfSize) / sizeMeters, 0.0F, 1.0F);
    const float normalizedZ = clamp((z + halfSize) / sizeMeters, 0.0F, 1.0F);
    const float gridX = normalizedX * static_cast<float>(resolution - 1);
    const float gridZ = normalizedZ * static_cast<float>(resolution - 1);

    const auto x0 = static_cast<std::uint32_t>(gridX);
    const auto z0 = static_cast<std::uint32_t>(gridZ);
    const auto x1 = std::min(x0 + 1, resolution - 1);
    const auto z1 = std::min(z0 + 1, resolution - 1);
    const float weightX = gridX - static_cast<float>(x0);
    const float weightZ = gridZ - static_cast<float>(z0);

    const auto heightAt = [this](std::uint32_t px, std::uint32_t pz) {
        return heights[static_cast<std::size_t>(pz) * resolution + px];
    };

    const float south = lerp(heightAt(x0, z0), heightAt(x1, z0), weightX);
    const float north = lerp(heightAt(x0, z1), heightAt(x1, z1), weightX);
    return lerp(south, north, weightZ);
}

float WorldDefinition::riverXAt(float z) const {
    if (river.empty()) {
        return 0.0F;
    }

    const float normalized = clamp((z + sizeMeters * 0.5F) / sizeMeters, 0.0F, 1.0F);
    const float position = normalized * static_cast<float>(river.size() - 1);
    const auto index = static_cast<std::size_t>(position);
    const auto nextIndex = std::min(index + 1, river.size() - 1);
    return lerp(river[index].center.x, river[nextIndex].center.x,
                position - static_cast<float>(index));
}

float WorldDefinition::riverHeightAt(float z) const {
    if (river.empty()) {
        return 0.0F;
    }

    const float normalized = clamp((z + sizeMeters * 0.5F) / sizeMeters, 0.0F, 1.0F);
    const float position = normalized * static_cast<float>(river.size() - 1);
    const auto index = static_cast<std::size_t>(position);
    const auto nextIndex = std::min(index + 1, river.size() - 1);
    return lerp(river[index].waterHeight, river[nextIndex].waterHeight,
                position - static_cast<float>(index));
}

float WorldDefinition::riverHalfWidthAt(float z) const {
    return 22.0F + 4.0F * std::sin(z * 0.0061F);
}

WorldDefinition generateHeroWorld(std::uint64_t seed) {
    WorldDefinition world;
    world.seed = seed;
    world.sizeMeters = kWorldSizeMeters;
    world.latitudeDegrees = 64.0F;
    world.resolution = kTerrainResolution;
    world.heights.resize(static_cast<std::size_t>(kTerrainResolution) * kTerrainResolution);
    world.river.resize(kRiverSampleCount);

    const RiverRule riverRule(seed);
    const Noise2D elevationNoise(seed ^ 0x73b7c91a5e4d820fULL);
    const Noise2D ridgeNoise(seed ^ 0xe1694d2a8c5073b1ULL);
    const Noise2D detailNoise(seed ^ 0x1c8a6e93d725b40fULL);
    const float halfSize = kWorldSizeMeters * 0.5F;
    const float cellSize = kWorldSizeMeters / static_cast<float>(kTerrainResolution - 1);

    float waterLevelSum = 0.0F;
    for (std::size_t index = 0; index < world.river.size(); ++index) {
        const float amount = static_cast<float>(index) /
                             static_cast<float>(world.river.size() - 1);
        const float z = -halfSize + amount * kWorldSizeMeters;
        const RiverGeometry geometry = riverRule.at(z);
        world.river[index] = {{geometry.x, z}, geometry.waterHeight};
        waterLevelSum += geometry.waterHeight;
    }
    world.waterLevelMeters = waterLevelSum / static_cast<float>(world.river.size());

    for (std::uint32_t row = 0; row < kTerrainResolution; ++row) {
        const float z = -halfSize + static_cast<float>(row) * cellSize;
        const RiverGeometry river = riverRule.at(z);

        for (std::uint32_t column = 0; column < kTerrainResolution; ++column) {
            const float x = -halfSize + static_cast<float>(column) * cellSize;
            const float distanceToRiver = std::abs(x - river.x);
            const float valley = smoothstep(45.0F, 1850.0F, distanceToRiver);
            const float broadElevation = elevationNoise.fbm(x * 0.00048F, z * 0.00048F, 5);
            const float ridge =
                1.0F - std::abs(ridgeNoise.fbm(x * 0.00082F, z * 0.00082F, 4));
            const float detail =
                detailNoise.fbm(x * 0.0036F, z * 0.0036F, 4) * 32.0F +
                detailNoise.fbm(x * 0.016F + 31.0F, z * 0.016F - 17.0F, 3) * 2.2F;

            const float mountain = std::pow(valley, 1.28F) * 760.0F;
            const float ridgeline = std::pow(valley, 2.15F) * ridge * 760.0F;
            const float channel = -3.5F * (1.0F - smoothstep(20.0F, 72.0F, distanceToRiver));
            const float broadVariation = broadElevation * 80.0F * (0.12F + 0.88F * valley);
            const float localVariation = detail * (0.08F + 0.92F * valley);

            const auto index = static_cast<std::size_t>(row) * kTerrainResolution + column;
            world.heights[index] =
                river.waterHeight + channel + mountain + ridgeline + broadVariation +
                localVariation;
        }
    }

    std::uint64_t fingerprint = kFnvOffset;
    hashCombine(fingerprint, seed);
    hashCombine(fingerprint, world.latitudeDegrees);
    hashCombine(fingerprint, static_cast<std::uint64_t>(kTerrainResolution));
    for (const float height : world.heights) {
        hashCombine(fingerprint, height);
    }
    for (const RiverSample& sample : world.river) {
        hashCombine(fingerprint, sample.center.x);
        hashCombine(fingerprint, sample.center.z);
        hashCombine(fingerprint, sample.waterHeight);
    }
    world.fingerprint = fingerprint;

    return world;
}

}  // namespace natureview
