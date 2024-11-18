#include "TopStructure.hpp"

#include "Allocation.hpp"

namespace d3d12
{
namespace raytracing
{
    TopStructure::TopStructure(
        std::shared_ptr<d3d12::Context> context,
        const std::vector<std::shared_ptr<BottomStructure>>& instance_list) :
        context(context),
        instance_list(instance_list)
    {
    }

    TopStructure::~TopStructure()
    {
        for (UINT i = 0; i < FRAME_COUNT; i++)
        {
            frame_resources[i].constant_pool.pool.clear();
        }
    }

    CurrentFrameResources& TopStructure::FrameResources()
    {
        return frame_resources[context->frame_index];
    }

    void TopStructure::Initialize()
    {
        auto instances_desc = BASIC_BUFFER_DESC;
        instances_desc.Width = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instance_list.size();
        context->device->CreateCommittedResource(
            &UPLOAD_HEAP,
            D3D12_HEAP_FLAG_NONE,
            &instances_desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&instances));

        instances->Map(0, nullptr, reinterpret_cast<void**>(
            &instance_data));

        for (UINT i = 0; i < instance_list.size(); ++i)
        {
            instance_data[i] =
            {
                .InstanceID = i,
                .InstanceMask = 1,
                .AccelerationStructure = instance_list[i]->GetGPUVirtualAddress(),
            };
        }

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs =
        {
            .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
            .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE,
            .NumDescs = static_cast<UINT>(instance_list.size()),
            .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
            .InstanceDescs = instances->GetGPUVirtualAddress()
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
            context->device->CreateCommittedResource(
                &DEFAULT_HEAP,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&frame_resources[i].tlas_update_scratch));
        }

        D3D12_RESOURCE_BARRIER uavBarrier = {};
        uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        uavBarrier.UAV.pResource = nullptr; // Applies to all UAV writes

        context->command_list->ResourceBarrier(1, &uavBarrier);

        initialized = true;
    }

    void TopStructure::Update()
    {
        if (!initialized)
        {
            Initialize();
        }

        UpdateTransforms();

        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc =
        {
            .DestAccelerationStructureData = tlas->GetResource()->GetGPUVirtualAddress(),
            .Inputs =
            {
                .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
                .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE,
                .NumDescs = static_cast<UINT>(instance_list.size()),
                .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
                .InstanceDescs = instances->GetGPUVirtualAddress()
            },
            .SourceAccelerationStructureData = tlas->GetResource()->GetGPUVirtualAddress(),
            .ScratchAccelerationStructureData = FrameResources().tlas_update_scratch->GetGPUVirtualAddress(),
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

    static float time = 0.0f;

    void TopStructure::UpdateTransforms()
    {
        using namespace DirectX;
        auto set = [&](int idx, XMMATRIX mx)
            {
                auto* ptr = reinterpret_cast<XMFLOAT3X4*>(
                    &instance_data[idx].Transform);

                XMStoreFloat3x4(ptr, mx);
            };

        time += 0.01f;

        auto cube = XMMatrixRotationRollPitchYaw(time / 2, time / 3, time / 5);
        cube *= XMMatrixTranslation(-1.5, 2, 2);
        set(0, cube);

        auto mirror = XMMatrixRotationX(-1.8f);
        mirror *= XMMatrixRotationY(XMScalarSinEst(time) / 8 + 1);
        mirror *= XMMatrixTranslation(2, 2, 2);
        set(1, mirror);

        auto floor = XMMatrixScaling(5, 5, 5);
        floor *= XMMatrixTranslation(0, 0, 2);
        set(2, floor);
    }

    void TopStructure::Render(
        ComPtr<ID3D12DescriptorHeap>& uav_heap,
        const D3D12_DISPATCH_RAYS_DESC& dispatch_desc)
    {
        InstanceUniforms uniforms;

        DescriptorHandle* cbv0 = FrameResources().constant_pool.GetBuffer(
            context.get(),
            &uniforms);

        const D3D12_GPU_DESCRIPTOR_HANDLE table = context->gpu_descriptor_ring_buffer->StoreTableInit();

        context->gpu_descriptor_ring_buffer->StoreTableCBV(
            cbv0);

        context->command_list->SetComputeRootDescriptorTable(
            0,
            table);

        // TODO add to gpu_descriptor_ring_buffer table
        const auto uav_heap2 = uav_heap.Get();
        context->command_list->SetDescriptorHeaps(1, &uav_heap2);

        const auto uav_table = uav_heap2->GetGPUDescriptorHandleForHeapStart();

        context->command_list->SetComputeRootDescriptorTable(
            0, uav_table); // ?u0 ?t0

        context->command_list->SetComputeRootShaderResourceView(
            1,
            tlas->GetResource()->GetGPUVirtualAddress());

        context->command_list->DispatchRays(&dispatch_desc);
    }

    void TopStructure::MoveToNextFrame()
    {
        FrameResources().constant_pool.Reset();
    }

    D3D12_GPU_VIRTUAL_ADDRESS TopStructure::GetGPUVirtualAddress()
    {
        return tlas->GetResource()->GetGPUVirtualAddress();
    }
}
}
