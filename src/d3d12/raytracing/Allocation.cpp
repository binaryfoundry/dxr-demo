#include "Allocation.hpp"

namespace d3d12
{
namespace raytracing
{
    void MakeAccelerationStructure(
        std::shared_ptr<d3d12::Context> context,
        ID3D12GraphicsCommandList4* command_list,
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs,
        D3D12MA::ResourcePtr& as_resource,
        UINT64* update_scratch_size)
    {
        auto make_buffer = [&](UINT64 size, auto initial_state, D3D12MA::ResourcePtr& res)
        {
            auto resource_desc = BASIC_BUFFER_DESC;
            resource_desc.Width = size;
            resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

            D3D12MA::ALLOCATION_DESC allocation_desc = {};
            allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

            D3D12MA::Allocation* alloc = nullptr;
            context->allocator->CreateResource(
                &allocation_desc,
                &resource_desc,
                initial_state,
                NULL,
                &alloc,
                __uuidof(ID3D12Resource),
                nullptr);

            res.reset(alloc);
        };

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO pre_build_info;
        context->device->GetRaytracingAccelerationStructurePrebuildInfo(
            &inputs,
            &pre_build_info);

        if (update_scratch_size)
        {
            *update_scratch_size = pre_build_info.UpdateScratchDataSizeInBytes;
        }

        D3D12MA::ResourcePtr scratch_resource;

        make_buffer(
            pre_build_info.ScratchDataSizeInBytes,
            D3D12_RESOURCE_STATE_COMMON,
            scratch_resource);

        make_buffer(
            pre_build_info.ResultDataMaxSizeInBytes,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            as_resource);

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build_desc =
        {
            .DestAccelerationStructureData = as_resource.get()->GetResource()->GetGPUVirtualAddress(),
            .Inputs = inputs,
            .ScratchAccelerationStructureData = scratch_resource.get()->GetResource()->GetGPUVirtualAddress()
        };

        command_list->BuildRaytracingAccelerationStructure(
            &build_desc,
            0,
            nullptr);

        context->CurrentFrame().ReleaseWhenFrameComplete(std::move(scratch_resource));
    }
}
}