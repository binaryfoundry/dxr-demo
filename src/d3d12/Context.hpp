#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>

namespace d3d12
{
    struct Context
    {
        uint32_t width = 0;
        uint32_t height = 0;

        ID3D12Device5* device = nullptr;
        ID3D12CommandAllocator* cmdAlloc = nullptr;
        IDXGISwapChain3* swapChain = nullptr;
    };
}
