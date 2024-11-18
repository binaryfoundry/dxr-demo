#pragma once

#include <vector>

#include "Allocator.hpp"
#include "Descriptor.hpp"

#include <d3d12.h>
#include <dxgi1_4.h>

#include <wrl.h>

using Microsoft::WRL::ComPtr;

namespace d3d12
{
    static constexpr UINT RENDERER_FRAME_COUNT = 3;
    static constexpr UINT FRAME_COUNT = RENDERER_FRAME_COUNT;

    class Frame
    {
    private:
        std::vector<D3D12MA::ResourcePtr> resources_to_release;
        std::vector<DescriptorHandle> handles_to_release;

    public:
        ComPtr<ID3D12CommandAllocator> command_allocator;

        ComPtr<ID3D12Resource> render_target;
        DescriptorHandle rtv_handle;

        UINT64 fence_value = 0;

        void ReleaseWhenFrameComplete(D3D12MA::ResourcePtr&& resource)
        {
            resources_to_release.emplace_back(std::move(resource));
        }

        void ReleaseWhenFrameComplete(DescriptorHandle&& handle)
        {
            handles_to_release.emplace_back(std::move(handle));
        }

        void ReleaseFrameComplete()
        {
            resources_to_release.clear();
            resources_to_release.clear();
        }
    };
}
