#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: metal_capture_tests IMAGE.ppm\n";
        return EXIT_FAILURE;
    }

    std::ifstream input(argv[1], std::ios::binary);
    std::string magic;
    int width = 0;
    int height = 0;
    int maximumValue = 0;
    input >> magic >> width >> height >> maximumValue;
    input.get();

    expect(magic == "P6", "capture must be a binary PPM");
    const bool validRetinaScale =
        width >= 320 &&
        height >= 180 &&
        width % 320 == 0 &&
        height % 180 == 0 &&
        width / 320 == height / 180;
    expect(validRetinaScale,
           "capture must match the requested logical size at a valid backing scale");
    expect(maximumValue == 255, "capture must use eight-bit channels");

    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3);
    input.read(
        reinterpret_cast<char*>(pixels.data()),
        static_cast<std::streamsize>(pixels.size()));
    expect(input.gcount() == static_cast<std::streamsize>(pixels.size()),
           "capture must contain complete RGB data");

    std::set<std::uint32_t> quantizedColors;
    int minimumLuminance = 255;
    int maximumLuminance = 0;
    int greenDominantPixels = 0;
    int blueDominantPixels = 0;
    for (std::size_t index = 0; index < pixels.size(); index += 3) {
        const int red = pixels[index];
        const int green = pixels[index + 1];
        const int blue = pixels[index + 2];
        minimumLuminance = std::min(minimumLuminance, (red + green + blue) / 3);
        maximumLuminance = std::max(maximumLuminance, (red + green + blue) / 3);
        quantizedColors.insert(
            static_cast<std::uint32_t>((red >> 4) << 8) |
            static_cast<std::uint32_t>((green >> 4) << 4) |
            static_cast<std::uint32_t>(blue >> 4));
        greenDominantPixels += green > blue && green > red ? 1 : 0;
        blueDominantPixels += blue > green && blue > red ? 1 : 0;
    }

    const int pixelCount = width * height;
    expect(quantizedColors.size() > 16, "capture must contain varied colors");
    expect(maximumLuminance - minimumLuminance > 24,
           "capture must contain meaningful luminance variation");
    expect(greenDominantPixels > pixelCount / 20,
           "capture must contain visible vegetation");
    expect(blueDominantPixels > pixelCount / 20,
           "capture must contain visible sky or water");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "Metal capture structure passed\n";
    return EXIT_SUCCESS;
}
