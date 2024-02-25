#pragma once

#include <memory>

#include "Context.hpp"

namespace d3d12
{
    class Raytracing
    {
    public:
        Raytracing(
            std::shared_ptr<d3d12::Context> context);
        ~Raytracing();

        void Initialize();
        void Render();

    private:
        std::shared_ptr<d3d12::Context> context;

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
