#pragma once

#include "../Context.hpp"

#include "TopStructure.hpp"
#include "BottomStructure.hpp"

namespace d3d12
{
namespace raytracing
{
    class Scene
    {
    private:
        std::shared_ptr<d3d12::Context> context;

        ID3D12Resource* quad_vb = nullptr;
        ID3D12Resource* cube_vb = nullptr;
        ID3D12Resource* cube_ib = nullptr;

        std::vector<std::shared_ptr<TopStructure>> tlas_init_list;
        std::vector<std::shared_ptr<BottomStructure>> blas_init_list;

        std::shared_ptr<raytracing::TopStructure> tlas;
        std::shared_ptr<raytracing::BottomStructure> cube_blas;
        std::shared_ptr<raytracing::BottomStructure> quad_blas;

        void InitMeshes();
        void InitAccelerationStructure();

    public:
        Scene(std::shared_ptr<d3d12::Context> context);
        virtual ~Scene() = default;

        void Update();
        void Render(
            ComPtr<ID3D12DescriptorHeap>& uav_heap,
            const D3D12_DISPATCH_RAYS_DESC& dispatch_desc);

        void MoveToNextFrame();
    };
}
}
