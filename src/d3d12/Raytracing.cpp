#include "Raytracing.hpp"

#include <DirectXMath.h>

#include "shaders/shader.fxh"

namespace d3d12
{
    constexpr UINT NUM_INSTANCES = 3;
    constexpr UINT64 NUM_SHADER_IDS = 3;

    constexpr float quadVtx[] =
    {
        -1, 0, -1, -1, 0,  1, 1, 0, 1,
        -1, 0, -1,  1, 0, -1, 1, 0, 1
    };

    constexpr float cubeVtx[] =
    {
        -1, -1, -1, 1, -1, -1, -1, 1, -1, 1, 1, -1,
        -1, -1,  1, 1, -1,  1, -1, 1,  1, 1, 1,  1
    };

    constexpr short cubeIdx[] =
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
        InitCommand();
        InitMeshes();
        InitBottomLevel();
        InitScene();
        InitTopLevel();
        InitRootSignature();
        InitPipeline();
    }

    void Raytracing::InitCommand()
    {
        context->device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&context->cmdAlloc));

        context->device->CreateCommandList1(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            D3D12_COMMAND_LIST_FLAG_NONE,
            IID_PPV_ARGS(&cmdList));
    }

    void Raytracing::InitMeshes()
    {
        auto makeAndCopy = [&](auto& data) {
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

        quadVB = makeAndCopy(quadVtx);
        cubeVB = makeAndCopy(cubeVtx);
        cubeIB = makeAndCopy(cubeIdx);
    }

    ID3D12Resource* Raytracing::MakeAccelerationStructure(
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs,
        UINT64* updateScratchSize)
    {
        auto makeBuffer = [&](UINT64 size, auto initialState)
        {
            auto desc = BASIC_BUFFER_DESC;
            desc.Width = size;
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

            ID3D12Resource* buffer;
            context->device->CreateCommittedResource(
                &DEFAULT_HEAP,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                initialState,
                nullptr,
                IID_PPV_ARGS(&buffer));

            return buffer;
        };

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo;
        context->device->GetRaytracingAccelerationStructurePrebuildInfo(
            &inputs,
            &prebuildInfo);

        if (updateScratchSize)
        {
            *updateScratchSize = prebuildInfo.UpdateScratchDataSizeInBytes;
        }

        auto* scratch = makeBuffer(
            prebuildInfo.ScratchDataSizeInBytes,
            D3D12_RESOURCE_STATE_COMMON);

        auto* as = makeBuffer(
            prebuildInfo.ResultDataMaxSizeInBytes,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc =
        {
            .DestAccelerationStructureData = as->GetGPUVirtualAddress(),
            .Inputs = inputs,
            .ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress()
        };

        context->cmdAlloc->Reset();

        cmdList->Reset(
            context->cmdAlloc,
            nullptr);

        cmdList->BuildRaytracingAccelerationStructure(
            &buildDesc,
            0,
            nullptr);

        cmdList->Close();

        context->cmdQueue->ExecuteCommandLists(
            1, reinterpret_cast<ID3D12CommandList**>(&cmdList));

        context->Flush();

        scratch->Release();
        return as;
    }

    void Raytracing::InitScene()
    {
        auto instancesDesc = BASIC_BUFFER_DESC;
        instancesDesc.Width = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * NUM_INSTANCES;
        context->device->CreateCommittedResource(
            &UPLOAD_HEAP,
            D3D12_HEAP_FLAG_NONE,
            &instancesDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&instances));
        instances->Map(0, nullptr, reinterpret_cast<void**>(&instanceData));

        for (UINT i = 0; i < NUM_INSTANCES; ++i)
        {
            instanceData[i] =
            {
                .InstanceID = i,
                .InstanceMask = 1,
                .AccelerationStructure = (i ? quadBlas : cubeBlas)->GetGPUVirtualAddress(),
            };
        }

        UpdateTransforms();
    }

    void Raytracing::InitTopLevel()
    {
        UINT64 updateScratchSize;
        tlas = MakeTLAS(
            instances,
            NUM_INSTANCES,
            &updateScratchSize);

        auto desc = BASIC_BUFFER_DESC;
        desc.Width = updateScratchSize;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        context->device->CreateCommittedResource(
            &DEFAULT_HEAP,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&tlasUpdateScratch));
    }

    void Raytracing::InitBottomLevel()
    {
        quadBlas = MakeBLAS(
            quadVB,
            std::size(quadVtx));

        cubeBlas = MakeBLAS(
            cubeVB,
            std::size(cubeVtx),
            cubeIB,
            std::size(cubeIdx));
    }

    void Raytracing::InitRootSignature()
    {
        D3D12_DESCRIPTOR_RANGE uavRange =
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
                    .pDescriptorRanges = &uavRange
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
            IID_PPV_ARGS(&rootSignature));

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

        D3D12_HIT_GROUP_DESC hitGroup =
        {
            .HitGroupExport = L"HitGroup",
            .Type = D3D12_HIT_GROUP_TYPE_TRIANGLES,
            .ClosestHitShaderImport = L"ClosestHit"
        };

        D3D12_RAYTRACING_SHADER_CONFIG shaderCfg =
        {
            .MaxPayloadSizeInBytes = 20,
            .MaxAttributeSizeInBytes = 8,
        };

        D3D12_GLOBAL_ROOT_SIGNATURE globalSig =
        {
            rootSignature
        };

        D3D12_RAYTRACING_PIPELINE_CONFIG pipelineCfg =
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
                .pDesc = &hitGroup
            },
            {
                .Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG,
                .pDesc = &shaderCfg
            },
            {
                .Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE,
                .pDesc = &globalSig
            },
            {
                .Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG,
                .pDesc = &pipelineCfg
            }
        };

        D3D12_STATE_OBJECT_DESC desc =
        {
            .Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
            .NumSubobjects = std::size(subobjects),
            .pSubobjects = subobjects
        };

        context->device->CreateStateObject(&desc, IID_PPV_ARGS(&pso));

        auto idDesc = BASIC_BUFFER_DESC;
        idDesc.Width = NUM_SHADER_IDS * D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
        context->device->CreateCommittedResource(
            &UPLOAD_HEAP,
            D3D12_HEAP_FLAG_NONE,
            &idDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&shaderIDs));

        ID3D12StateObjectProperties* props;
        pso->QueryInterface(&props);

        void* data;
        auto writeId = [&](const wchar_t* name)
        {
            void* id = props->GetShaderIdentifier(name);
            memcpy(data, id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
            data = static_cast<char*>(data) +
                D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
        };

        shaderIDs->Map(0, nullptr, &data);
        writeId(L"RayGeneration");
        writeId(L"Miss");
        writeId(L"HitGroup");
        shaderIDs->Unmap(0, nullptr);

        props->Release();
    }

    ID3D12Resource* Raytracing::MakeBLAS(
        ID3D12Resource* vertexBuffer,
        UINT vertexFloats,
        ID3D12Resource* indexBuffer,
        UINT indices)
    {
        D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc =
        {
            .Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
            .Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,

            .Triangles =
            {
                .Transform3x4 = 0,

                .IndexFormat = indexBuffer ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_UNKNOWN,
                .VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT,
                .IndexCount = indices,
                .VertexCount = vertexFloats / 3,
                .IndexBuffer = indexBuffer ? indexBuffer->GetGPUVirtualAddress() : 0,
                .VertexBuffer =
                {
                    .StartAddress = vertexBuffer->GetGPUVirtualAddress(),
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
            .pGeometryDescs = &geometryDesc
        };

        return MakeAccelerationStructure(inputs);
    }

    ID3D12Resource* Raytracing::MakeTLAS(
        ID3D12Resource* instances,
        UINT numInstances,
        UINT64* updateScratchSize)
    {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs =
        {
            .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
            .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE,
            .NumDescs = numInstances,
            .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
            .InstanceDescs = instances->GetGPUVirtualAddress()
        };

        return MakeAccelerationStructure(inputs, updateScratchSize);
    }

    void Raytracing::UpdateTransforms()
    {
        using namespace DirectX;
        auto set = [&](int idx, XMMATRIX mx)
        {
            auto* ptr = reinterpret_cast<XMFLOAT3X4*>(&instanceData[idx].Transform);
            XMStoreFloat3x4(ptr, mx);
        };

        auto time = static_cast<float>(GetTickCount64()) / 1000;

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
            .DestAccelerationStructureData = tlas->GetGPUVirtualAddress(),
            .Inputs =
            {
                .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
                .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE,
                .NumDescs = NUM_INSTANCES,
                .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
                .InstanceDescs = instances->GetGPUVirtualAddress()
            },
            .SourceAccelerationStructureData = tlas->GetGPUVirtualAddress(),
            .ScratchAccelerationStructureData = tlasUpdateScratch->GetGPUVirtualAddress(),
        };

        cmdList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);

        D3D12_RESOURCE_BARRIER barrier =
        {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
            .UAV =
            {
                .pResource = tlas
            }
        };

        cmdList->ResourceBarrier(1, &barrier);
    }

    void Raytracing::Render()
    {
        context->cmdAlloc->Reset();
        cmdList->Reset(context->cmdAlloc, nullptr);

        UpdateScene();

        cmdList->SetPipelineState1(pso);

        cmdList->SetComputeRootSignature(rootSignature);

        cmdList->SetDescriptorHeaps(1, &context->uavHeap);

        auto uavTable = context->uavHeap->GetGPUDescriptorHandleForHeapStart();

        cmdList->SetComputeRootDescriptorTable(0, uavTable); // ?u0 ?t0

        cmdList->SetComputeRootShaderResourceView(
            1,
            tlas->GetGPUVirtualAddress());

        auto rtDesc = context->renderTarget->GetDesc();

        D3D12_DISPATCH_RAYS_DESC dispatchDesc =
        {
            .RayGenerationShaderRecord =
            {
                .StartAddress = shaderIDs->GetGPUVirtualAddress(),
                .SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
            },
            .MissShaderTable =
            {
                .StartAddress = shaderIDs->GetGPUVirtualAddress() +
                                D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT,
                .SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
            },
            .HitGroupTable =
            {
                .StartAddress = shaderIDs->GetGPUVirtualAddress() +
                                2 * D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT,
                .SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
            },
            .Width = static_cast<UINT>(rtDesc.Width),
            .Height = rtDesc.Height,
            .Depth = 1
        };

        cmdList->DispatchRays(&dispatchDesc);

        ID3D12Resource* backBuffer;
        context->swapChain->GetBuffer(
            context->swapChain->GetCurrentBackBufferIndex(),
            IID_PPV_ARGS(&backBuffer));

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
            cmdList->ResourceBarrier(1, &rb);
        };

        barrier(
            context->renderTarget,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE);

        barrier(
            backBuffer,
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_COPY_DEST);

        cmdList->CopyResource(
            backBuffer,
            context->renderTarget);

        barrier(
            backBuffer,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PRESENT);

        barrier(
            context->renderTarget,
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        backBuffer->Release();

        cmdList->Close();
        context->cmdQueue->ExecuteCommandLists(
            1, reinterpret_cast<ID3D12CommandList**>(&cmdList));

        context->Flush();
        context->swapChain->Present(1, 0);
    }

}
