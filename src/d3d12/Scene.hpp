#pragma once

#include <array>
#include <memory>

#include "Context.hpp"

#include "raytracing/TopStructure.hpp"
#include "raytracing/BottomStructure.hpp"

namespace d3d12
{
    class Scene
    {
    public:
        Scene(
            std::shared_ptr<d3d12::Context> context);
        virtual ~Scene() = default;

        void Initialize();

        void Render();
        void Resize();

        void MoveToNextFrame();

    private:
        std::shared_ptr<d3d12::Context> context;

        ComPtr<ID3D12DescriptorHeap> uav_heap;
        ComPtr<ID3D12Resource> render_target;

        ID3D12RootSignature* root_signature = nullptr;
        ID3D12StateObject* pso = nullptr;
        ID3D12Resource* shader_ids = nullptr;

        void InitRootSignature();
        void InitPipeline();

        ID3D12Resource* quad_vb = nullptr;
        ID3D12Resource* cube_vb = nullptr;
        ID3D12Resource* cube_ib = nullptr;

        std::shared_ptr<raytracing::TopStructure> tlas;

        std::shared_ptr<raytracing::BottomStructure> cube_blas;
        std::shared_ptr<raytracing::BottomStructure> quad_blas;

        std::vector<std::shared_ptr<raytracing::BottomStructure>> blas_init_list;

        void InitMeshes();
        void InitAccelerationStructure();

        void Flush();
    };
}
