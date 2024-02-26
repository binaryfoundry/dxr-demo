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

        ID3D12GraphicsCommandList4* cmd_list = nullptr;

        ID3D12Resource* quad_vb = nullptr;
        ID3D12Resource* cube_vb = nullptr;
        ID3D12Resource* cube_ib = nullptr;

        ID3D12Resource* quad_blas = nullptr;
        ID3D12Resource* cube_blas = nullptr;

        ID3D12Resource* instances = nullptr;
        D3D12_RAYTRACING_INSTANCE_DESC* instance_data = nullptr;

        ID3D12Resource* tlas = nullptr;
        ID3D12Resource* tlas_update_scratch = nullptr;

        ID3D12RootSignature* root_signature = nullptr;

        ID3D12StateObject* pso = nullptr;

        ID3D12Resource* shader_ids = nullptr;

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
            UINT64* update_scratch_size = nullptr);

        ID3D12Resource* MakeBLAS(
            ID3D12Resource* vertex_buffer,
            UINT vertex_floats,
            ID3D12Resource* index_buffer = nullptr,
            UINT indices = 0);

        ID3D12Resource* MakeTLAS(
            ID3D12Resource* instances,
            UINT num_instances,
            UINT64* update_scratch_size);
    };
}
