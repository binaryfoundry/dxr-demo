#pragma once

#include "../Context.hpp"

#include "../Allocator.hpp"

namespace d3d12
{
namespace raytracing
{
    void MakeAccelerationStructure(
        std::shared_ptr<d3d12::Context> context,
        ID3D12GraphicsCommandList4* command_list,
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs,
        D3D12MA::ResourcePtr& as_resource,
        UINT64* update_scratch_size = nullptr);
}
}
