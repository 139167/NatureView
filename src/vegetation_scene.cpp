#include "nature/render/vegetation_scene.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "nature/core/math.hpp"
#include "nature/core/noise.hpp"

namespace natureview {
namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr std::uint64_t kScatterSalt = 0x9f31c8d7a24b65e1ULL;

float unitHash(std::uint64_t value) {
    return static_cast<float>(mixBits(value) >> 40U) * (1.0F / 16777216.0F);
}

void appendTriangle(
    RenderMesh& mesh,
    const Vec3& a,
    const Vec3& b,
    const Vec3& c,
    float materialKind) {
    Vec3 normal = normalize(cross(b - a, c - a));
    if (normal.y < 0.0F) {
        normal = normal * -1.0F;
    }
    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({a, normal, {materialKind, 0.0F}});
    mesh.vertices.push_back({b, normal, {materialKind, 0.0F}});
    mesh.vertices.push_back({c, normal, {materialKind, 0.0F}});
    mesh.indices.push_back(base);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
}

RenderMesh createConiferPrototype() {
    RenderMesh mesh;
    constexpr int sides = 9;
    constexpr float trunkRadius = 0.075F;
    constexpr float trunkTop = 0.48F;

    for (int side = 0; side < sides; ++side) {
        const float angle0 =
            static_cast<float>(side) * 2.0F * std::numbers::pi_v<float> /
            static_cast<float>(sides);
        const float angle1 =
            static_cast<float>(side + 1) * 2.0F * std::numbers::pi_v<float> /
            static_cast<float>(sides);
        const Vec3 bottom0 = {
            std::cos(angle0) * trunkRadius,
            0.0F,
            std::sin(angle0) * trunkRadius,
        };
        const Vec3 bottom1 = {
            std::cos(angle1) * trunkRadius,
            0.0F,
            std::sin(angle1) * trunkRadius,
        };
        const Vec3 top0 = {bottom0.x, trunkTop, bottom0.z};
        const Vec3 top1 = {bottom1.x, trunkTop, bottom1.z};
        appendTriangle(mesh, bottom0, top1, bottom1, 0.0F);
        appendTriangle(mesh, bottom0, top0, top1, 0.0F);
    }

    struct CanopyTier {
        float base;
        float top;
        float radius;
    };
    constexpr CanopyTier tiers[] = {
        {0.22F, 0.43F, 1.00F},
        {0.34F, 0.55F, 0.84F},
        {0.46F, 0.68F, 0.68F},
        {0.58F, 0.82F, 0.50F},
        {0.70F, 1.00F, 0.30F},
    };
    for (const CanopyTier& tier : tiers) {
        for (int side = 0; side < sides; ++side) {
            const float angle0 =
                static_cast<float>(side) * 2.0F * std::numbers::pi_v<float> /
                static_cast<float>(sides);
            const float angle1 =
                static_cast<float>(side + 1) * 2.0F * std::numbers::pi_v<float> /
                static_cast<float>(sides);
            const Vec3 base0 = {
                std::cos(angle0) * tier.radius,
                tier.base,
                std::sin(angle0) * tier.radius,
            };
            const Vec3 base1 = {
                std::cos(angle1) * tier.radius,
                tier.base,
                std::sin(angle1) * tier.radius,
            };
            const Vec3 top = {0.0F, tier.top, 0.0F};
            appendTriangle(mesh, base0, top, base1, 1.0F);
        }
    }

    mesh.bounds = {
        {-1.0F, 0.0F, -1.0F},
        {1.0F, 1.0F, 1.0F},
    };
    return mesh;
}

void appendCanopyCluster(
    RenderMesh& mesh,
    const Vec3& center,
    const Vec3& radius) {
    const Vec3 x = {radius.x, 0.0F, 0.0F};
    const Vec3 y = {0.0F, radius.y, 0.0F};
    const Vec3 z = {0.0F, 0.0F, radius.z};
    const Vec3 positions[6] = {
        center + x,
        center - x,
        center + y,
        center - y,
        center + z,
        center - z,
    };
    const std::uint32_t faces[8][3] = {
        {0, 2, 4}, {4, 2, 1}, {1, 2, 5}, {5, 2, 0},
        {4, 3, 0}, {1, 3, 4}, {5, 3, 1}, {0, 3, 5},
    };
    for (const auto& face : faces) {
        appendTriangle(
            mesh,
            positions[face[0]],
            positions[face[1]],
            positions[face[2]],
            1.0F);
    }
}

void appendBranch(
    RenderMesh& mesh,
    const Vec3& begin,
    const Vec3& end,
    float width) {
    const Vec3 direction = normalize(end - begin);
    Vec3 side = cross(direction, {0.0F, 1.0F, 0.0F});
    if (lengthSquared(side) < 1.0e-5F) {
        side = {1.0F, 0.0F, 0.0F};
    }
    side = normalize(side) * width;
    const Vec3 other = normalize(cross(direction, side)) * width * 0.72F;
    const Vec3 beginA = begin + side;
    const Vec3 beginB = begin - side;
    const Vec3 endA = end + side * 0.55F;
    const Vec3 endB = end - side * 0.55F;
    const Vec3 beginC = begin + other;
    const Vec3 beginD = begin - other;
    const Vec3 endC = end + other * 0.55F;
    const Vec3 endD = end - other * 0.55F;
    appendTriangle(mesh, beginA, endA, beginB, 0.0F);
    appendTriangle(mesh, beginB, endA, endB, 0.0F);
    appendTriangle(mesh, beginC, beginD, endC, 0.0F);
    appendTriangle(mesh, beginD, endD, endC, 0.0F);
}

RenderMesh createBroadleafPrototype() {
    RenderMesh mesh;
    constexpr int sides = 7;
    constexpr float trunkRadius = 0.085F;
    constexpr float trunkTop = 0.62F;
    for (int side = 0; side < sides; ++side) {
        const float angle0 =
            static_cast<float>(side) * 2.0F * std::numbers::pi_v<float> /
            static_cast<float>(sides);
        const float angle1 =
            static_cast<float>(side + 1) * 2.0F * std::numbers::pi_v<float> /
            static_cast<float>(sides);
        const Vec3 bottom0 = {
            std::cos(angle0) * trunkRadius,
            0.0F,
            std::sin(angle0) * trunkRadius,
        };
        const Vec3 bottom1 = {
            std::cos(angle1) * trunkRadius,
            0.0F,
            std::sin(angle1) * trunkRadius,
        };
        const Vec3 top0 = {bottom0.x, trunkTop, bottom0.z};
        const Vec3 top1 = {bottom1.x, trunkTop, bottom1.z};
        appendTriangle(mesh, bottom0, top1, bottom1, 0.0F);
        appendTriangle(mesh, bottom0, top0, top1, 0.0F);
    }

    const Vec3 branchBase = {0.0F, 0.42F, 0.0F};
    const Vec3 trunkCenter = {0.0F, 0.72F, 0.0F};
    const Vec3 rightCluster = {0.34F, 0.61F, 0.06F};
    const Vec3 leftCluster = {-0.30F, 0.64F, -0.14F};
    const Vec3 backCluster = {0.02F, 0.67F, -0.34F};
    appendBranch(mesh, branchBase, trunkCenter, 0.035F);
    appendBranch(mesh, branchBase, rightCluster, 0.030F);
    appendBranch(mesh, branchBase, leftCluster, 0.030F);
    appendBranch(mesh, {0.0F, 0.48F, 0.0F}, backCluster, 0.026F);
    appendCanopyCluster(mesh, trunkCenter, {0.72F, 0.30F, 0.72F});
    appendCanopyCluster(mesh, rightCluster, {0.48F, 0.25F, 0.48F});
    appendCanopyCluster(mesh, leftCluster, {0.45F, 0.24F, 0.45F});
    appendCanopyCluster(mesh, backCluster, {0.38F, 0.22F, 0.38F});
    appendCanopyCluster(mesh, {0.12F, 0.88F, 0.08F}, {0.34F, 0.19F, 0.34F});
    mesh.bounds = {
        {-1.0F, 0.0F, -1.0F},
        {1.0F, 1.0F, 1.0F},
    };
    return mesh;
}

float slopeNormalY(const WorldDefinition& world, float x, float z) {
    constexpr float offset = 12.0F;
    const float dx =
        (world.sampleHeight(x + offset, z) - world.sampleHeight(x - offset, z)) /
        (2.0F * offset);
    const float dz =
        (world.sampleHeight(x, z + offset) - world.sampleHeight(x, z - offset)) /
        (2.0F * offset);
    return 1.0F / std::sqrt(1.0F + dx * dx + dz * dz);
}

}  // namespace

