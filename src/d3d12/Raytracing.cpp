#include "Raytracing.hpp"

#include "shaders/shader.fxh"

namespace d3d12
{
    constexpr UINT64 NUM_SHADER_IDS = 3;

    Raytracing::Raytracing(
        std::shared_ptr<d3d12::Context> context) :
        context(context)
    {
    }

    void Raytracing::MoveToNextFrame()
    {
        scene->MoveToNextFrame();
    }

    void Raytracing::Initialize()
    {
        scene = std::make_unique<raytracing::Scene>(context);

        InitRootSignature();
        InitPipeline();

        Resize();
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

    void Raytracing::Flush()
    {
        static UINT64 value = 1;
        context->command_queue->Signal(context->fence.Get(), value);
        context->fence->SetEventOnCompletion(value++, nullptr);
    }

    void Raytracing::Resize()
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

        if (render_target.Get()) [[likely]]
        {
            render_target.Get()->Release();
        }

        ID3D12Resource* new_render_target;

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
            IID_PPV_ARGS(&new_render_target));

        D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc =
        {
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D
        };

        context->device->CreateUnorderedAccessView(
            new_render_target,
            nullptr,
            &uav_desc,
            uav_heap->GetCPUDescriptorHandleForHeapStart());

        render_target = new_render_target;
    }

    void Raytracing::Render()
    {
        scene->Update();

        context->command_list->SetPipelineState1(
            pso);

        context->command_list->SetComputeRootSignature(
            root_signature);

        const auto rt_desc = context->CurrentFrame().render_target->GetDesc();

        const D3D12_DISPATCH_RAYS_DESC dispatch_desc =
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

        scene->Render(uav_heap, dispatch_desc);

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
            render_target.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE);

        barrier(
            back_buffer,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_DEST);

        context->command_list->CopyResource(
            back_buffer,
            render_target.Get());

        barrier(
            back_buffer,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        barrier(
            render_target.Get(),
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        back_buffer->Release();
    }
}
