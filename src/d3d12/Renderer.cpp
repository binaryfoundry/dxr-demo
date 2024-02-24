#include "Renderer.hpp"


#include "shaders/shader.fxh"

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxgi")

namespace d3d12
{
    constexpr DXGI_SAMPLE_DESC NO_AA =
    {
        .Count = 1,
        .Quality = 0
    };

    constexpr D3D12_HEAP_PROPERTIES UPLOAD_HEAP =
    {
        .Type = D3D12_HEAP_TYPE_UPLOAD
    };

    constexpr D3D12_HEAP_PROPERTIES DEFAULT_HEAP =
    {
        .Type = D3D12_HEAP_TYPE_DEFAULT
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

    Renderer::Renderer(HWND hwnd) :
        hwnd(hwnd)
    {

    }

    Renderer::~Renderer()
    {

    }

    void Renderer::Initialize(uint32_t w, uint32_t h)
    {
        width = w;
        height = h;

        InitDevice();
        InitSurfaces(hwnd);
        InitCommand();
        InitMeshes();
        InitBottomLevel();
        InitScene();
        InitTopLevel();
        InitRootSignature();
        InitPipeline();
    }

    void Renderer::SetSize(uint32_t w, uint32_t h)
    {
        width = w;
        height = h;

        Resize();
    }

    void Renderer::Flush()
    {
        static UINT64 value = 1;
        cmdQueue->Signal(fence, value);
        fence->SetEventOnCompletion(value++, nullptr);
    }

    void Renderer::InitDevice()
    {
        if (ID3D12Debug* debug;
            SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
            debug->EnableDebugLayer(), debug->Release();

        D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device));

        D3D12_COMMAND_QUEUE_DESC cmdQueueDesc =
        {
            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
        };

        device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&cmdQueue));

        device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    }

    void Renderer::InitSurfaces(HWND hwnd)
    {
        IDXGIFactory2* factory;

        if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG,
            IID_PPV_ARGS(&factory))))
            CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc =
        {
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .SampleDesc = NO_AA,
            .BufferCount = 2,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        };

        IDXGISwapChain1* swapChain1;
        factory->CreateSwapChainForHwnd(cmdQueue, hwnd, &swapChainDesc, nullptr,
            nullptr, &swapChain1);
        swapChain1->QueryInterface(&swapChain);
        swapChain1->Release();

        factory->Release();

        D3D12_DESCRIPTOR_HEAP_DESC uavHeapDesc =
        {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = 1,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        };

        device->CreateDescriptorHeap(&uavHeapDesc, IID_PPV_ARGS(&uavHeap));

        Resize();
    }

    void Renderer::Resize()
    {
        if (!swapChain) [[unlikely]]
        {
            return;
        }

        RECT rect;
        GetClientRect(hwnd, &rect);
        auto width = std::max<UINT>(rect.right - rect.left, 1);
        auto height = std::max<UINT>(rect.bottom - rect.top, 1);

        Flush();

        swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

        if (renderTarget) [[likely]]
        {
            renderTarget->Release();
        }

        D3D12_RESOURCE_DESC rtDesc =
        {
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Width = width,
            .Height = height,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .SampleDesc = NO_AA,
            .Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
        };

        device->CreateCommittedResource(&DEFAULT_HEAP, D3D12_HEAP_FLAG_NONE, &rtDesc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nullptr, IID_PPV_ARGS(&renderTarget));

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc =
        {
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D
        };

        device->CreateUnorderedAccessView(
            renderTarget, nullptr, &uavDesc,
            uavHeap->GetCPUDescriptorHandleForHeapStart());
    }

    void Renderer::InitCommand()
    {
        device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&cmdAlloc));
        device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            D3D12_COMMAND_LIST_FLAG_NONE,
            IID_PPV_ARGS(&cmdList));
    }

    void Renderer::InitMeshes()
    {
        auto makeAndCopy = [&](auto& data) {
            auto desc = BASIC_BUFFER_DESC;
            desc.Width = sizeof(data);
            ID3D12Resource* res;
            device->CreateCommittedResource(&UPLOAD_HEAP, D3D12_HEAP_FLAG_NONE,
                &desc, D3D12_RESOURCE_STATE_COMMON,
                nullptr, IID_PPV_ARGS(&res));

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

    ID3D12Resource* Renderer::MakeAccelerationStructure(
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs,
        UINT64* updateScratchSize)
    {
        auto makeBuffer = [&](UINT64 size, auto initialState)
        {
            auto desc = BASIC_BUFFER_DESC;
            desc.Width = size;
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            ID3D12Resource* buffer;
            device->CreateCommittedResource(&DEFAULT_HEAP, D3D12_HEAP_FLAG_NONE,
                &desc, initialState, nullptr,
                IID_PPV_ARGS(&buffer));
            return buffer;
        };

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo;
        device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs,
            &prebuildInfo);

        if (updateScratchSize) {
            *updateScratchSize = prebuildInfo.UpdateScratchDataSizeInBytes;
        }

        auto* scratch = makeBuffer(prebuildInfo.ScratchDataSizeInBytes,
            D3D12_RESOURCE_STATE_COMMON);
        auto* as = makeBuffer(prebuildInfo.ResultDataMaxSizeInBytes,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc =
        {
            .DestAccelerationStructureData = as->GetGPUVirtualAddress(),
            .Inputs = inputs,
            .ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress()
        };

        cmdAlloc->Reset();
        cmdList->Reset(cmdAlloc, nullptr);
        cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
        cmdList->Close();
        cmdQueue->ExecuteCommandLists(
            1, reinterpret_cast<ID3D12CommandList**>(&cmdList));

        Flush();
        scratch->Release();
        return as;
    }

    ID3D12Resource* Renderer::MakeBLAS(ID3D12Resource* vertexBuffer, UINT vertexFloats,
        ID3D12Resource* indexBuffer, UINT indices)
    {
        D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc =
        {
            .Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
            .Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,

            .Triangles = {
                .Transform3x4 = 0,

                .IndexFormat = indexBuffer ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_UNKNOWN,
                .VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT,
                .IndexCount = indices,
                .VertexCount = vertexFloats / 3,
                .IndexBuffer = indexBuffer ? indexBuffer->GetGPUVirtualAddress() : 0,
                .VertexBuffer = {.StartAddress = vertexBuffer->GetGPUVirtualAddress(),
                                 .StrideInBytes = sizeof(float) * 3}}
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

    ID3D12Resource* Renderer::MakeTLAS(ID3D12Resource* instances, UINT numInstances,
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

    void Renderer::InitScene()
    {
        auto instancesDesc = BASIC_BUFFER_DESC;
        instancesDesc.Width = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * NUM_INSTANCES;
        device->CreateCommittedResource(&UPLOAD_HEAP, D3D12_HEAP_FLAG_NONE,
            &instancesDesc, D3D12_RESOURCE_STATE_COMMON,
            nullptr, IID_PPV_ARGS(&instances));
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

    void Renderer::UpdateTransforms()
    {
        using namespace DirectX;
        auto set = [&](int idx, XMMATRIX mx) {
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

    void Renderer::InitTopLevel()
    {
        UINT64 updateScratchSize;
        tlas = MakeTLAS(instances, NUM_INSTANCES, &updateScratchSize);

        auto desc = BASIC_BUFFER_DESC;
        desc.Width = updateScratchSize;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        device->CreateCommittedResource(&DEFAULT_HEAP, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&tlasUpdateScratch));
    }

    void Renderer::InitBottomLevel()
    {
        quadBlas = MakeBLAS(quadVB, std::size(quadVtx));
        cubeBlas = MakeBLAS(cubeVB, std::size(cubeVtx), cubeIB, std::size(cubeIdx));
    }

    void Renderer::InitRootSignature()
    {
        D3D12_DESCRIPTOR_RANGE uavRange =
        {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
            .NumDescriptors = 1,
        };

        D3D12_ROOT_PARAMETER params[] =
        {
            {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
             .DescriptorTable = {.NumDescriptorRanges = 1,
                                 .pDescriptorRanges = &uavRange}},

            {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
             .Descriptor = {.ShaderRegister = 0, .RegisterSpace = 0}}
        };

        D3D12_ROOT_SIGNATURE_DESC desc =
        {
            .NumParameters = std::size(params),
            .pParameters = params
        };

        ID3DBlob* blob;
        D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob,
            nullptr);
        device->CreateRootSignature(0, blob->GetBufferPointer(),
            blob->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature));
        blob->Release();
    }

    void Renderer::InitPipeline()
    {
        D3D12_DXIL_LIBRARY_DESC lib = {
            .DXILLibrary = {.pShaderBytecode = compiledShader,
                            .BytecodeLength = std::size(compiledShader)} };

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
            {.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, .pDesc = &lib},
            {.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, .pDesc = &hitGroup},
            {.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, .pDesc = &shaderCfg},
            {.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, .pDesc = &globalSig},
            {.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, .pDesc = &pipelineCfg}
        };

        D3D12_STATE_OBJECT_DESC desc =
        {
            .Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
            .NumSubobjects = std::size(subobjects),
            .pSubobjects = subobjects
        };

        device->CreateStateObject(&desc, IID_PPV_ARGS(&pso));

        auto idDesc = BASIC_BUFFER_DESC;
        idDesc.Width = NUM_SHADER_IDS * D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
        device->CreateCommittedResource(&UPLOAD_HEAP, D3D12_HEAP_FLAG_NONE, &idDesc,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&shaderIDs));

        ID3D12StateObjectProperties* props;
        pso->QueryInterface(&props);

        void* data;
        auto writeId = [&](const wchar_t* name) {
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

    void Renderer::UpdateScene()
    {
        UpdateTransforms();

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc =
        {
            .DestAccelerationStructureData = tlas->GetGPUVirtualAddress(),
            .Inputs = {
                .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
                .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE,
                .NumDescs = NUM_INSTANCES,
                .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
                .InstanceDescs = instances->GetGPUVirtualAddress()},
            .SourceAccelerationStructureData = tlas->GetGPUVirtualAddress(),
            .ScratchAccelerationStructureData = tlasUpdateScratch->GetGPUVirtualAddress(),
        };

        cmdList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);

        D3D12_RESOURCE_BARRIER barrier = { .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
                                          .UAV = {.pResource = tlas} };
        cmdList->ResourceBarrier(1, &barrier);
    }

    void Renderer::Render()
    {
        cmdAlloc->Reset();
        cmdList->Reset(cmdAlloc, nullptr);

        UpdateScene();

        cmdList->SetPipelineState1(pso);
        cmdList->SetComputeRootSignature(rootSignature);
        cmdList->SetDescriptorHeaps(1, &uavHeap);
        auto uavTable = uavHeap->GetGPUDescriptorHandleForHeapStart();
        cmdList->SetComputeRootDescriptorTable(0, uavTable); // ?u0 ?t0
        cmdList->SetComputeRootShaderResourceView(1, tlas->GetGPUVirtualAddress());

        auto rtDesc = renderTarget->GetDesc();

        D3D12_DISPATCH_RAYS_DESC dispatchDesc = {
            .RayGenerationShaderRecord = {
                .StartAddress = shaderIDs->GetGPUVirtualAddress(),
                .SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES},
            .MissShaderTable = {
                .StartAddress = shaderIDs->GetGPUVirtualAddress() +
                                D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT,
                .SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES},
            .HitGroupTable = {
                .StartAddress = shaderIDs->GetGPUVirtualAddress() +
                                2 * D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT,
                .SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES},
            .Width = static_cast<UINT>(rtDesc.Width),
            .Height = rtDesc.Height,
            .Depth = 1 };
        cmdList->DispatchRays(&dispatchDesc);

        ID3D12Resource* backBuffer;
        swapChain->GetBuffer(swapChain->GetCurrentBackBufferIndex(),
            IID_PPV_ARGS(&backBuffer));

        auto barrier = [&](auto* resource, auto before, auto after) {
            D3D12_RESOURCE_BARRIER rb = {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                .Transition = {.pResource = resource,
                               .StateBefore = before,
                               .StateAfter = after},
            };
            cmdList->ResourceBarrier(1, &rb);
        };

        barrier(renderTarget, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE);
        barrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_COPY_DEST);

        cmdList->CopyResource(backBuffer, renderTarget);

        barrier(backBuffer, D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PRESENT);
        barrier(renderTarget, D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        backBuffer->Release();

        cmdList->Close();
        cmdQueue->ExecuteCommandLists(
            1, reinterpret_cast<ID3D12CommandList**>(&cmdList));

        Flush();
        swapChain->Present(1, 0);
    }

    void Renderer::Destroy()
    {

    }
}
