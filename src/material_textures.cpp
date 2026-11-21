#include "nature/render/material_textures.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "nature/core/math.hpp"

namespace natureview {
namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;

float hash2(int x, int y, std::uint64_t seed) {
    std::uint64_t value =
        seed ^
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) *
         0x9e3779b185ebca87ULL) ^
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(y)) *
         0xc2b2ae3d27d4eb4fULL);
    value = mixBits(value);
    return static_cast<float>(value >> 40U) * (1.0F / 16777216.0F);
}

int wrapIndex(int value, int period) {
    value %= period;
    return value < 0 ? value + period : value;
}

float tileNoise(
    float u,
    float v,
    int period,
    std::uint64_t seed) {
    const float scaledU = u * static_cast<float>(period);
    const float scaledV = v * static_cast<float>(period);
    const int x0 = static_cast<int>(std::floor(scaledU));
    const int y0 = static_cast<int>(std::floor(scaledV));
    const float fx = scaledU - std::floor(scaledU);
    const float fy = scaledV - std::floor(scaledV);
    const float sx = fx * fx * (3.0F - 2.0F * fx);
    const float sy = fy * fy * (3.0F - 2.0F * fy);
    const auto sample = [&](int x, int y) {
        return hash2(wrapIndex(x, period), wrapIndex(y, period), seed);
    };
    const float a = lerp(sample(x0, y0), sample(x0 + 1, y0), sx);
    const float b = lerp(sample(x0, y0 + 1), sample(x0 + 1, y0 + 1), sx);
    return lerp(a, b, sy);
}

float tileFbm(
    float u,
    float v,
    int basePeriod,
    int octaves,
    std::uint64_t seed) {
    float value = 0.0F;
    float amplitude = 0.5F;
    int period = basePeriod;
    for (int octave = 0; octave < octaves; ++octave) {
        value += tileNoise(u, v, period, seed + static_cast<std::uint64_t>(octave) * 0x517cc1b727220a95ULL) *
                 amplitude;
        period *= 2;
        amplitude *= 0.5F;
    }
    return value;
}

std::uint8_t byteValue(float value) {
    return static_cast<std::uint8_t>(
        std::lround(clamp(value, 0.0F, 1.0F) * 255.0F));
}

void writePixel(
    Texture2DRgba8& texture,
    std::uint32_t x,
    std::uint32_t y,
    float red,
    float green,
    float blue) {
    const std::size_t index =
        (static_cast<std::size_t>(y) * texture.width + x) * 4;
    texture.pixels[index] = byteValue(red);
    texture.pixels[index + 1] = byteValue(green);
    texture.pixels[index + 2] = byteValue(blue);
    texture.pixels[index + 3] = 255;
}

Texture2DRgba8 allocateTexture(std::uint32_t size) {
    Texture2DRgba8 texture;
    texture.width = size;
    texture.height = size;
    texture.pixels.resize(static_cast<std::size_t>(size) * size * 4);
    return texture;
}

}  // namespace

std::uint64_t Texture2DRgba8::fingerprint() const {
    std::uint64_t hash = kFnvOffset;
    hashCombine(hash, static_cast<std::uint64_t>(width));
    hashCombine(hash, static_cast<std::uint64_t>(height));
    for (const std::uint8_t value : pixels) {
        hashCombine(hash, static_cast<std::uint64_t>(value));
    }
    return hash;
}

MaterialTextureSet generateMaterialTextures(
    std::uint64_t seed,
    std::uint32_t size) {
    size = std::clamp(size, 64U, 1024U);
    MaterialTextureSet textures{
        allocateTexture(size),
        allocateTexture(size),
        allocateTexture(size),
    };

    for (std::uint32_t y = 0; y < size; ++y) {
        const float v = (static_cast<float>(y) + 0.5F) / static_cast<float>(size);
        for (std::uint32_t x = 0; x < size; ++x) {
            const float u = (static_cast<float>(x) + 0.5F) / static_cast<float>(size);
            const float broad = tileFbm(u, v, 3, 4, seed ^ 0x1b4f03a9d72c65e1ULL);
            const float fine = tileNoise(u, v, 48, seed ^ 0x8d29a7f13b6e40c5ULL);

            const float grass = 0.70F + broad * 0.42F - fine * 0.17F;
            writePixel(
                textures.grass,
                x,
                y,
                0.10F + grass * 0.10F,
                0.19F + grass * 0.20F,
                0.055F + grass * 0.065F);

            const float ridge =
                1.0F - std::abs(tileFbm(u, v, 4, 4, seed ^ 0x4fa139c82d7e560bULL) * 2.0F - 1.0F);
            const float crack = smoothstep(0.72F, 0.93F, ridge);
            const float rock = 0.28F + broad * 0.25F + fine * 0.10F - crack * 0.18F;
            writePixel(
                textures.rock,
                x,
                y,
                rock * 1.02F,
                rock * 0.96F,
                rock * 0.86F);

            const float strands =
                0.5F + 0.5F * std::sin(
                    u * 2.0F * std::numbers::pi_v<float> * 14.0F +
                    broad * 5.0F);
            const float bark = 0.15F + broad * 0.20F + strands * 0.13F - fine * 0.06F;
            writePixel(
                textures.bark,
                x,
                y,
                bark * 1.18F,
                bark * 0.82F,
                bark * 0.58F);
        }
    }
    return textures;
}

}  // namespace natureview
