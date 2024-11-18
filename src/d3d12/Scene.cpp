#include "Scene.hpp"

#include "shaders/shader.fxh"

namespace d3d12
{
    constexpr UINT64 NUM_SHADER_IDS = 3;

    constexpr UINT NUM_INSTANCES = 3;

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

    Scene::Scene(
        std::shared_ptr<d3d12::Context> context) :
        context(context)
    {
    }

    void Scene::MoveToNextFrame()
    {
        tlas->MoveToNextFrame();
    }

    void Scene::Initialize()
    {
        InitMeshes();
        InitAccelerationStructure();

        InitRootSignature();
        InitPipeline();

        Resize();
    }

    void Scene::InitMeshes()
    {
        auto make_and_copy = [&](auto& data, D3D12MA::ResourcePtr& res) {
            auto desc = BASIC_BUFFER_DESC;
            desc.Width = sizeof(data);

            D3D12MA::ALLOCATION_DESC allocation_desc = {};
            allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

            D3D12MA::Allocation* alloc = nullptr;
            context->allocator->CreateResource(
                &allocation_desc,
                &desc,
                D3D12_RESOURCE_STATE_COMMON,
                NULL,
                &alloc,
                __uuidof(ID3D12Resource),
                nullptr);

            res.reset(alloc);

            void* ptr;
            res->GetResource()->Map(0, nullptr, &ptr);
            memcpy(ptr, data, sizeof(data));
            res->GetResource()->Unmap(0, nullptr);
        };

        make_and_copy(quad_vtx, quad_vb);
        make_and_copy(cube_vtx, cube_vb);
        make_and_copy(cube_idx, cube_ib);
    }

    void Scene::InitAccelerationStructure()
    {
        quad_blas = std::make_shared<raytracing::BottomStructure>(
            context,
            quad_vb->GetResource(),
            std::size(quad_vtx));

        cube_blas = std::make_shared<raytracing::BottomStructure>(
            context,
            cube_vb->GetResource(),
            std::size(cube_vtx),
            cube_ib->GetResource(),
            std::size(cube_idx));

        blas_init_list.push_back(quad_blas);
        blas_init_list.push_back(cube_blas);

        std::vector<std::shared_ptr<raytracing::BottomStructure>> instance_list;
        instance_list.push_back(cube_blas);
        instance_list.push_back(quad_blas);
        instance_list.push_back(quad_blas);

        tlas = std::make_shared<raytracing::TopStructure>(
            context, instance_list);
    }

    void Scene::InitRootSignature()
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

    void Scene::InitPipeline()
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

        D3D12MA::ALLOCATION_DESC allocation_desc = {};
        allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

        D3D12MA::Allocation* shader_ids_alloc = nullptr;
        context->allocator->CreateResource(
            &allocation_desc,
            &id_desc,
            D3D12_RESOURCE_STATE_COMMON,
            NULL,
            &shader_ids_alloc,
            __uuidof(ID3D12Resource),
            nullptr);

        shader_ids.reset(shader_ids_alloc);

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

        shader_ids->GetResource()->Map(0, nullptr, &data);
        write_id(L"RayGeneration");
        write_id(L"Miss");
        write_id(L"HitGroup");
        shader_ids->GetResource()->Unmap(0, nullptr);

        props->Release();
    }

    void Scene::Flush()
    {
        static UINT64 value = 1;
        context->command_queue->Signal(context->fence.Get(), value);
        context->fence->SetEventOnCompletion(value++, nullptr);
    }

    void Scene::Resize()
    {
        Flush();

        if (uav_heap.Get()) [[likely]]
        {
            uav_heap.Get()->Release();
        }

        ID3D12DescriptorHeap* new_uav_heap;

        D3D12_DESCRIPTOR_HEAP_DESC uav_heap_desc =
        {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = 1,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        };

        context->device->CreateDescriptorHeap(
            &uav_heap_desc,
            IID_PPV_ARGS(&new_uav_heap));

        uav_heap = new_uav_heap;

        if (render_target) [[likely]]
        {
            context->CurrentFrame().ReleaseWhenFrameComplete(std::move(render_target));
        }

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

        D3D12MA::ALLOCATION_DESC allocation_desc = {};
        allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        D3D12MA::Allocation* rt_alloc = nullptr;
        context->allocator->CreateResource(
            &allocation_desc,
            &rt_desc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            NULL,
            &rt_alloc,
            __uuidof(ID3D12Resource),
            nullptr);

        D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc =
        {
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D
        };

        context->device->CreateUnorderedAccessView(
            rt_alloc->GetResource(),
            nullptr,
            &uav_desc,
            uav_heap->GetCPUDescriptorHandleForHeapStart());

        render_target.reset(rt_alloc);
    }

    void Scene::Render()
    {
        context->command_list->SetPipelineState1(
            pso);

        context->command_list->SetComputeRootSignature(
            root_signature);

        const auto rt_desc = context->CurrentFrame().render_target->GetDesc();

        const D3D12_DISPATCH_RAYS_DESC dispatch_desc =
        {
            .RayGenerationShaderRecord =
            {
                .StartAddress = shader_ids->GetResource()->GetGPUVirtualAddress(),
                .SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
            },
            .MissShaderTable =
            {
                .StartAddress = shader_ids->GetResource()->GetGPUVirtualAddress() +
                                D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT,
                .SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
            },
            .HitGroupTable =
            {
                .StartAddress = shader_ids->GetResource()->GetGPUVirtualAddress() +
                                2 * D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT,
                .SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
            },
            .Width = static_cast<UINT>(rt_desc.Width),
            .Height = rt_desc.Height,
            .Depth = 1
        };

        {
            D3D12_RESOURCE_BARRIER uavBarrier = {};
            uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            uavBarrier.UAV.pResource = nullptr; // Applies to all UAV writes

            if (!blas_init_list.empty())
            {

                for (const auto& blas : blas_init_list)
                {
                    blas->Initialize();
                }

                context->command_list->ResourceBarrier(1, &uavBarrier);

                blas_init_list.clear();
            }

            tlas->Update();

            tlas->Render(uav_heap, dispatch_desc);
        }

        ID3D12Resource* back_buffer;
        context->swap_chain->GetBuffer(
            context->swap_chain->GetCurrentBackBufferIndex(),
            IID_PPV_ARGS(&back_buffer));

        auto barrier = [&](auto* resource, auto before, auto after)
        {
            const D3D12_RESOURCE_BARRIER rb =
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

        barrier(
            render_target->GetResource(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE);

        barrier(
            back_buffer,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_DEST);

        context->command_list->CopyResource(
            back_buffer,
            render_target->GetResource());

        barrier(
            back_buffer,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        barrier(
            render_target->GetResource(),
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        back_buffer->Release();
    }
}
