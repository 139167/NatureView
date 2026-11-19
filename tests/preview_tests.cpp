#include <cstdlib>
#include <iostream>
#include <string>

#include "nature/render/preview_renderer.hpp"

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

}  // namespace

int main() {
    natureview::NatureSimulation first(139167);
    natureview::NatureSimulation second(139167);
    natureview::SoftwarePreviewRenderer renderer;
    const natureview::PreviewSettings settings{192, 108, 2500.0F};

    const natureview::PreviewImage firstImage = renderer.render(first, settings);
    const natureview::PreviewImage secondImage = renderer.render(second, settings);

    expect(firstImage.width == 192 && firstImage.height == 108,
           "preview must use requested dimensions");
    expect(firstImage.pixels.size() == 192U * 108U,
           "preview must contain one color per pixel");
    expect(firstImage.fingerprint() == secondImage.fingerprint(),
           "same simulation state must render identically");

    bool hasDarkTerrain = false;
    bool hasSky = false;
    for (const natureview::Rgb8& pixel : firstImage.pixels) {
        hasDarkTerrain = hasDarkTerrain || pixel.green < pixel.blue;
        hasSky = hasSky || pixel.blue > pixel.green;
    }
    expect(hasDarkTerrain, "preview must contain landscape or water pixels");
    expect(hasSky, "preview must contain sky pixels");

    for (int frame = 0; frame < 1200; ++frame) {
        first.advance(natureview::NatureSimulation::kFixedStepSeconds);
    }
    const natureview::PreviewImage laterImage = renderer.render(first, settings);
    expect(laterImage.fingerprint() != firstImage.fingerprint(),
           "camera movement must change the rendered image");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All preview tests passed\n";
    return EXIT_SUCCESS;
}
