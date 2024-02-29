#pragma once

#include <array>
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
        void Resize();

    private:
        std::shared_ptr<d3d12::Context> context;

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

        std::array<ComPtr<ID3D12DescriptorHeap>, FRAME_COUNT> uav_heaps;
        std::array<ComPtr<ID3D12Resource>, FRAME_COUNT> render_targets;

        void InitMeshes();
        void InitBottomLevel();
        void InitScene();
        void InitTopLevel();
        void InitRootSignature();
        void InitPipeline();

        void Flush();

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
