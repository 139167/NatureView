#pragma once

#include <cstdint>
#include <vector>

namespace natureview {

struct Texture2DRgba8 {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> pixels;

    [[nodiscard]] std::uint64_t fingerprint() const;
};

struct MaterialTextureSet {
    Texture2DRgba8 grass;
    Texture2DRgba8 rock;
    Texture2DRgba8 bark;
};

[[nodiscard]] MaterialTextureSet generateMaterialTextures(
    std::uint64_t seed,
    std::uint32_t size = 256);

}  // namespace natureview
