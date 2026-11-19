#pragma once

#include <cstdint>

namespace natureview::platform {

struct MetalClearOptions {
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    std::uint32_t frameLimit = 0;
};

int runMetalClearApp(const MetalClearOptions& options);

}  // namespace natureview::platform