VegetationScene buildVegetationScene(
    const WorldDefinition& world,
    VegetationSettings settings) {
    VegetationScene scene;
    scene.prototypes[0] = createConiferPrototype();
    scene.prototypes[1] = createBroadleafPrototype();
    if (world.heights.empty() || world.sizeMeters <= 0.0F ||
        settings.spacingMeters < 10.0F) {
        return scene;
    }

    const int gridSize = static_cast<int>(
        std::ceil(world.sizeMeters / settings.spacingMeters));
    const float halfSize = world.sizeMeters * 0.5F;
    const Noise2D forestNoise(world.seed ^ 0x8b2f71c9d451e306ULL);
    scene.instances.reserve(
        static_cast<std::size_t>(gridSize) * static_cast<std::size_t>(gridSize) / 2);

    for (int gridZ = 0; gridZ < gridSize; ++gridZ) {
        for (int gridX = 0; gridX < gridSize; ++gridX) {
            const std::uint64_t cellSeed =
                mixBits(world.seed ^ kScatterSalt ^
                        (static_cast<std::uint64_t>(gridX) << 32U) ^
                        static_cast<std::uint64_t>(gridZ));
            const float jitterX = unitHash(cellSeed ^ 0x11ULL) - 0.5F;
            const float jitterZ = unitHash(cellSeed ^ 0x22ULL) - 0.5F;
            const float x =
                -halfSize +
                (static_cast<float>(gridX) + 0.5F + jitterX * 0.9F) *
                    settings.spacingMeters;
            const float z =
                -halfSize +
                (static_cast<float>(gridZ) + 0.5F + jitterZ * 0.9F) *
                    settings.spacingMeters;
            if (std::abs(x) >= halfSize || std::abs(z) >= halfSize) {
                continue;
            }

            const float height = world.sampleHeight(x, z);
            if (height > settings.treeLineMeters || height < 8.0F) {
                continue;
            }
            const float riverDistance = std::abs(x - world.riverXAt(z));
            if (riverDistance < world.riverHalfWidthAt(z) + 32.0F) {
                continue;
            }
            const float normalY = slopeNormalY(world, x, z);
            if (normalY < settings.minimumSlopeNormalY) {
                continue;
            }

            const float altitudeDensity =
                1.0F - smoothstep(720.0F, settings.treeLineMeters, height);
            const float slopeDensity = smoothstep(
                settings.minimumSlopeNormalY,
                0.72F,
                normalY);
            const float forestField =
                forestNoise.fbm(x * 0.0021F, z * 0.0021F, 4);
            const float standDensity =
                smoothstep(-0.12F, 0.24F, forestField);
            const float density =
                0.96F * altitudeDensity *
                (0.60F + slopeDensity * 0.40F) *
                (0.28F + standDensity * 0.96F);
            if (unitHash(cellSeed ^ 0x33ULL) > density) {
                continue;
            }

            const bool broadleaf =
                height < 700.0F &&
                unitHash(cellSeed ^ 0x99ULL) < 0.34F;
            scene.instances.push_back({
                {x, height, z},
                5.8F + unitHash(cellSeed ^ 0x44ULL) * 8.2F,
                1.15F + unitHash(cellSeed ^ 0x55ULL) * 1.10F,
                unitHash(cellSeed ^ 0x66ULL) * 2.0F * std::numbers::pi_v<float>,
                unitHash(cellSeed ^ 0x77ULL) * 2.0F * std::numbers::pi_v<float>,
                unitHash(cellSeed ^ 0x88ULL),
                broadleaf ? 1U : 0U,
            });
        }
    }

    std::uint64_t fingerprint = kFnvOffset;
    hashCombine(fingerprint, world.fingerprint);
    hashCombine(fingerprint, settings.spacingMeters);
    hashCombine(fingerprint, static_cast<std::uint64_t>(scene.instances.size()));
    for (const TreeInstance& tree : scene.instances) {
        hashCombine(fingerprint, tree.position.x);
        hashCombine(fingerprint, tree.position.y);
        hashCombine(fingerprint, tree.position.z);
        hashCombine(fingerprint, tree.heightMeters);
        hashCombine(fingerprint, tree.canopyRadiusMeters);
        hashCombine(fingerprint, tree.yawRadians);
        hashCombine(fingerprint, tree.windPhase);
        hashCombine(fingerprint, tree.colorVariant);
        hashCombine(fingerprint, static_cast<std::uint64_t>(tree.prototypeIndex));
    }
    scene.fingerprint = fingerprint;
    return scene;
}

}  // namespace natureview
