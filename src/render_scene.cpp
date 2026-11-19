#include "nature/render/render_scene.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "nature/core/math.hpp"

namespace natureview {
namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;

Vec3 terrainNormal(const WorldDefinition& world, float x, float z) {
    constexpr float sampleSpacing = 3.0F;
    const float heightLeft = world.sampleHeight(x - sampleSpacing, z);
    const float heightRight = world.sampleHeight(x + sampleSpacing, z);
    const float heightSouth = world.sampleHeight(x, z - sampleSpacing);
    const float heightNorth = world.sampleHeight(x, z + sampleSpacing);
    return normalize({
        heightLeft - heightRight,
        2.0F * sampleSpacing,
        heightSouth - heightNorth,
    });
}

void includePoint(Bounds3& bounds, const Vec3& point) {
    bounds.minimum.x = std::min(bounds.minimum.x, point.x);
    bounds.minimum.y = std::min(bounds.minimum.y, point.y);
    bounds.minimum.z = std::min(bounds.minimum.z, point.z);
    bounds.maximum.x = std::max(bounds.maximum.x, point.x);
    bounds.maximum.y = std::max(bounds.maximum.y, point.y);
    bounds.maximum.z = std::max(bounds.maximum.z, point.z);
}

Bounds3 emptyBounds() {
    constexpr float infinity = std::numeric_limits<float>::infinity();
    return {
        {infinity, infinity, infinity},
        {-infinity, -infinity, -infinity},
    };
}

void hashMesh(std::uint64_t& hash, const RenderMesh& mesh) {
    hashCombine(hash, static_cast<std::uint64_t>(mesh.vertices.size()));
    hashCombine(hash, static_cast<std::uint64_t>(mesh.indices.size()));
    for (const RenderVertex& vertex : mesh.vertices) {
        hashCombine(hash, vertex.position.x);
        hashCombine(hash, vertex.position.y);
        hashCombine(hash, vertex.position.z);
        hashCombine(hash, vertex.normal.x);
        hashCombine(hash, vertex.normal.y);
        hashCombine(hash, vertex.normal.z);
        hashCombine(hash, vertex.uv.x);
        hashCombine(hash, vertex.uv.z);
    }
    for (const std::uint32_t index : mesh.indices) {
        hashCombine(hash, static_cast<std::uint64_t>(index));
    }
}

float chunkRadius(const Bounds3& bounds) {
    return length(bounds.maximum - bounds.minimum) * 0.5F;
}

}  // namespace

std::uint64_t RenderScene::fingerprint() const {
    std::uint64_t hash = kFnvOffset;
    hashCombine(hash, worldFingerprint);
    hashCombine(hash, worldSizeMeters);
    hashCombine(hash, chunkSizeMeters);
    hashCombine(hash, static_cast<std::uint64_t>(chunkColumns));
    hashCombine(hash, static_cast<std::uint64_t>(chunkRows));
    for (const TerrainChunk& chunk : terrainChunks) {
        hashCombine(hash, static_cast<std::uint64_t>(chunk.column));
        hashCombine(hash, static_cast<std::uint64_t>(chunk.row));
        hashMesh(hash, chunk.mesh);
    }
    hashMesh(hash, river);
    return hash;
}

