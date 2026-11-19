#include <cstdlib>
#include <exception>
#include <iostream>

#include "dx12_device.hpp"

int main() {
    try {
        const natureview::platform::Dx12Device device;
        std::cout << "adapter=" << device.adapterName() << '\n'
                  << "uses_warp=" << (device.usesWarp() ? "true" : "false") << '\n'
                  << "dedicated_video_memory_bytes="
                  << device.dedicatedVideoMemoryBytes() << '\n'
                  << "feature_level=11_0\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "D3D12 initialization failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
