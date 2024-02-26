#pragma once

#include <vector>
#include <wrl.h>

#include "Allocator.hpp"
#include "Context.hpp"
#include "Descriptor.hpp"

using Microsoft::WRL::ComPtr;

namespace d3d12
{
    static const UINT RENDERER_FRAME_COUNT = 3;
    static const UINT FRAME_COUNT = RENDERER_FRAME_COUNT;

    struct Frame
    {
        ComPtr<ID3D12CommandAllocator> command_allocator_;

        ComPtr<ID3D12Resource> render_target_;
        DescriptorHandle rtv_handle_;

        std::vector<D3D12MA::ResourcePtr> resources_to_release_;
        std::vector<DescriptorHandle> handles_to_release_;

        UINT64 fence_value_ = 0;
    } frames_[FRAME_COUNT];
}
