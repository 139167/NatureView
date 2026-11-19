#include "nature/render/preview_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>

#include "nature/core/math.hpp"

namespace natureview {
namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;

Vec3 addScaled(const Vec3& origin, const Vec3& direction, float distance) {
    return origin + direction * distance;
}

float riverHalfWidth(float z) {
    return 22.0F + 4.0F * std::sin(z * 0.0061F);
}

Vec3 terrainNormal(const WorldDefinition& world, const Vec3& point) {
    constexpr float sampleSpacing = 3.0F;
    const float heightLeft = world.sampleHeight(point.x - sampleSpacing, point.z);
    const float heightRight = world.sampleHeight(point.x + sampleSpacing, point.z);
    const float heightSouth = world.sampleHeight(point.x, point.z - sampleSpacing);
    const float heightNorth = world.sampleHeight(point.x, point.z + sampleSpacing);
    return normalize({
        heightLeft - heightRight,
        2.0F * sampleSpacing,
        heightSouth - heightNorth,
    });
}

Vec3 sunDirection(const EnvironmentState& environment) {
    const float elevation = environment.sunElevationDegrees * kPi / 180.0F;
    const float horizontal = std::cos(elevation);
    return normalize({
        horizontal * std::cos(environment.sunAzimuthRadians),
        std::sin(elevation),
        horizontal * std::sin(environment.sunAzimuthRadians),
    });
}

Vec3 mixColor(const Vec3& begin, const Vec3& end, float amount) {
    const float clampedAmount = clamp(amount, 0.0F, 1.0F);
    return begin * (1.0F - clampedAmount) + end * clampedAmount;
}

Vec3 skyColor(
    const EnvironmentState& environment,
    const Vec3& direction) {
    const float elevation = clamp(direction.y, -1.0F, 1.0F);
    const float daylight = smoothstep(-4.0F, 18.0F, environment.sunElevationDegrees);
    const Vec3 night = {0.012F, 0.022F, 0.045F};
    const Vec3 horizon = {0.58F, 0.67F, 0.72F};
    const Vec3 zenith = {0.16F, 0.32F, 0.56F};

    const float vertical = smoothstep(-0.08F, 0.65F, elevation);
    Vec3 sky = mixColor(horizon, zenith, vertical);
    sky = mixColor(night, sky, daylight);

    const float cloudShade = 1.0F - environment.cloudCover * 0.28F;
    sky = sky * cloudShade;

    const float sunAmount = std::max(0.0F, dot(direction, sunDirection(environment)));
    const float glow = std::pow(sunAmount, 12.0F) * 0.22F * daylight;
    const float disk = std::pow(sunAmount, 900.0F) * 2.4F * daylight;
    return sky + Vec3{1.0F, 0.72F, 0.38F} * (glow + disk);
}

Vec3 terrainColor(
    const EnvironmentState& environment,
    const Vec3& point,
    const Vec3& normal) {
    const float rockAmount = smoothstep(0.22F, 0.62F, 1.0F - normal.y);
    const float altitudeRock = smoothstep(520.0F, 920.0F, point.y);
    const float rock = clamp(rockAmount + altitudeRock, 0.0F, 1.0F);

    const float moistureVariation =
        0.5F + 0.5F * std::sin(point.x * 0.011F + point.z * 0.017F);
    const Vec3 vegetation =
        mixColor({0.075F, 0.13F, 0.055F}, {0.13F, 0.19F, 0.075F}, moistureVariation);
    const Vec3 stone = {0.23F, 0.22F, 0.19F};
    Vec3 color = mixColor(vegetation, stone, rock);

    const float snow =
        smoothstep(1060.0F, 1220.0F, point.y) * smoothstep(0.35F, 0.72F, normal.y);
    color = mixColor(color, {0.78F, 0.82F, 0.84F}, snow);

    const Vec3 lightDirection = sunDirection(environment);
    const float sunUp = clamp(lightDirection.y * 3.0F + 0.08F, 0.06F, 1.0F);
    const float diffuse = std::max(0.0F, dot(normal, lightDirection));
    const float skyLight = 0.18F + 0.24F * clamp(normal.y, 0.0F, 1.0F);
    return color * (skyLight + diffuse * 0.92F * sunUp);
}

Vec3 waterColor(
    const WorldDefinition& world,
    const EnvironmentState& environment,
    const Vec3& point,
    const Vec3& viewDirection) {
    const float waveA = 0.035F * std::sin(point.x * 0.075F + point.z * 0.052F);
    const float waveB = 0.025F * std::sin(point.z * 0.13F - point.x * 0.031F);
    const Vec3 normal = normalize({waveA, 1.0F, waveB});
    const Vec3 reflected = viewDirection - normal * (2.0F * dot(viewDirection, normal));
    const Vec3 reflection = skyColor(environment, reflected);
    const Vec3 deepWater = {0.025F, 0.105F, 0.125F};
    const float fresnel =
        std::pow(1.0F - clamp(std::abs(dot(viewDirection, normal)), 0.0F, 1.0F), 3.0F);
    const float sparkle =
        std::pow(std::max(0.0F, dot(reflected, sunDirection(environment))), 180.0F);

    const float channelWidth = std::max(1.0F, riverHalfWidth(point.z));
    const float edgeFoam =
        smoothstep(0.72F, 1.0F, std::abs(point.x - world.riverXAt(point.z)) / channelWidth);
    Vec3 color = mixColor(deepWater, reflection, 0.30F + fresnel * 0.62F);
    color = color + Vec3{1.0F, 0.92F, 0.78F} * (sparkle * 0.7F);
    return mixColor(color, {0.58F, 0.64F, 0.63F}, edgeFoam * 0.18F);
}

Rgb8 toDisplayColor(const Vec3& linearColor) {
    const auto encode = [](float value) {
        const float corrected = std::pow(clamp(value, 0.0F, 1.0F), 1.0F / 2.2F);
        return static_cast<std::uint8_t>(std::lround(corrected * 255.0F));
    };
    return {encode(linearColor.x), encode(linearColor.y), encode(linearColor.z)};
}

}  // namespace

