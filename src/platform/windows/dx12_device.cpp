#include "dx12_device.hpp"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace natureview::platform {
namespace {

std::runtime_error hresultError(const char* operation, HRESULT result) {
    std::ostringstream message;
    message << operation << " failed (HRESULT 0x" << std::hex
            << static_cast<unsigned long>(result) << ')';
    return std::runtime_error(message.str());
}

std::string utf8AdapterName(const DXGI_ADAPTER_DESC1& description) {
    const int requiredBytes = WideCharToMultiByte(
        CP_UTF8,
        0,
        description.Description,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr);
    if (requiredBytes <= 1) {
        return "Unknown D3D12 adapter";
    }

    std::string name(static_cast<std::size_t>(requiredBytes), '\0');
    const int writtenBytes = WideCharToMultiByte(
        CP_UTF8,
        0,
        description.Description,
        -1,
        name.data(),
        requiredBytes,
        nullptr,
        nullptr);
    if (writtenBytes <= 1) {
        return "Unknown D3D12 adapter";
    }
    name.resize(static_cast<std::size_t>(writtenBytes - 1));
    return name;
}

Microsoft::WRL::ComPtr<ID3D12Device> createDevice(
    IDXGIAdapter1* adapter,
    HRESULT& result) {
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    result = D3D12CreateDevice(
        adapter,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(device.ReleaseAndGetAddressOf()));
    return device;
}

}  // namespace

Dx12Device::Dx12Device() {
    HRESULT result =
        CreateDXGIFactory1(IID_PPV_ARGS(factory_.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        throw hresultError("CreateDXGIFactory1", result);
    }

    for (UINT index = 0;; ++index) {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> candidate;
        result = factory_->EnumAdapters1(index, candidate.ReleaseAndGetAddressOf());
        if (result == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        if (FAILED(result)) {
            throw hresultError("IDXGIFactory4::EnumAdapters1", result);
        }

        DXGI_ADAPTER_DESC1 description{};
        result = candidate->GetDesc1(&description);
        if (FAILED(result)) {
            throw hresultError("IDXGIAdapter1::GetDesc1", result);
        }
        if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
            continue;
        }

        device_ = createDevice(candidate.Get(), result);
        if (SUCCEEDED(result)) {
            adapter_ = candidate;
            adapterName_ = utf8AdapterName(description);
            dedicatedVideoMemoryBytes_ = description.DedicatedVideoMemory;
            usesWarp_ = false;
            return;
        }
    }

    result = factory_->EnumWarpAdapter(
        IID_PPV_ARGS(adapter_.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        throw hresultError("IDXGIFactory4::EnumWarpAdapter", result);
    }

    device_ = createDevice(adapter_.Get(), result);
    if (FAILED(result)) {
        throw hresultError("D3D12CreateDevice(WARP)", result);
    }

    DXGI_ADAPTER_DESC1 description{};
    result = adapter_->GetDesc1(&description);
    if (FAILED(result)) {
        throw hresultError("IDXGIAdapter1::GetDesc1(WARP)", result);
    }
    adapterName_ = utf8AdapterName(description);
    dedicatedVideoMemoryBytes_ = description.DedicatedVideoMemory;
    usesWarp_ = true;
}

}  // namespace natureview::platform
