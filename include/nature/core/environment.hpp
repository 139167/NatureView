#pragma once

#include <cstdint>

namespace natureview {

struct EnvironmentState {
    double elapsedSeconds = 0.0;
    double solarTimeSeconds = 0.0;
    float cloudCover = 0.0F;
    float humidity = 0.0F;
    float rainIntensity = 0.0F;
    float fogDensityPerMeter = 0.0F;
    float windSpeedMetersPerSecond = 0.0F;
    float windDirectionRadians = 0.0F;
    float gustFactor = 0.0F;
    float riverDischargeCubicMetersPerSecond = 0.0F;
    float airTemperatureCelsius = 0.0F;
    float sunElevationDegrees = 0.0F;
    float sunAzimuthRadians = 0.0F;
};

class EnvironmentModel {
public:
    explicit EnvironmentModel(std::uint64_t seed);

    void step(float fixedDeltaSeconds, double elapsedSeconds);

    [[nodiscard]] const EnvironmentState& state() const {
        return state_;
    }

private:
    double startSolarTimeSeconds_ = 0.0;
    float phaseA_ = 0.0F;
    float phaseB_ = 0.0F;
    float phaseC_ = 0.0F;
    float rainMemory_ = 0.0F;
    EnvironmentState state_{};
};

}  // namespace natureview
