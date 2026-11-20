#pragma once

#include <cstdint>
#include <vector>

#include "nature/core/world.hpp"
#include "nature/render/render_scene.hpp"

namespace natureview {

struct TreeInstance {
    Vec3 position;
    float heightMeters = 0.0F;
    float canopyRadiusMeters = 0.0F;
    float yawRadians = 0.0F;
    float windPhase = 0.0F;
    float colorVariant = 0.0F;
};

struct VegetationSettings {
    float spacingMeters = 38.0F;
    float treeLineMeters = 1020.0F;
    float minimumSlopeNormalY = 0.38F;
};

struct VegetationScene {
    RenderMesh prototype;
    std::vector<TreeInstance> instances;
    std::uint64_t fingerprint = 0;
};

[[nodiscard]] VegetationScene buildVegetationScene(
    const WorldDefinition& world,
    VegetationSettings settings = {});

}  // namespace natureview
