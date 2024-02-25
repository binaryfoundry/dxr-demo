#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>

namespace d3d12
{
    constexpr D3D12_HEAP_PROPERTIES UPLOAD_HEAP =
    {
        .Type = D3D12_HEAP_TYPE_UPLOAD
    };

    constexpr D3D12_HEAP_PROPERTIES DEFAULT_HEAP =
    {
        .Type = D3D12_HEAP_TYPE_DEFAULT
    };

    constexpr DXGI_SAMPLE_DESC NO_AA =
    {
        .Count = 1,
        .Quality = 0
    };

    struct Context
    {
        uint32_t width = 0;
        uint32_t height = 0;

        ID3D12Device5* device = nullptr;
        ID3D12CommandAllocator* cmdAlloc = nullptr;
        IDXGISwapChain3* swapChain = nullptr;
        ID3D12DescriptorHeap* uavHeap = nullptr;
        ID3D12CommandQueue* cmdQueue = nullptr;

        ID3D12Fence* fence = nullptr;
        ID3D12Resource* renderTarget = nullptr;

        void Flush()
        {
            static UINT64 value = 1;
            cmdQueue->Signal(fence, value);
            fence->SetEventOnCompletion(value++, nullptr);
        }
    };
}
