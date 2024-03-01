#include "Raytracing.hpp"

#include <DirectXMath.h>

#include "shaders/shader.fxh"

namespace d3d12
{
    constexpr UINT NUM_INSTANCES = 3;
    constexpr UINT64 NUM_SHADER_IDS = 3;

    constexpr float quad_vtx[] =
    {
        -1, 0, -1, -1, 0,  1, 1, 0, 1,
        -1, 0, -1,  1, 0, -1, 1, 0, 1
    };

    constexpr float cube_vtx[] =
    {
        -1, -1, -1, 1, -1, -1, -1, 1, -1, 1, 1, -1,
        -1, -1,  1, 1, -1,  1, -1, 1,  1, 1, 1,  1
    };

    constexpr short cube_idx[] =
    {
        4, 6, 0, 2, 0, 6, 0, 1, 4, 5, 4, 1,
        0, 2, 1, 3, 1, 2, 1, 3, 5, 7, 5, 3,
        2, 6, 3, 7, 3, 6, 4, 5, 6, 7, 6, 5
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

    Raytracing::Raytracing(
        std::shared_ptr<d3d12::Context> context) :
        context(context)
    {
    }

    Raytracing::~Raytracing()
    {
    }

    void Raytracing::Initialize()
    {
        InitMeshes();
        InitBottomLevel();
        InitScene();
        InitTopLevel();
        InitRootSignature();
        InitPipeline();

        Resize();
    }

    void Raytracing::InitMeshes()
    {
        auto make_and_copy = [&](auto& data) {
            auto desc = BASIC_BUFFER_DESC;
            desc.Width = sizeof(data);
            ID3D12Resource* res;
            context->device->CreateCommittedResource(
                &UPLOAD_HEAP,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&res));

            void* ptr;
            res->Map(0, nullptr, &ptr);
            memcpy(ptr, data, sizeof(data));
            res->Unmap(0, nullptr);
            return res;
        };

        quad_vb = make_and_copy(quad_vtx);
        cube_vb = make_and_copy(cube_vtx);
        cube_ib = make_and_copy(cube_idx);
    }

    ID3D12Resource* Raytracing::MakeAccelerationStructure(
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs,
        UINT64* update_scratch_size)
    {
        auto make_buffer = [&](UINT64 size, auto initial_state)
        {
            auto desc = BASIC_BUFFER_DESC;
            desc.Width = size;
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

            ID3D12Resource* buffer;
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

        context->frames[0].command_allocator->Reset();

        ID3D12GraphicsCommandList4* command_list;

        context->device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            context->frames[0].command_allocator.Get(),
            nullptr,
            IID_PPV_ARGS(&command_list));

        command_list->Close();

        command_list->Reset(
            context->frames[0].command_allocator.Get(),
            nullptr);

        command_list->BuildRaytracingAccelerationStructure(
            &build_desc,
            0,
            nullptr);

        command_list->Close();

        context->command_queue->ExecuteCommandLists(
            1, reinterpret_cast<ID3D12CommandList**>(&command_list));

        Flush();

        scratch->Release();
        return as;
    }

