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

SoundscapeSynthesizer::SoundscapeSynthesizer(
    std::uint64_t seed,
    std::uint32_t sampleRate) {
    sampleRate_ = static_cast<float>(std::clamp(sampleRate, 8000U, 192000U));
    sampleSeconds_ = 1.0F / sampleRate_;
    parameterSmoothing_ =
        1.0F - std::exp(-1.0F / (0.18F * sampleRate_));
    leftNoiseState_ =
        static_cast<std::uint32_t>(seed ^ 0x9e3779b9ULL);
    rightNoiseState_ =
        static_cast<std::uint32_t>((seed >> 32U) ^ 0x85ebca6bULL);
    if (leftNoiseState_ == 0) {
        leftNoiseState_ = 1;
    }
    if (rightNoiseState_ == 0) {
        rightNoiseState_ = 1;
    }
}

void SoundscapeSynthesizer::setParameters(
    const SoundscapeParameters& parameters) {
    target_ = parameters;
    if (!parametersInitialized_) {
        current_ = target_;
        parametersInitialized_ = true;
    }
}

void SoundscapeSynthesizer::nextSample(float& left, float& right) {
    current_.windGain +=
        (target_.windGain - current_.windGain) * parameterSmoothing_;
    current_.windTurbulence +=
        (target_.windTurbulence - current_.windTurbulence) * parameterSmoothing_;
    current_.foliageGain +=
        (target_.foliageGain - current_.foliageGain) * parameterSmoothing_;
    current_.waterGain +=
        (target_.waterGain - current_.waterGain) * parameterSmoothing_;
    current_.waterFlow +=
        (target_.waterFlow - current_.waterFlow) * parameterSmoothing_;
    current_.waterPan +=
        (target_.waterPan - current_.waterPan) * parameterSmoothing_;
    current_.rainGain +=
        (target_.rainGain - current_.rainGain) * parameterSmoothing_;

    const float leftNoise = signedNoise(leftNoiseState_);
    const float rightNoise = signedNoise(rightNoiseState_);
    const float windCoefficient =
        0.0012F + current_.windTurbulence * 0.003F;
    const float waterFastCoefficient =
        0.035F + current_.waterFlow * 0.06F;
    const float waterSlowCoefficient =
        0.006F + current_.waterFlow * 0.012F;

    leftWindLow_ += (leftNoise - leftWindLow_) * windCoefficient;
    rightWindLow_ += (rightNoise - rightWindLow_) * windCoefficient;
    leftWaterFast_ +=
        (leftNoise - leftWaterFast_) * waterFastCoefficient;
    rightWaterFast_ +=
        (rightNoise - rightWaterFast_) * waterFastCoefficient;
    leftWaterSlow_ +=
        (leftWaterFast_ - leftWaterSlow_) * waterSlowCoefficient;
    rightWaterSlow_ +=
        (rightWaterFast_ - rightWaterSlow_) * waterSlowCoefficient;

    const float gust =
        0.62F +
        0.38F * std::sin(
                    elapsedSeconds_ *
                    (0.23F + current_.windTurbulence * 0.31F) *
                    std::numbers::pi_v<float> * 2.0F);
    const float panAngle =
        (current_.waterPan + 1.0F) * 0.25F * std::numbers::pi_v<float>;
    const float waterLeft = std::cos(panAngle);
    const float waterRight = std::sin(panAngle);
    const float waterLeftSignal =
        (leftWaterFast_ - leftWaterSlow_ * 0.72F) * 2.0F;
    const float waterRightSignal =
        (rightWaterFast_ - rightWaterSlow_ * 0.72F) * 2.0F;
    const float rainLeft =
        (leftNoise - leftRainPrevious_ * 0.88F) * 1.7F;
    const float rainRight =
        (rightNoise - rightRainPrevious_ * 0.88F) * 1.7F;
    leftRainPrevious_ = leftNoise;
    rightRainPrevious_ = rightNoise;

    const float mixLeft =
        leftWindLow_ * 2.4F * gust * current_.windGain * 0.34F +
        waterLeftSignal * current_.waterGain * 0.62F * waterLeft +
        rainLeft * current_.rainGain * 0.42F +
        (leftNoise - leftWindLow_) * 0.35F * current_.foliageGain * 0.25F;
    const float mixRight =
        rightWindLow_ * 2.4F * gust * current_.windGain * 0.34F +
        waterRightSignal * current_.waterGain * 0.62F * waterRight +
        rainRight * current_.rainGain * 0.42F +
        (rightNoise - rightWindLow_) * 0.35F * current_.foliageGain * 0.25F;

    left = std::tanh(mixLeft * 0.90F);
    right = std::tanh(mixRight * 0.90F);
    elapsedSeconds_ += sampleSeconds_;
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

    SoundscapeSynthesizer synthesizer(settings.seed, settings.sampleRate);
    synthesizer.setParameters(parameters);
    const float fadeFrames = std::max(1.0F, 0.012F * settings.sampleRate);

    for (std::size_t frame = 0; frame < frameCount; ++frame) {
        float left = 0.0F;
        float right = 0.0F;
        synthesizer.nextSample(left, right);

        const float fade = std::min(
            1.0F,
            std::min(
                static_cast<float>(frame) / fadeFrames,
                static_cast<float>(frameCount - frame - 1) / fadeFrames));
        output[frame * 2] = left * fade;
        output[frame * 2 + 1] = right * fade;
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
