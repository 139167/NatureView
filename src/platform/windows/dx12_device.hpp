#pragma once

#include <cstdint>
#include <string>

#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

namespace natureview::platform {

class Dx12Device {
public:
    Dx12Device();

    Dx12Device(const Dx12Device&) = delete;
    Dx12Device& operator=(const Dx12Device&) = delete;
    Dx12Device(Dx12Device&&) = default;
    Dx12Device& operator=(Dx12Device&&) = default;

    [[nodiscard]] ID3D12Device* device() const {
        return device_.Get();
    }

    [[nodiscard]] IDXGIAdapter1* adapter() const {
        return adapter_.Get();
    }

    [[nodiscard]] const std::string& adapterName() const {
        return adapterName_;
    }

    [[nodiscard]] std::uint64_t dedicatedVideoMemoryBytes() const {
        return dedicatedVideoMemoryBytes_;
    }

    [[nodiscard]] bool usesWarp() const {
        return usesWarp_;
    }

private:
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory_;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    std::string adapterName_;
    std::uint64_t dedicatedVideoMemoryBytes_ = 0;
    bool usesWarp_ = false;
};

}  // namespace natureview::platform
