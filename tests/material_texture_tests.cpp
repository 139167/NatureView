#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

#include "nature/render/material_textures.hpp"

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void validate(
    const natureview::Texture2DRgba8& texture,
    const std::string& name) {
    expect(texture.width == 128 && texture.height == 128,
           name + " dimensions must match");
    expect(texture.pixels.size() == 128U * 128U * 4U,
           name + " must contain RGBA8 pixels");
    std::set<std::uint32_t> colors;
    for (std::size_t index = 0; index < texture.pixels.size(); index += 4) {
        const std::uint32_t color =
            static_cast<std::uint32_t>(texture.pixels[index]) |
            (static_cast<std::uint32_t>(texture.pixels[index + 1]) << 8U) |
            (static_cast<std::uint32_t>(texture.pixels[index + 2]) << 16U);
        colors.insert(color);
        expect(texture.pixels[index + 3] == 255,
               name + " alpha must be opaque");
    }
    expect(colors.size() > 24, name + " must contain surface variation");
}

}  // namespace

int main() {
    const auto first = natureview::generateMaterialTextures(139167, 128);
    const auto second = natureview::generateMaterialTextures(139167, 128);
    const auto different = natureview::generateMaterialTextures(139168, 128);

    validate(first.grass, "grass");
    validate(first.rock, "rock");
    validate(first.bark, "bark");
    expect(first.grass.fingerprint() == second.grass.fingerprint(),
           "grass texture must be deterministic");
    expect(first.rock.fingerprint() == second.rock.fingerprint(),
           "rock texture must be deterministic");
    expect(first.bark.fingerprint() == second.bark.fingerprint(),
           "bark texture must be deterministic");
    expect(first.grass.fingerprint() != different.grass.fingerprint(),
           "seed must change grass texture");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "Material texture tests passed\n";
    return EXIT_SUCCESS;
}
