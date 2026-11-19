#include "nature/core/environment.hpp"

#include <cmath>

#include "nature/core/math.hpp"

namespace natureview {
namespace {

float seedUnit(std::uint64_t seed, std::uint64_t salt) {
    const std::uint64_t value = mixBits(seed ^ salt);
    return static_cast<float>(value >> 40U) * (1.0F / 16777216.0F);
}

}  // namespace

EnvironmentModel::EnvironmentModel(std::uint64_t seed, float latitudeDegrees)
    : startSolarTimeSeconds_(static_cast<double>(seedUnit(seed, 0x3c217ef9a804d5b1ULL)) * 86400.0),
      phaseA_(seedUnit(seed, 0xa93475d18c2f06e7ULL) * 2.0F * kPi),
      phaseB_(seedUnit(seed, 0x5d81e34bc790a2f9ULL) * 2.0F * kPi),
      phaseC_(seedUnit(seed, 0x71f8c39d5ea2460bULL) * 2.0F * kPi),
      latitudeDegrees_(latitudeDegrees) {
    step(0.0F, 0.0);
}

void EnvironmentModel::step(float fixedDeltaSeconds, double elapsedSeconds) {
    const float time = static_cast<float>(elapsedSeconds);
    const float cloudWave =
        0.54F + 0.16F * std::sin(time * 0.0183F + phaseA_) +
        0.10F * std::sin(time * 0.0071F + phaseB_) +
        0.035F * std::sin(time * 0.061F + phaseC_);
    const float stormSignal =
        0.5F + 0.32F * std::sin(time * 0.0039F + phaseB_ + 0.8F) +
        0.18F * std::sin(time * 0.0017F + phaseC_);

    state_.elapsedSeconds = elapsedSeconds;
    state_.solarTimeSeconds =
        std::fmod(startSolarTimeSeconds_ + elapsedSeconds, 86400.0);
    state_.cloudCover = clamp(cloudWave, 0.0F, 1.0F);
    state_.rainIntensity =
        clamp((stormSignal - 0.62F) / 0.34F, 0.0F, 1.0F) *
        clamp((state_.cloudCover - 0.52F) / 0.38F, 0.0F, 1.0F);

    const float rainRetention = 1.0F - std::exp(-fixedDeltaSeconds / 95.0F);
    rainMemory_ += (state_.rainIntensity - rainMemory_) * rainRetention;
    rainMemory_ = clamp(rainMemory_, 0.0F, 1.0F);

    state_.humidity =
        clamp(0.48F + state_.cloudCover * 0.34F + state_.rainIntensity * 0.15F, 0.0F, 1.0F);
    state_.fogDensityPerMeter =
        0.000012F + std::pow(state_.humidity, 4.0F) * 0.000085F +
        state_.rainIntensity * 0.000055F;
    state_.gustFactor =
        0.82F + 0.28F * std::sin(time * 0.071F + phaseA_) +
        0.10F * std::sin(time * 0.19F + phaseC_);
    state_.windSpeedMetersPerSecond =
        2.0F + 2.6F * (0.5F + 0.5F * std::sin(time * 0.011F + phaseB_)) +
        state_.rainIntensity * 2.8F;
    state_.windSpeedMetersPerSecond *= clamp(state_.gustFactor, 0.45F, 1.25F);
    state_.windDirectionRadians =
        std::fmod(0.7F + time * 0.0014F + 0.22F * std::sin(time * 0.0043F + phaseC_),
                  2.0F * kPi);
    if (state_.windDirectionRadians < 0.0F) {
        state_.windDirectionRadians += 2.0F * kPi;
    }

    const float dayFraction =
        static_cast<float>(state_.solarTimeSeconds / 86400.0);
    const float dayAngle = dayFraction * 2.0F * kPi;
    state_.sunElevationDegrees = -8.0F + 52.0F * (0.5F + 0.5F * std::sin(dayAngle - kPi * 0.5F));
    state_.sunAzimuthRadians = std::fmod(dayAngle + 4.7F, 2.0F * kPi);
    state_.airTemperatureCelsius =
        7.0F + 3.5F * std::sin(dayAngle - kPi * 0.5F) -
        3.0F * state_.cloudCover - 1.5F * state_.rainIntensity;
    state_.riverDischargeCubicMetersPerSecond =
        13.0F + 4.0F * std::sin(time * 0.0009F + phaseA_) + rainMemory_ * 18.0F;

    const float nightFactor =
        1.0F - smoothstep(-6.0F, 3.0F, state_.sunElevationDegrees);
    const float clearSky =
        1.0F - smoothstep(0.42F, 0.78F, state_.cloudCover);
    const float latitudeFactor = 1.0F - clamp(
        std::abs(latitudeDegrees_ - 67.0F) / 14.0F,
        0.0F,
        1.0F);
    const float auroraSignal = clamp(
        0.48F +
            0.28F * std::sin(time * 0.0013F + phaseC_) +
            0.18F * std::sin(time * 0.0031F + phaseA_ + 1.7F),
        0.0F,
        1.0F);
    state_.auroraActivity =
        clamp(nightFactor * clearSky * latitudeFactor * auroraSignal, 0.0F, 1.0F);
}

}  // namespace natureview