    void Raytracing::InitScene()
    {
        for (UINT n = 0; n < FRAME_COUNT; n++)
        {
            auto instances_desc = BASIC_BUFFER_DESC;
            instances_desc.Width = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * NUM_INSTANCES;
            context->device->CreateCommittedResource(
                &UPLOAD_HEAP,
                D3D12_HEAP_FLAG_NONE,
                &instances_desc,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&frame_resources[n].instances));

            frame_resources[n].instances->Map(0, nullptr, reinterpret_cast<void**>(
                &frame_resources[n].instance_data));

            for (UINT i = 0; i < NUM_INSTANCES; ++i)
            {
                frame_resources[n].instance_data[i] =
                {
                    .InstanceID = i,
                    .InstanceMask = 1,
                    .AccelerationStructure = (i ? quad_blas : cube_blas)->GetGPUVirtualAddress(),
                };
            }
        }
    }

    void Raytracing::InitTopLevel()
    {
        for (UINT n = 0; n < FRAME_COUNT; n++)
        {
            UINT64 update_scratch_size;
            frame_resources[n].tlas = MakeTLAS(
                frame_resources[n].instances.Get(),
                NUM_INSTANCES,
                &update_scratch_size);

            auto desc = BASIC_BUFFER_DESC;
            desc.Width = update_scratch_size;
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            context->device->CreateCommittedResource(
                &DEFAULT_HEAP,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&frame_resources[n].tlas_update_scratch));
        }
    }

    void Raytracing::InitBottomLevel()
    {
        quad_blas = MakeBLAS(
            quad_vb,
            std::size(quad_vtx));

        cube_blas = MakeBLAS(
            cube_vb,
            std::size(cube_vtx),
            cube_ib,
            std::size(cube_idx));
    }

    void Raytracing::InitRootSignature()
    {
        D3D12_DESCRIPTOR_RANGE uav_range =
        {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
            .NumDescriptors = 1,
        };

        D3D12_ROOT_PARAMETER params[] =
        {
            {
                .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                .DescriptorTable =
                {
                    .NumDescriptorRanges = 1,
                    .pDescriptorRanges = &uav_range
                }
            },

            {
                .ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
                .Descriptor =
                {
                    .ShaderRegister = 0,
                    .RegisterSpace = 0
                }
            }
        };

        D3D12_ROOT_SIGNATURE_DESC desc =
        {
            .NumParameters = std::size(params),
            .pParameters = params
        };

        ID3DBlob* blob;
        D3D12SerializeRootSignature(
            &desc,
            D3D_ROOT_SIGNATURE_VERSION_1_0,
            &blob,
            nullptr);

        context->device->CreateRootSignature(
            0,
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            IID_PPV_ARGS(&root_signature));

        blob->Release();
    }

    void Raytracing::InitPipeline()
    {
        D3D12_DXIL_LIBRARY_DESC lib =
        {
            .DXILLibrary =
            {
                .pShaderBytecode = compiledShader,
                .BytecodeLength = std::size(compiledShader)
            }
        };

        D3D12_HIT_GROUP_DESC hit_group =
        {
            .HitGroupExport = L"HitGroup",
            .Type = D3D12_HIT_GROUP_TYPE_TRIANGLES,
            .ClosestHitShaderImport = L"ClosestHit"
        };

        D3D12_RAYTRACING_SHADER_CONFIG shader_cfg =
        {
            .MaxPayloadSizeInBytes = 20,
            .MaxAttributeSizeInBytes = 8,
        };

        D3D12_GLOBAL_ROOT_SIGNATURE global_sig =
        {
            root_signature
        };

        D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_cfg =
        {
            .MaxTraceRecursionDepth = 3
        };

        D3D12_STATE_SUBOBJECT subobjects[] =
        {
            {
                .Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY,
                .pDesc = &lib
            },
            {
                .Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP,
                .pDesc = &hit_group
            },
            {
                .Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG,
                .pDesc = &shader_cfg
            },
            {
                .Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE,
                .pDesc = &global_sig
            },
            {
                .Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG,
                .pDesc = &pipeline_cfg
            }
        };

        D3D12_STATE_OBJECT_DESC desc =
        {
            .Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
            .NumSubobjects = std::size(subobjects),
            .pSubobjects = subobjects
        };

        context->device->CreateStateObject(&desc, IID_PPV_ARGS(&pso));

        auto id_desc = BASIC_BUFFER_DESC;
        id_desc.Width = NUM_SHADER_IDS * D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
        context->device->CreateCommittedResource(
            &UPLOAD_HEAP,
            D3D12_HEAP_FLAG_NONE,
            &id_desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&shader_ids));

        ID3D12StateObjectProperties* props;
        pso->QueryInterface(&props);

        void* data;
        auto write_id = [&](const wchar_t* name)
        {
            void* id = props->GetShaderIdentifier(name);
            memcpy(data, id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
            data = static_cast<char*>(data) +
                D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
        };

        shader_ids->Map(0, nullptr, &data);
        write_id(L"RayGeneration");
        write_id(L"Miss");
        write_id(L"HitGroup");
        shader_ids->Unmap(0, nullptr);

        props->Release();
    }

    ID3D12Resource* Raytracing::MakeBLAS(
        ID3D12Resource* vertex_buffer,
        UINT vertex_floats,
        ID3D12Resource* index_buffer,
        UINT indices)
    {
        D3D12_RAYTRACING_GEOMETRY_DESC geometry_desc =
        {
            .Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
            .Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,

            .Triangles =
            {
                .Transform3x4 = 0,

                .IndexFormat = index_buffer ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_UNKNOWN,
                .VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT,
                .IndexCount = indices,
                .VertexCount = vertex_floats / 3,
                .IndexBuffer = index_buffer ? index_buffer->GetGPUVirtualAddress() : 0,
                .VertexBuffer =
                {
                    .StartAddress = vertex_buffer->GetGPUVirtualAddress(),
                    .StrideInBytes = sizeof(float) * 3
                }
            }
        };

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs =
        {
            .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
            .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
            .NumDescs = 1,
            .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
            .pGeometryDescs = &geometry_desc
        };

        return MakeAccelerationStructure(inputs);
    }

    ID3D12Resource* Raytracing::MakeTLAS(
        ID3D12Resource* instances,
        UINT num_instances,
        UINT64* update_scratch_size)
    {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs =
        {
            .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
            .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE,
            .NumDescs = num_instances,
            .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
            .InstanceDescs = instances->GetGPUVirtualAddress()
        };

        return MakeAccelerationStructure(inputs, update_scratch_size);
    }

    void Raytracing::Flush()
    {
        static UINT64 value = 1;
        context->command_queue->Signal(context->fence.Get(), value);
        context->fence->SetEventOnCompletion(value++, nullptr);
    }

    void Raytracing::Resize()
    {
        Flush();

        for (UINT i = 0; i < FRAME_COUNT; i++)
        {
            if (frame_resources[i].uav_heap.Get()) [[likely]]
            {
                frame_resources[i].uav_heap.Get()->Release();
            }

            ID3D12DescriptorHeap* uav_heap;

            D3D12_DESCRIPTOR_HEAP_DESC uav_heap_desc =
            {
                .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                .NumDescriptors = 1,
                .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
            };

            context->device->CreateDescriptorHeap(
                &uav_heap_desc,
                IID_PPV_ARGS(&uav_heap));

            frame_resources[i].uav_heap = uav_heap;

            if (frame_resources[i].render_target.Get()) [[likely]]
            {
                frame_resources[i].render_target.Get()->Release();
            }

            ID3D12Resource* render_target;

            D3D12_RESOURCE_DESC rt_desc =
            {
                .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                .Width = context->width,
                .Height = context->height,
                .DepthOrArraySize = 1,
                .MipLevels = 1,
                .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                .SampleDesc = NO_AA,
                .Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
            };

            context->device->CreateCommittedResource(
                &DEFAULT_HEAP,
                D3D12_HEAP_FLAG_NONE,
                &rt_desc,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                nullptr,
                IID_PPV_ARGS(&render_target));

            D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc =
            {
                .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D
            };

            context->device->CreateUnorderedAccessView(
                render_target,
                nullptr,
                &uav_desc,
                uav_heap->GetCPUDescriptorHandleForHeapStart());

            frame_resources[i].render_target = render_target;
        }
    }

    static float time = 0.0f;

    void Raytracing::UpdateTransforms()
    {
        using namespace DirectX;
        auto set = [&](int idx, XMMATRIX mx)
        {
            auto* ptr = reinterpret_cast<XMFLOAT3X4*>(
                &FrameResources().instance_data[idx].Transform);

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

    void Raytracing::UpdateScene()
    {
        UpdateTransforms();

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc =
        {
            .DestAccelerationStructureData = FrameResources().tlas->GetGPUVirtualAddress(),
            .Inputs =
            {
                .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
                .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE,
                .NumDescs = NUM_INSTANCES,
                .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
                .InstanceDescs = FrameResources().instances->GetGPUVirtualAddress()
            },
            .SourceAccelerationStructureData = FrameResources().tlas->GetGPUVirtualAddress(),
            .ScratchAccelerationStructureData = FrameResources().tlas_update_scratch->GetGPUVirtualAddress(),
        };

        context->command_list->BuildRaytracingAccelerationStructure(
            &desc,
            0,
            nullptr);

        D3D12_RESOURCE_BARRIER barrier =
        {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
            .UAV =
            {
                .pResource = FrameResources().tlas.Get()
            }
        };

        context->command_list->ResourceBarrier(1, &barrier);
    }

    void Raytracing::Render()
    {
        UpdateScene();

        context->command_list->SetPipelineState1(
            pso);

        context->command_list->SetComputeRootSignature(
            root_signature);

        ID3D12DescriptorHeap* uav_heap = FrameResources().uav_heap.Get();

        context->command_list->SetDescriptorHeaps(1, &uav_heap);

        auto uav_table = uav_heap->GetGPUDescriptorHandleForHeapStart();

        context->command_list->SetComputeRootDescriptorTable(
            0, uav_table); // ?u0 ?t0

        context->command_list->SetComputeRootShaderResourceView(
            1,
            FrameResources().tlas->GetGPUVirtualAddress());

        auto rt_desc = context->CurrentFrame().render_target->GetDesc();

        D3D12_DISPATCH_RAYS_DESC dispatch_desc =
        {
            .RayGenerationShaderRecord =
            {
                .StartAddress = shader_ids->GetGPUVirtualAddress(),
                .SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
            },
            .MissShaderTable =
            {
                .StartAddress = shader_ids->GetGPUVirtualAddress() +
                                D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT,
                .SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
            },
            .HitGroupTable =
            {
                .StartAddress = shader_ids->GetGPUVirtualAddress() +
                                2 * D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT,
                .SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
            },
            .Width = static_cast<UINT>(rt_desc.Width),
            .Height = rt_desc.Height,
            .Depth = 1
        };

        context->command_list->DispatchRays(&dispatch_desc);

        ID3D12Resource* back_buffer;
        context->swap_chain->GetBuffer(
            context->swap_chain->GetCurrentBackBufferIndex(),
            IID_PPV_ARGS(&back_buffer));

        auto barrier = [&](auto* resource, auto before, auto after)
        {
            D3D12_RESOURCE_BARRIER rb =
            {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                .Transition =
                {
                    .pResource = resource,
                    .StateBefore = before,
                    .StateAfter = after
                },
            };
            context->command_list->ResourceBarrier(1, &rb);
        };

        auto* render_target = FrameResources().render_target.Get();

        barrier(
            render_target,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE);

        barrier(
            back_buffer,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_DEST);

        context->command_list->CopyResource(
            back_buffer,
            render_target);

        barrier(
            back_buffer,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        barrier(
            render_target,
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        back_buffer->Release();
    }

}
