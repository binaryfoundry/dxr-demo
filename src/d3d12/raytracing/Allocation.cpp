#include "Allocation.hpp"

#include "../Allocator.hpp"

namespace d3d12
{
namespace raytracing
{
    ID3D12Resource* MakeAccelerationStructure(
        std::shared_ptr<d3d12::Context> context,
        ID3D12GraphicsCommandList4* command_list,
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs,
        UINT64* update_scratch_size)
    {
        auto make_buffer = [&](UINT64 size, auto initial_state)
        {
            auto desc = BASIC_BUFFER_DESC;
            desc.Width = size;
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

            ID3D12Resource* buffer;
            // TODO use custom allocator
            //context->allocator->CreateResource(
            context->device->CreateCommittedResource(
                &DEFAULT_HEAP,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                initial_state,
                nullptr,
                IID_PPV_ARGS(&buffer));

            return buffer;
        };

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO pre_build_info;
        context->device->GetRaytracingAccelerationStructurePrebuildInfo(
            &inputs,
            &pre_build_info);

        if (update_scratch_size)
        {
            *update_scratch_size = pre_build_info.UpdateScratchDataSizeInBytes;
        }

        auto* scratch = make_buffer(
            pre_build_info.ScratchDataSizeInBytes,
            D3D12_RESOURCE_STATE_COMMON);

        auto* as = make_buffer(
            pre_build_info.ResultDataMaxSizeInBytes,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build_desc =
        {
            .DestAccelerationStructureData = as->GetGPUVirtualAddress(),
            .Inputs = inputs,
            .ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress()
        };

        command_list->BuildRaytracingAccelerationStructure(
            &build_desc,
            0,
            nullptr);

        // TODO Add to release list
        //scratch->Release();
        //context->CurrentFrame().handles_to_release.push_back(scratch);

        return as;
    }
}
}