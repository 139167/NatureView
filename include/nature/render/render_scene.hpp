#pragma once

#include <cstdint>
#include <vector>

#include "nature/core/camera.hpp"
#include "nature/core/world.hpp"

namespace natureview {

struct Bounds3 {
    Vec3 minimum;
    Vec3 maximum;
};

struct RenderVertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
};

struct RenderMesh {
    std::vector<RenderVertex> vertices;
    std::vector<std::uint32_t> indices;
    Bounds3 bounds{};
};

struct TerrainChunk {
    std::uint32_t column = 0;
    std::uint32_t row = 0;
    RenderMesh mesh;
};

struct RenderScene {
    std::uint64_t worldFingerprint = 0;
    float worldSizeMeters = 0.0F;
    float chunkSizeMeters = 0.0F;
    std::uint32_t chunkColumns = 0;
    std::uint32_t chunkRows = 0;
    std::vector<TerrainChunk> terrainChunks;
    RenderMesh river;

    [[nodiscard]] std::uint64_t fingerprint() const;
};

struct RenderSceneSettings {
    float chunkSizeMeters = 500.0F;
    std::uint32_t verticesPerChunkSide = 33;
    float riverSliceSpacingMeters = 25.0F;
};

[[nodiscard]] RenderScene buildRenderScene(
    const WorldDefinition& world,
    RenderSceneSettings settings = {});

[[nodiscard]] bool isTerrainChunkVisible(
    const TerrainChunk& chunk,
    const CameraState& camera,
    float aspectRatio,
    float maximumDistanceMeters);

[[nodiscard]] std::vector<std::uint32_t> selectVisibleTerrainChunks(
    const RenderScene& scene,
    const CameraState& camera,
    float aspectRatio = 16.0F / 9.0F,
    float maximumDistanceMeters = 4500.0F);

}  // namespace natureview
