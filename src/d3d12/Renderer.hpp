#pragma once

#include <algorithm>
#include <DirectXMath.h>
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include "../interfaces/IRenderer.hpp"

namespace d3d12
{
    constexpr UINT NUM_INSTANCES = 3;
    constexpr UINT64 NUM_SHADER_IDS = 3;

    class Renderer : public IRenderer
    {
    public:
        Renderer(HWND hwnd);
        virtual ~Renderer();

        void Initialize(uint32_t width, uint32_t height);
        void SetSize(uint32_t width, uint32_t height);
        void Render();
        void Destroy();

    private:
        uint32_t width;
        uint32_t height;

        HWND hwnd;

        ID3D12Device5* device = nullptr;
        ID3D12CommandQueue* cmdQueue = nullptr;
        ID3D12Fence* fence = nullptr;
        IDXGISwapChain3* swapChain = nullptr;
        ID3D12DescriptorHeap* uavHeap = nullptr;

        ID3D12Resource* renderTarget = nullptr;
        ID3D12CommandAllocator* cmdAlloc = nullptr;
        ID3D12GraphicsCommandList4* cmdList = nullptr;

        ID3D12Resource* quadVB = nullptr;
        ID3D12Resource* cubeVB = nullptr;
        ID3D12Resource* cubeIB = nullptr;

        ID3D12Resource* quadBlas = nullptr;
        ID3D12Resource* cubeBlas = nullptr;

        ID3D12Resource* instances = nullptr;
        D3D12_RAYTRACING_INSTANCE_DESC* instanceData = nullptr;

        ID3D12Resource* tlas = nullptr;
        ID3D12Resource* tlasUpdateScratch = nullptr;

        ID3D12RootSignature* rootSignature = nullptr;

        ID3D12StateObject* pso = nullptr;

        ID3D12Resource* shaderIDs = nullptr;

        void Flush();
        void Resize();
        void InitDevice();
        void InitSurfaces(HWND hwnd);
        void InitCommand();
        void InitMeshes();
        void InitBottomLevel();
        void InitScene();
        void InitTopLevel();
        void InitRootSignature();
        void InitPipeline();

        void UpdateTransforms();
        void UpdateScene();

        ID3D12Resource* MakeAccelerationStructure(
            const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs,
            UINT64* updateScratchSize = nullptr);

        ID3D12Resource* MakeBLAS(
            ID3D12Resource* vertexBuffer,
            UINT vertexFloats,
            ID3D12Resource* indexBuffer = nullptr,
            UINT indices = 0);

        ID3D12Resource* MakeTLAS(
            ID3D12Resource* instances,
            UINT numInstances,
            UINT64* updateScratchSize);
    };
}
