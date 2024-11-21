#pragma once

#include "../Context.hpp"
#include "../Allocator.hpp"
#include "../../Camera.hpp"
#include "../../Entities.hpp"
#include "BottomStructure.hpp"

#include <vector>
#include <memory>

namespace d3d12
{
namespace raytracing
{
    struct RaytracingUniforms
    {
        glm::vec4 Position;
        float TMin;
        float TMax;
        float Aspect;
        float Zoom;
        glm::vec4 Padding1;
        glm::vec4 Padding2;
        glm::mat4 View;
        glm::mat4 Padding4;
        glm::mat4 Padding5;
    };

    struct CurrentFrameResources
    {
        D3D12MA::ResourcePtr tlas_update_scratch;
        D3D12MA::ResourcePtr constants;
    };

    class TopStructure
    {
    private:
        std::shared_ptr<d3d12::Context> context;

        CurrentFrameResources& FrameResources();
        std::array<CurrentFrameResources, FRAME_COUNT> frame_resources;

        D3D12MA::ResourcePtr instances;
        D3D12_RAYTRACING_INSTANCE_DESC* instance_data = nullptr;

        D3D12MA::ResourcePtr tlas;

        void Update(
            EntityList& entities);

    public:
        TopStructure(
            std::shared_ptr<d3d12::Context> context);
        virtual ~TopStructure() = default;

        void Initialize(
            std::vector<std::shared_ptr<raytracing::BottomStructure>>& blas_list,
            EntityList& entities);

        void Render(
            Camera& camera,
            EntityList& entities,
            ComPtr<ID3D12DescriptorHeap>& uav_heap,
            const D3D12_DISPATCH_RAYS_DESC& dispatch_desc);

        D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress();
    };
}
}
