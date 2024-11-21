#include "TopStructure.hpp"

#include "Allocation.hpp"
#include <stdexcept>

#include <DirectXMath.h>

namespace d3d12
{
namespace raytracing
{
    TopStructure::TopStructure(
        std::shared_ptr<d3d12::Context> context) :
        context(context)
    {
    }

    CurrentFrameResources& TopStructure::FrameResources()
    {
        return frame_resources[context->frame_index];
    }

    void TopStructure::Initialize(
        std::vector<std::shared_ptr<raytracing::BottomStructure>>& blas_list,
        EntityList& entities)
    {
        // TODO move internal to FrameResources
        for (size_t i = 0; i < FRAME_COUNT; i++)
        {
            auto resource_desc = BASIC_BUFFER_DESC;
            resource_desc.Width = sizeof(RaytracingUniforms);

            D3D12MA::ALLOCATION_DESC allocation_desc = {};
            allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

            D3D12MA::Allocation* cbv_alloc = nullptr;
            context->allocator->CreateResource(
                &allocation_desc,
                &resource_desc,
                D3D12_RESOURCE_STATE_COMMON,
                NULL,
                &cbv_alloc,
                __uuidof(ID3D12Resource),
                nullptr);

            frame_resources[i].constants.reset(cbv_alloc);
        }

        auto instances_desc = BASIC_BUFFER_DESC;
        instances_desc.Width = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * entities.size();

        D3D12MA::ALLOCATION_DESC allocation_desc = {};
        allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

        D3D12MA::Allocation* instances_alloc = nullptr;
        context->allocator->CreateResource(
            &allocation_desc,
            &instances_desc,
            D3D12_RESOURCE_STATE_COMMON,
            NULL,
            &instances_alloc,
            __uuidof(ID3D12Resource),
            nullptr);
        instances.reset(instances_alloc);

        instances->GetResource()->Map(0, nullptr, reinterpret_cast<void**>(
            &instance_data));

        for (UINT i = 0; i < entities.size(); ++i)
        {
            auto& entity = entities[i];
            auto& blas = blas_list[entity.instance_id];

            instance_data[i] =
            {
                .InstanceID = static_cast<UINT>(entity.instance_id),
                .InstanceMask = 1,
                .AccelerationStructure = blas->GetGPUVirtualAddress(),
            };
        }

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs =
        {
            .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
            .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE,
            .NumDescs = static_cast<UINT>(entities.size()),
            .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
            .InstanceDescs = instances->GetResource()->GetGPUVirtualAddress()
        };

        UINT64 update_scratch_size = 0;

        MakeAccelerationStructure(
            context,
            context->command_list.Get(),
            inputs,
            tlas,
            &update_scratch_size);

        for (size_t i = 0; i < FRAME_COUNT; i++)
        {
            auto desc = BASIC_BUFFER_DESC;
            desc.Width = update_scratch_size;
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

            D3D12MA::ALLOCATION_DESC allocation_desc = {};
            allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

            D3D12MA::Allocation* scratch_alloc = nullptr;
            context->allocator->CreateResource(
                &allocation_desc,
                &desc,
                D3D12_RESOURCE_STATE_COMMON,
                NULL,
                &scratch_alloc,
                __uuidof(ID3D12Resource),
                nullptr);
            frame_resources[i].tlas_update_scratch.reset(scratch_alloc);
        }

        D3D12_RESOURCE_BARRIER uavBarrier = {};
        uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        uavBarrier.UAV.pResource = nullptr; // Applies to all UAV writes

        context->command_list->ResourceBarrier(1, &uavBarrier);
    }

    void TopStructure::Update(
        EntityList& entities)
    {
        using namespace DirectX;
        auto set = [&](int idx, XMMATRIX mx)
        {
            auto* ptr = reinterpret_cast<XMFLOAT3X4*>(
                &instance_data[idx].Transform);

            XMStoreFloat3x4(ptr, mx);
        };

        for (UINT i = 0; i < entities.size(); i++)
        {
            const auto& entity = entities[i];

            // TODO Just use GLM
            auto transform = XMMatrixScaling(
                entity.scale.x,
                entity.scale.y,
                entity.scale.z);

            // TODO set rotation

            transform *= XMMatrixTranslation(
                entity.position.x,
                entity.position.y,
                entity.position.z);

            set(i, transform);
        }

        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc =
        {
            .DestAccelerationStructureData = tlas->GetResource()->GetGPUVirtualAddress(),
            .Inputs =
            {
                .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
                .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE,
                .NumDescs = static_cast<UINT>(entities.size()),
                .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
                .InstanceDescs = instances->GetResource()->GetGPUVirtualAddress()
            },
            .SourceAccelerationStructureData = tlas->GetResource()->GetGPUVirtualAddress(),
            .ScratchAccelerationStructureData = FrameResources().tlas_update_scratch->GetResource()->GetGPUVirtualAddress(),
        };

        context->command_list->BuildRaytracingAccelerationStructure(
            &desc,
            0,
            nullptr);

        const D3D12_RESOURCE_BARRIER barrier =
        {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
            .UAV =
            {
                .pResource = tlas->GetResource()
            }
        };

        context->command_list->ResourceBarrier(1, &barrier);
    }

    void TopStructure::Render(
        Camera& camera,
        EntityList& entities,
        ComPtr<ID3D12DescriptorHeap>& uav_heap,
        const D3D12_DISPATCH_RAYS_DESC& dispatch_desc)
    {
        using namespace DirectX;

        Update(entities);

        RaytracingUniforms uniforms;
        uniforms.Position = glm::vec4(camera.Position(), 1.0);
        uniforms.View = camera.View();
        uniforms.Aspect = camera.Aspect();
        uniforms.TMin = camera.Near();
        uniforms.TMax = camera.Far();
        uniforms.Zoom = camera.Zoom();

        D3D12MA::ResourcePtr& constant_buffer = FrameResources().constants;

        if (!constant_buffer || !constant_buffer->GetResource())
        {
            throw std::runtime_error("Invalid constant buffer resource.");
        }

        void* mapped_ptr = nullptr;
        HRESULT hr = constant_buffer->GetResource()->Map(0, nullptr, &mapped_ptr);
        if (FAILED(hr))
        {
            throw std::runtime_error("Failed to map constant buffer.");
        }
        memcpy(mapped_ptr, &uniforms, sizeof(uniforms));
        constant_buffer->GetResource()->Unmap(0, nullptr);

        context->command_list->SetComputeRootConstantBufferView(
            0,
            constant_buffer->GetResource()->GetGPUVirtualAddress());

        const auto uav_descriptor_heap = uav_heap.Get();
        if (!uav_descriptor_heap)
        {
            throw std::runtime_error("Invalid UAV descriptor heap.");
        }
        context->command_list->SetDescriptorHeaps(
            1, &uav_descriptor_heap);

        const auto uav_descriptor_table =
            uav_descriptor_heap->GetGPUDescriptorHandleForHeapStart();

        context->command_list->SetComputeRootDescriptorTable(
            1, uav_descriptor_table);

        context->command_list->SetComputeRootShaderResourceView(
            2,
            tlas->GetResource()->GetGPUVirtualAddress());

        context->command_list->DispatchRays(&dispatch_desc);
    }

    D3D12_GPU_VIRTUAL_ADDRESS TopStructure::GetGPUVirtualAddress()
    {
        return tlas->GetResource()->GetGPUVirtualAddress();
    }
}
}
