#pragma once

#include <array>

#include <d3d12.h>
#include <dxgi1_4.h>

#include <wrl.h>

using Microsoft::WRL::ComPtr;

#include "Frame.hpp"
#include "Allocator.hpp"

namespace d3d12
{
    constexpr DXGI_SAMPLE_DESC NO_AA =
    {
        .Count = 1,
        .Quality = 0
    };

    constexpr D3D12_RESOURCE_DESC BASIC_BUFFER_DESC =
    {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Width = 0, // Will be changed in copies
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .SampleDesc = NO_AA,
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR
    };

    struct Context
    {
        size_t frame_index = 0;

        uint32_t width = 0;
        uint32_t height = 0;

        ComPtr<ID3D12Device5> device = nullptr;
        ComPtr<IDXGISwapChain3> swap_chain = nullptr;

        ComPtr<ID3D12CommandQueue> command_queue;

        ComPtr<ID3D12Fence> fence = nullptr;

        std::array<Frame, FRAME_COUNT> frames;

        D3D12MA::Allocator* allocator = nullptr;
        std::unique_ptr<DescriptorAllocator> descriptor_allocator;

        ComPtr<ID3D12GraphicsCommandList4> command_list;
        //ComPtr<ID3D12GraphicsCommandList4> command_list_4;

        Frame& CurrentFrame()
        {
            return frames[frame_index];
        }
    };
}
