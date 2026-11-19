#include "nature/core/camera.hpp"

#include <algorithm>
#include <cmath>

namespace natureview {

HeroCameraPath::HeroCameraPath(const WorldDefinition& world) {
    constexpr float firstZ = -2300.0F;
    constexpr float lastZ = 2300.0F;
    constexpr float spacing = 80.0F;
    constexpr int pointCount =
        static_cast<int>((lastZ - firstZ) / spacing) + 1;
    points_.reserve(static_cast<std::size_t>(pointCount));

    for (int index = 0; index < pointCount; ++index) {
        const float z = firstZ + static_cast<float>(index) * spacing;
        const float x = world.riverXAt(z) + 92.0F + 7.0F * std::sin(z * 0.0041F);
        const float y = world.sampleHeight(x, z) + 2.1F;
        points_.push_back({x, y, z});
    }

    for (int pass = 0; pass < 5; ++pass) {
        std::vector<Vec3> smoothed = points_;
        for (std::size_t index = 2; index + 2 < points_.size(); ++index) {
            const float weightedHeight =
                points_[index - 2].y +
                2.0F * points_[index - 1].y +
                3.0F * points_[index].y +
                2.0F * points_[index + 1].y +
                points_[index + 2].y;
            smoothed[index].y = weightedHeight / 9.0F;
        }
        points_ = std::move(smoothed);
    }

    cumulativeLengths_.resize(points_.size(), 0.0F);
    for (std::size_t index = 1; index < points_.size(); ++index) {
        cumulativeLengths_[index] =
            cumulativeLengths_[index - 1] + length(points_[index] - points_[index - 1]);
    }
}

CameraState HeroCameraPath::sample(float viewDistanceMeters) const {
    CameraState state;
    if (points_.empty()) {
        return state;
    }

    state.viewDistanceMeters =
        clamp(viewDistanceMeters, 0.0F, lengthMeters());

    const auto upper = std::lower_bound(
        cumulativeLengths_.begin(), cumulativeLengths_.end(), state.viewDistanceMeters);
    const std::size_t endIndex = std::min<std::size_t>(
        static_cast<std::size_t>(std::distance(cumulativeLengths_.begin(), upper)),
        points_.size() - 1);
    const std::size_t beginIndex = endIndex == 0 ? 0 : endIndex - 1;

    const float segmentStart = cumulativeLengths_[beginIndex];
    const float segmentLength = cumulativeLengths_[endIndex] - segmentStart;
    const float amount = segmentLength > 1.0e-6F
                             ? (state.viewDistanceMeters - segmentStart) / segmentLength
                             : 0.0F;

    state.position = {};
    state.position.x =
        lerp(points_[beginIndex].x, points_[endIndex].x, amount);
    state.position.y =
        lerp(points_[beginIndex].y, points_[endIndex].y, amount);
    state.position.z =
        lerp(points_[beginIndex].z, points_[endIndex].z, amount);

    const std::size_t forwardStart = std::min(beginIndex, points_.size() - 2);
    state.forward = normalize(points_[forwardStart + 1] - points_[forwardStart]);
    return state;
}

}  // namespace natureview
