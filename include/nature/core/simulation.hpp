#pragma once

#include <cstdint>

#include "nature/core/camera.hpp"
#include "nature/core/environment.hpp"
#include "nature/core/world.hpp"

namespace natureview {

class NatureSimulation {
public:
    static constexpr float kFixedStepSeconds = 0.05F;
    static constexpr float kCameraSpeedMetersPerSecond = 0.65F;

    explicit NatureSimulation(std::uint64_t seed);

    void advance(double wallDeltaSeconds);

    [[nodiscard]] const WorldDefinition& world() const {
        return world_;
    }

    [[nodiscard]] const EnvironmentState& environment() const {
        return environment_.state();
    }

    [[nodiscard]] const CameraState& camera() const {
        return camera_;
    }

    [[nodiscard]] double elapsedSeconds() const {
        return elapsedSeconds_;
    }

    [[nodiscard]] std::uint64_t tickCount() const {
        return tick_;
    }

    [[nodiscard]] std::uint64_t stateFingerprint() const;

private:
    void stepFixed();

    WorldDefinition world_;
    HeroCameraPath cameraPath_;
    EnvironmentModel environment_;
    CameraState camera_;
    double accumulatorSeconds_ = 0.0;
    double elapsedSeconds_ = 0.0;
    float cameraDistanceMeters_ = 0.0F;
    std::uint64_t tick_ = 0;
};

}  // namespace natureview
