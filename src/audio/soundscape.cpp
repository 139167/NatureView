#include "nature/audio/soundscape.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <numbers>

namespace natureview {
namespace {

constexpr std::uint32_t kChannelCount = 2;
constexpr std::uint16_t kBitsPerSample = 16;

float signedNoise(std::uint32_t& state) {
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    return static_cast<float>(state) * (2.0F / 4294967295.0F) - 1.0F;
}

void writeU16(std::ofstream& output, std::uint16_t value) {
    const char bytes[2] = {
        static_cast<char>(value & 0xffU),
        static_cast<char>((value >> 8U) & 0xffU),
    };
    output.write(bytes, 2);
}

void writeU32(std::ofstream& output, std::uint32_t value) {
    const char bytes[4] = {
        static_cast<char>(value & 0xffU),
        static_cast<char>((value >> 8U) & 0xffU),
        static_cast<char>((value >> 16U) & 0xffU),
        static_cast<char>((value >> 24U) & 0xffU),
    };
    output.write(bytes, 4);
}

}  // namespace

SoundscapeParameters SoundscapeDirector::evaluate(
    const EnvironmentState& environment,
    const CameraState& camera,
    const WorldDefinition& world) const {
    SoundscapeParameters parameters;
    parameters.windGain = clamp(environment.windSpeedMetersPerSecond / 11.0F, 0.0F, 1.0F);
    parameters.windTurbulence =
        clamp((environment.gustFactor - 0.45F) / 0.75F, 0.0F, 1.0F);
    parameters.foliageGain =
        parameters.windGain * (1.0F - environment.rainIntensity * 0.35F);
    parameters.rainGain =
        smoothstep(0.02F, 0.82F, environment.rainIntensity);

    const float riverX = world.riverXAt(camera.position.z);
    const float riverDistance = std::abs(camera.position.x - riverX);
    const float proximity = 1.0F / (1.0F + riverDistance / 90.0F);
    const float discharge =
        clamp((environment.riverDischargeCubicMetersPerSecond - 2.0F) / 20.0F,
              0.0F,
              1.0F);
    parameters.waterGain = discharge * (0.30F + proximity * 0.70F);
    parameters.waterFlow = discharge;

    const Vec3 toRiver = {riverX - camera.position.x, 0.0F, 0.0F};
    const Vec3 right =
        normalize(cross({0.0F, 1.0F, 0.0F}, camera.forward));
    parameters.waterPan =
        clamp(dot(normalize(toRiver), right) * 0.72F, -0.65F, 0.65F);
    return parameters;
}

std::vector<float> OfflineSoundscapeRenderer::render(
    const SoundscapeParameters& parameters,
    SoundscapeRenderSettings settings) const {
    settings.sampleRate = std::clamp(settings.sampleRate, 8000U, 192000U);
    settings.durationSeconds =
        std::clamp(settings.durationSeconds, 0.01, 60.0);
    const auto frameCount = static_cast<std::size_t>(
        std::llround(settings.durationSeconds * settings.sampleRate));
    std::vector<float> output(frameCount * kChannelCount);

    std::uint32_t leftState =
        static_cast<std::uint32_t>(settings.seed ^ 0x9e3779b9ULL);
    std::uint32_t rightState =
        static_cast<std::uint32_t>((settings.seed >> 32U) ^ 0x85ebca6bULL);
    if (leftState == 0) {
        leftState = 1;
    }
    if (rightState == 0) {
        rightState = 1;
    }

    float leftWindLow = 0.0F;
    float rightWindLow = 0.0F;
    float leftWaterFast = 0.0F;
    float rightWaterFast = 0.0F;
    float leftWaterSlow = 0.0F;
    float rightWaterSlow = 0.0F;
    float leftRainPrevious = 0.0F;
    float rightRainPrevious = 0.0F;

    const float panAngle = (parameters.waterPan + 1.0F) * 0.25F * std::numbers::pi_v<float>;
    const float waterLeft = std::cos(panAngle);
    const float waterRight = std::sin(panAngle);
    const float secondsPerFrame = 1.0F / static_cast<float>(settings.sampleRate);
    const float windCoefficient = 0.0012F + parameters.windTurbulence * 0.003F;
    const float waterFastCoefficient = 0.035F + parameters.waterFlow * 0.06F;
    const float waterSlowCoefficient = 0.006F + parameters.waterFlow * 0.012F;
    const float fadeFrames = std::max(1.0F, 0.012F * settings.sampleRate);

    for (std::size_t frame = 0; frame < frameCount; ++frame) {
        const float seconds = static_cast<float>(frame) * secondsPerFrame;
        const float leftNoise = signedNoise(leftState);
        const float rightNoise = signedNoise(rightState);

        leftWindLow += (leftNoise - leftWindLow) * windCoefficient;
        rightWindLow += (rightNoise - rightWindLow) * windCoefficient;
        leftWaterFast +=
            (leftNoise - leftWaterFast) * waterFastCoefficient;
        rightWaterFast +=
            (rightNoise - rightWaterFast) * waterFastCoefficient;
        leftWaterSlow +=
            (leftWaterFast - leftWaterSlow) * waterSlowCoefficient;
        rightWaterSlow +=
            (rightWaterFast - rightWaterSlow) * waterSlowCoefficient;

        const float gust =
            0.62F +
            0.38F * std::sin(
                        seconds * (0.23F + parameters.windTurbulence * 0.31F) *
                            std::numbers::pi_v<float> * 2.0F);
        const float windLeft = leftWindLow * 2.4F * gust;
        const float windRight = rightWindLow * 2.4F * gust;
        const float waterLeftSignal =
            (leftWaterFast - leftWaterSlow * 0.72F) * 2.0F;
        const float waterRightSignal =
            (rightWaterFast - rightWaterSlow * 0.72F) * 2.0F;
        const float rainLeft =
            (leftNoise - leftRainPrevious * 0.88F) * 1.7F;
        const float rainRight =
            (rightNoise - rightRainPrevious * 0.88F) * 1.7F;
        leftRainPrevious = leftNoise;
        rightRainPrevious = rightNoise;
        const float foliageLeft =
            (leftNoise - leftWindLow) * 0.35F * parameters.foliageGain;
        const float foliageRight =
            (rightNoise - rightWindLow) * 0.35F * parameters.foliageGain;

        float mixLeft =
            windLeft * parameters.windGain * 0.34F +
            waterLeftSignal * parameters.waterGain * 0.62F * waterLeft +
            rainLeft * parameters.rainGain * 0.42F +
            foliageLeft * 0.25F;
        float mixRight =
            windRight * parameters.windGain * 0.34F +
            waterRightSignal * parameters.waterGain * 0.62F * waterRight +
            rainRight * parameters.rainGain * 0.42F +
            foliageRight * 0.25F;

        const float fade = std::min(
            1.0F,
            std::min(
                static_cast<float>(frame) / fadeFrames,
                static_cast<float>(frameCount - frame - 1) / fadeFrames));
        output[frame * 2] = std::tanh(mixLeft * 0.90F) * fade;
        output[frame * 2 + 1] = std::tanh(mixRight * 0.90F) * fade;
    }
    return output;
}

bool writeStereoWav(
    const std::filesystem::path& outputPath,
    const std::vector<float>& interleavedSamples,
    std::uint32_t sampleRate) {
    if (interleavedSamples.empty() ||
        interleavedSamples.size() % kChannelCount != 0 ||
        sampleRate == 0) {
        return false;
    }

    if (!outputPath.parent_path().empty()) {
        std::filesystem::create_directories(outputPath.parent_path());
    }
    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        return false;
    }

    const auto frameCount =
        static_cast<std::uint32_t>(interleavedSamples.size() / kChannelCount);
    const std::uint32_t dataSize =
        frameCount * kChannelCount * (kBitsPerSample / 8U);
    output.write("RIFF", 4);
    writeU32(output, 36U + dataSize);
    output.write("WAVEfmt ", 8);
    writeU32(output, 16);
    writeU16(output, 1);
    writeU16(output, static_cast<std::uint16_t>(kChannelCount));
    writeU32(output, sampleRate);
    writeU32(output, sampleRate * kChannelCount * (kBitsPerSample / 8U));
    writeU16(output, static_cast<std::uint16_t>(kChannelCount * (kBitsPerSample / 8U)));
    writeU16(output, kBitsPerSample);
    output.write("data", 4);
    writeU32(output, dataSize);

    for (const float sample : interleavedSamples) {
        const float bounded = clamp(sample, -1.0F, 1.0F);
        const auto value = static_cast<std::int16_t>(
            std::lround(bounded * std::numeric_limits<std::int16_t>::max()));
        writeU16(output, static_cast<std::uint16_t>(value));
    }
    return output.good();
}

}  // namespace natureview
