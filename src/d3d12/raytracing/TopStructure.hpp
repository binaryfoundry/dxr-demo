#pragma once

#include "../Context.hpp"
#include "../Allocator.hpp"
#include "../../Camera.hpp"
#include "BottomStructure.hpp"

#include <vector>
#include <memory>
#include <DirectXMath.h>

namespace d3d12
{
namespace raytracing
{
    struct RaytracingUniforms
    {
        glm::vec4 Position;
        glm::vec4 Padding0;
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

        std::vector<std::shared_ptr<BottomStructure>> instance_list;
        D3D12MA::ResourcePtr instances;
        D3D12_RAYTRACING_INSTANCE_DESC* instance_data = nullptr;

        D3D12MA::ResourcePtr tlas;
        bool initialized = false;

        void Initialize();
        void UpdateTransforms();

    public:
        TopStructure(
            std::shared_ptr<d3d12::Context> context,
            const std::vector<std::shared_ptr<BottomStructure>>& instance_list);
        virtual ~TopStructure() = default;

        void Update();
        void Render(
            Camera& camera,
            ComPtr<ID3D12DescriptorHeap>& uav_heap,
            const D3D12_DISPATCH_RAYS_DESC& dispatch_desc);

        D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress();
    };
}
}
