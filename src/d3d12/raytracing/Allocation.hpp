#pragma once

#include "../Context.hpp"

namespace d3d12
{
namespace raytracing
{
    ID3D12Resource* MakeAccelerationStructure(
        std::shared_ptr<d3d12::Context> context,
        ID3D12GraphicsCommandList4* command_list,
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs,
        UINT64* update_scratch_size = nullptr);
}
}