RenderScene buildRenderScene(
    const WorldDefinition& world,
    RenderSceneSettings settings) {
    if (world.heights.empty() || world.sizeMeters <= 0.0F) {
        throw std::invalid_argument("Cannot build a render scene from an empty world");
    }
    if (settings.chunkSizeMeters <= 0.0F ||
        settings.verticesPerChunkSide < 2 ||
        settings.verticesPerChunkSide > 129 ||
        settings.riverSliceSpacingMeters <= 0.0F) {
        throw std::invalid_argument("Invalid render scene settings");
    }

    const float chunkRatio = world.sizeMeters / settings.chunkSizeMeters;
    const auto chunkColumns = static_cast<std::uint32_t>(std::lround(chunkRatio));
    if (chunkColumns == 0 ||
        std::abs(chunkRatio - static_cast<float>(chunkColumns)) > 1.0e-4F) {
        throw std::invalid_argument("Chunk size must divide the world size");
    }

    RenderScene scene;
    scene.worldFingerprint = world.fingerprint;
    scene.worldSizeMeters = world.sizeMeters;
    scene.chunkSizeMeters = settings.chunkSizeMeters;
    scene.chunkColumns = chunkColumns;
    scene.chunkRows = chunkColumns;
    scene.terrainChunks.reserve(
        static_cast<std::size_t>(chunkColumns) * static_cast<std::size_t>(chunkColumns));

    const float halfSize = world.sizeMeters * 0.5F;
    const std::uint32_t side = settings.verticesPerChunkSide;
    std::vector<float> boundaries(static_cast<std::size_t>(chunkColumns) + 1);
    for (std::uint32_t index = 0; index <= chunkColumns; ++index) {
        boundaries[index] = -halfSize + static_cast<float>(index) * settings.chunkSizeMeters;
    }

    for (std::uint32_t row = 0; row < scene.chunkRows; ++row) {
        for (std::uint32_t column = 0; column < scene.chunkColumns; ++column) {
            TerrainChunk chunk;
            chunk.column = column;
            chunk.row = row;
            chunk.mesh.bounds = emptyBounds();
            chunk.mesh.vertices.reserve(static_cast<std::size_t>(side) * side);
            chunk.mesh.indices.reserve(
                static_cast<std::size_t>(side - 1) * (side - 1) * 6);

            const float startX = boundaries[column];
            const float endX = boundaries[column + 1];
            const float startZ = boundaries[row];
            const float endZ = boundaries[row + 1];

            for (std::uint32_t vertexRow = 0; vertexRow < side; ++vertexRow) {
                const float rowAmount =
                    static_cast<float>(vertexRow) / static_cast<float>(side - 1);
                const float z = vertexRow == 0
                                    ? startZ
                                    : (vertexRow + 1 == side
                                           ? endZ
                                           : lerp(startZ, endZ, rowAmount));

                for (std::uint32_t vertexColumn = 0; vertexColumn < side; ++vertexColumn) {
                    const float columnAmount =
                        static_cast<float>(vertexColumn) / static_cast<float>(side - 1);
                    const float x = vertexColumn == 0
                                        ? startX
                                        : (vertexColumn + 1 == side
                                               ? endX
                                               : lerp(startX, endX, columnAmount));
                    const Vec3 position{x, world.sampleHeight(x, z), z};
                    const Vec2 uv{
                        (x + halfSize) / world.sizeMeters,
                        (z + halfSize) / world.sizeMeters,
                    };
                    chunk.mesh.vertices.push_back({
                        position,
                        terrainNormal(world, x, z),
                        uv,
                    });
                    includePoint(chunk.mesh.bounds, position);
                }
            }

            for (std::uint32_t vertexRow = 0; vertexRow + 1 < side; ++vertexRow) {
                for (std::uint32_t vertexColumn = 0; vertexColumn + 1 < side;
                     ++vertexColumn) {
                    const std::uint32_t topLeft = vertexRow * side + vertexColumn;
                    const std::uint32_t bottomLeft = topLeft + side;
                    const std::uint32_t topRight = topLeft + 1;
                    const std::uint32_t bottomRight = bottomLeft + 1;
                    chunk.mesh.indices.insert(
                        chunk.mesh.indices.end(),
                        {
                            topLeft,
                            bottomLeft,
                            topRight,
                            topRight,
                            bottomLeft,
                            bottomRight,
                        });
                }
            }

            scene.terrainChunks.push_back(std::move(chunk));
        }
    }

    scene.river.bounds = emptyBounds();
    const std::uint32_t riverSliceCount = static_cast<std::uint32_t>(
        std::ceil(world.sizeMeters / settings.riverSliceSpacingMeters)) + 1;
    scene.river.vertices.reserve(static_cast<std::size_t>(riverSliceCount) * 2);
    scene.river.indices.reserve(
        static_cast<std::size_t>(riverSliceCount - 1) * 6);

    for (std::uint32_t slice = 0; slice < riverSliceCount; ++slice) {
        const float offset = std::min(
            static_cast<float>(slice) * settings.riverSliceSpacingMeters,
            world.sizeMeters);
        const float z = -halfSize + offset;
        const float centerX = world.riverXAt(z);
        const float halfWidth = world.riverHalfWidthAt(z);
        const float waterHeight = world.riverHeightAt(z);
        const float v = offset / world.sizeMeters;

        const Vec3 leftPosition = {centerX - halfWidth, waterHeight, z};
        const Vec3 rightPosition = {centerX + halfWidth, waterHeight, z};
        scene.river.vertices.push_back({
            leftPosition,
            {0.0F, 1.0F, 0.0F},
            {0.0F, v},
        });
        scene.river.vertices.push_back({
            rightPosition,
            {0.0F, 1.0F, 0.0F},
            {1.0F, v},
        });
        includePoint(scene.river.bounds, leftPosition);
        includePoint(scene.river.bounds, rightPosition);
    }

    for (std::uint32_t slice = 0; slice + 1 < riverSliceCount; ++slice) {
        const std::uint32_t first = slice * 2;
        const std::uint32_t second = first + 2;
        scene.river.indices.insert(
            scene.river.indices.end(),
            {
                first,
                second,
                first + 1,
                first + 1,
                second,
                second + 1,
            });
    }

    return scene;
}

bool isTerrainChunkVisible(
    const TerrainChunk& chunk,
    const CameraState& camera,
    float aspectRatio,
    float maximumDistanceMeters) {
    if (aspectRatio <= 0.0F || maximumDistanceMeters <= 0.0F) {
        return false;
    }

    const Vec3 center =
        (chunk.mesh.bounds.minimum + chunk.mesh.bounds.maximum) * 0.5F;
    const float radius = chunkRadius(chunk.mesh.bounds);
    const Vec3 toCenter = center - camera.position;
    const float centerDistance = length(toCenter);
    if (centerDistance <= radius) {
        return true;
    }

    const float distanceAlongView = dot(toCenter, camera.forward);
    if (distanceAlongView + radius < 0.0F ||
        distanceAlongView - radius > maximumDistanceMeters) {
        return false;
    }

    const float verticalHalfAngle =
        camera.verticalFovDegrees * 0.5F * kPi / 180.0F;
    const float horizontalHalfAngle =
        std::atan(std::tan(verticalHalfAngle) * aspectRatio);
    const float maximumHalfAngle = std::max(verticalHalfAngle, horizontalHalfAngle);
    const float centerAngle = std::acos(
        clamp(dot(normalize(toCenter), camera.forward), -1.0F, 1.0F));
    const float angularRadius =
        std::asin(clamp(radius / centerDistance, 0.0F, 1.0F));
    return centerAngle <= maximumHalfAngle + angularRadius;
}

std::vector<std::uint32_t> selectVisibleTerrainChunks(
    const RenderScene& scene,
    const CameraState& camera,
    float aspectRatio,
    float maximumDistanceMeters) {
    std::vector<std::uint32_t> visible;
    visible.reserve(scene.terrainChunks.size());

    for (std::uint32_t index = 0; index < scene.terrainChunks.size(); ++index) {
        if (isTerrainChunkVisible(
                scene.terrainChunks[index],
                camera,
                aspectRatio,
                maximumDistanceMeters)) {
            visible.push_back(index);
        }
    }
    return visible;
}

}  // namespace natureview