std::uint64_t PreviewImage::fingerprint() const {
    std::uint64_t hash = kFnvOffset;
    hashCombine(hash, static_cast<std::uint64_t>(width));
    hashCombine(hash, static_cast<std::uint64_t>(height));
    for (const Rgb8& pixel : pixels) {
        hashCombine(hash, static_cast<std::uint64_t>(pixel.red));
        hashCombine(hash, static_cast<std::uint64_t>(pixel.green));
        hashCombine(hash, static_cast<std::uint64_t>(pixel.blue));
    }
    return hash;
}

PreviewImage SoftwarePreviewRenderer::render(
    const NatureSimulation& simulation,
    PreviewSettings settings) const {
    settings.width = std::clamp(settings.width, 64, 3840);
    settings.height = std::clamp(settings.height, 64, 2160);
    settings.maximumDistanceMeters =
        std::clamp(settings.maximumDistanceMeters, 500.0F, 5500.0F);

    PreviewImage image;
    image.width = settings.width;
    image.height = settings.height;
    image.pixels.resize(
        static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height));

    const CameraState& camera = simulation.camera();
    const EnvironmentState& environment = simulation.environment();
    const WorldDefinition& world = simulation.world();
    const Vec3 worldUp = {0.0F, 1.0F, 0.0F};
    const Vec3 right = normalize(cross(worldUp, camera.forward));
    const Vec3 up = normalize(cross(camera.forward, right));
    const float aspect =
        static_cast<float>(image.width) / static_cast<float>(image.height);
    const float tangent =
        std::tan(camera.verticalFovDegrees * 0.5F * kPi / 180.0F);

    for (int y = 0; y < image.height; ++y) {
        const float screenY =
            (1.0F - 2.0F * (static_cast<float>(y) + 0.5F) / static_cast<float>(image.height)) *
            tangent;

        for (int x = 0; x < image.width; ++x) {
            const float screenX =
                (2.0F * (static_cast<float>(x) + 0.5F) / static_cast<float>(image.width) - 1.0F) *
                aspect * tangent;
            const Vec3 rayDirection =
                normalize(camera.forward + right * screenX + up * screenY);

            Vec3 color = skyColor(environment, rayDirection);
            float distance = 1.0F;
            float previousDistance = distance;
            Vec3 hitPoint = {};
            bool hitWater = false;
            bool hitTerrain = false;

            for (int step = 0; step < 700 && distance < settings.maximumDistanceMeters;
                 ++step) {
                const Vec3 point = addScaled(camera.position, rayDirection, distance);
                const float terrainHeight = world.sampleHeight(point.x, point.z);
                const float waterHeight = world.riverHeightAt(point.z);
                const float distanceToRiver = std::abs(point.x - world.riverXAt(point.z));
                const bool overRiver =
                    distanceToRiver <= riverHalfWidth(point.z) &&
                    point.y <= waterHeight;
                const bool belowTerrain = point.y <= terrainHeight;

                if (overRiver || belowTerrain) {
                    float low = previousDistance;
                    float high = distance;
                    for (int refinement = 0; refinement < 7; ++refinement) {
                        const float middle = (low + high) * 0.5F;
                        const Vec3 middlePoint =
                            addScaled(camera.position, rayDirection, middle);
                        const float middleTerrain =
                            world.sampleHeight(middlePoint.x, middlePoint.z);
                        const float middleWater = world.riverHeightAt(middlePoint.z);
                        const bool middleOverRiver =
                            std::abs(middlePoint.x - world.riverXAt(middlePoint.z)) <=
                                riverHalfWidth(middlePoint.z) &&
                            middlePoint.y <= middleWater;
                        if (middleOverRiver || middlePoint.y <= middleTerrain) {
                            high = middle;
                        } else {
                            low = middle;
                        }
                    }

                    distance = high;
                    hitPoint = addScaled(camera.position, rayDirection, distance);
                    const float finalWaterHeight = world.riverHeightAt(hitPoint.z);
                    const bool finalOverRiver =
                        std::abs(hitPoint.x - world.riverXAt(hitPoint.z)) <=
                            riverHalfWidth(hitPoint.z) &&
                        hitPoint.y <= finalWaterHeight + 0.08F;
                    const float finalTerrainHeight =
                        world.sampleHeight(hitPoint.x, hitPoint.z);
                    hitWater = finalOverRiver && hitPoint.y > finalTerrainHeight + 0.08F;
                    hitTerrain = !hitWater;
                    break;
                }

                previousDistance = distance;
                distance += std::max(1.25F, distance * 0.0045F);
            }

            if (hitTerrain) {
                const Vec3 normal = terrainNormal(world, hitPoint);
                color = terrainColor(environment, hitPoint, normal);
            } else if (hitWater) {
                color = waterColor(world, environment, hitPoint, rayDirection);
            }

            if (hitTerrain || hitWater) {
                const float fogAmount =
                    1.0F - std::exp(-environment.fogDensityPerMeter * distance);
                const float aerialHaze = smoothstep(900.0F, 4800.0F, distance) * 0.34F;
                const float haze = clamp(fogAmount + aerialHaze, 0.0F, 0.82F);
                color = mixColor(color, skyColor(environment, {0.0F, 0.06F, 1.0F}), haze);
            }

            image.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                         static_cast<std::size_t>(x)] = toDisplayColor(color);
        }
    }

    return image;
}

bool writePpm(
    const std::filesystem::path& outputPath,
    const PreviewImage& image) {
    if (image.width <= 0 || image.height <= 0 ||
        image.pixels.size() !=
            static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height)) {
        return false;
    }

    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        return false;
    }

    output << "P6\n" << image.width << ' ' << image.height << "\n255\n";
    for (const Rgb8& pixel : image.pixels) {
        const char bytes[3] = {
            static_cast<char>(pixel.red),
            static_cast<char>(pixel.green),
            static_cast<char>(pixel.blue),
        };
        output.write(bytes, 3);
    }
    return output.good();
}

}  // namespace natureview
