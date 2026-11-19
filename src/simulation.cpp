#include "nature/core/simulation.hpp"

#include <algorithm>
#include <cmath>

namespace natureview {
namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;

}  // namespace

NatureSimulation::NatureSimulation(std::uint64_t seed)
    : world_(generateHeroWorld(seed)),
      cameraPath_(world_),
      environment_(seed),
      camera_(cameraPath_.sample(0.0F)) {}

void NatureSimulation::advance(double wallDeltaSeconds) {
    if (!std::isfinite(wallDeltaSeconds) || wallDeltaSeconds <= 0.0) {
        return;
    }

    constexpr double maximumCatchUpSeconds = 0.25;
    constexpr double tickToleranceSeconds = 1.0e-9;
    accumulatorSeconds_ += std::min(wallDeltaSeconds, maximumCatchUpSeconds);

    while (accumulatorSeconds_ + tickToleranceSeconds >=
           static_cast<double>(kFixedStepSeconds)) {
        stepFixed();
        accumulatorSeconds_ -= static_cast<double>(kFixedStepSeconds);
        accumulatorSeconds_ = std::max(accumulatorSeconds_, 0.0);
    }
}

void NatureSimulation::stepFixed() {
    ++tick_;
    elapsedSeconds_ += static_cast<double>(kFixedStepSeconds);
    environment_.step(kFixedStepSeconds, elapsedSeconds_);

    cameraDistanceMeters_ = std::min(
        cameraDistanceMeters_ + kCameraSpeedMetersPerSecond * kFixedStepSeconds,
        cameraPath_.lengthMeters());
    camera_ = cameraPath_.sample(cameraDistanceMeters_);
}

std::uint64_t NatureSimulation::stateFingerprint() const {
    std::uint64_t fingerprint = kFnvOffset;
    hashCombine(fingerprint, world_.fingerprint);
    hashCombine(fingerprint, tick_);

    const EnvironmentState& environment = environment_.state();
    hashCombine(fingerprint, environment.cloudCover);
    hashCombine(fingerprint, environment.humidity);
    hashCombine(fingerprint, environment.rainIntensity);
    hashCombine(fingerprint, environment.fogDensityPerMeter);
    hashCombine(fingerprint, environment.windSpeedMetersPerSecond);
    hashCombine(fingerprint, environment.windDirectionRadians);
    hashCombine(fingerprint, environment.gustFactor);
    hashCombine(fingerprint, environment.riverDischargeCubicMetersPerSecond);
    hashCombine(fingerprint, environment.airTemperatureCelsius);
    hashCombine(fingerprint, environment.sunElevationDegrees);
    hashCombine(fingerprint, environment.sunAzimuthRadians);

    hashCombine(fingerprint, camera_.position.x);
    hashCombine(fingerprint, camera_.position.y);
    hashCombine(fingerprint, camera_.position.z);
    hashCombine(fingerprint, camera_.forward.x);
    hashCombine(fingerprint, camera_.forward.y);
    hashCombine(fingerprint, camera_.forward.z);
    return fingerprint;
}

}  // namespace natureview
