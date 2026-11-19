#pragma once

#include <cstdint>
#include <string>

namespace natureview::platform {

struct MetalAppOptions {
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    std::uint32_t frameLimit = 0;
    double prewarmSeconds = 0.0;
    std::string capturePath;
};

int runMetalApp(const MetalAppOptions& options);

}  // namespace natureview::platform
