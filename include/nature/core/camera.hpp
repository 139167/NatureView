#pragma once

#include <vector>

#include "nature/core/math.hpp"
#include "nature/core/world.hpp"

namespace natureview {

struct CameraState {
    Vec3 position;
    Vec3 forward{0.0F, 0.0F, 1.0F};
    float viewDistanceMeters = 0.0F;
    float verticalFovDegrees = 52.0F;
};

class HeroCameraPath {
public:
    explicit HeroCameraPath(const WorldDefinition& world);

    [[nodiscard]] CameraState sample(float viewDistanceMeters) const;
    [[nodiscard]] float lengthMeters() const {
        return cumulativeLengths_.empty() ? 0.0F : cumulativeLengths_.back();
    }

private:
    std::vector<Vec3> points_;
    std::vector<float> cumulativeLengths_;
};

}  // namespace natureview
