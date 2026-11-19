#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "nature/core/simulation.hpp"

namespace natureview {

struct PreviewSettings {
    int width = 960;
    int height = 540;
    float maximumDistanceMeters = 4500.0F;
};

struct Rgb8 {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
};

struct PreviewImage {
    int width = 0;
    int height = 0;
    std::vector<Rgb8> pixels;

    [[nodiscard]] std::uint64_t fingerprint() const;
};

class SoftwarePreviewRenderer {
public:
    [[nodiscard]] PreviewImage render(
        const NatureSimulation& simulation,
        PreviewSettings settings = {}) const;
};

[[nodiscard]] bool writePpm(
    const std::filesystem::path& outputPath,
    const PreviewImage& image);

}  // namespace natureview
