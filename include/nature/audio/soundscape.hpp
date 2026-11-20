#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "nature/core/camera.hpp"
#include "nature/core/environment.hpp"
#include "nature/core/world.hpp"

namespace natureview {

struct SoundscapeParameters {
    float windGain = 0.0F;
    float windTurbulence = 0.0F;
    float foliageGain = 0.0F;
    float waterGain = 0.0F;
    float waterFlow = 0.0F;
    float waterPan = 0.0F;
    float rainGain = 0.0F;
};

class SoundscapeDirector {
public:
    [[nodiscard]] SoundscapeParameters evaluate(
        const EnvironmentState& environment,
        const CameraState& camera,
        const WorldDefinition& world) const;
};

struct SoundscapeRenderSettings {
    std::uint32_t sampleRate = 48000;
    double durationSeconds = 1.0;
    std::uint64_t seed = 0;
};

class OfflineSoundscapeRenderer {
public:
    [[nodiscard]] std::vector<float> render(
        const SoundscapeParameters& parameters,
        SoundscapeRenderSettings settings) const;
};

[[nodiscard]] bool writeStereoWav(
    const std::filesystem::path& outputPath,
    const std::vector<float>& interleavedSamples,
    std::uint32_t sampleRate);

}  // namespace natureview
