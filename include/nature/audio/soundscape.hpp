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

class SoundscapeSynthesizer {
public:
    SoundscapeSynthesizer(std::uint64_t seed, std::uint32_t sampleRate);

    void setParameters(const SoundscapeParameters& parameters);
    void nextSample(float& left, float& right);

private:
    std::uint32_t leftNoiseState_ = 0;
    std::uint32_t rightNoiseState_ = 0;
    float leftWindLow_ = 0.0F;
    float rightWindLow_ = 0.0F;
    float leftWaterFast_ = 0.0F;
    float rightWaterFast_ = 0.0F;
    float leftWaterSlow_ = 0.0F;
    float rightWaterSlow_ = 0.0F;
    float leftRainPrevious_ = 0.0F;
    float rightRainPrevious_ = 0.0F;
    float sampleRate_ = 48000.0F;
    float sampleSeconds_ = 0.0F;
    float elapsedSeconds_ = 0.0F;
    float parameterSmoothing_ = 0.0F;
    bool parametersInitialized_ = false;
    SoundscapeParameters current_;
    SoundscapeParameters target_;
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
