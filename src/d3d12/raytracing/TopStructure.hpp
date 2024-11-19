#pragma once

#include "../Context.hpp"
#include "../Allocator.hpp"

#include "BottomStructure.hpp"

#include <vector>
#include <memory>
#include <DirectXMath.h>

namespace d3d12
{
namespace raytracing
{
    struct InstanceUniforms
    {
        DirectX::XMMATRIX Transform;
        DirectX::XMMATRIX Scale;
        DirectX::XMMATRIX Padding1;
        DirectX::XMMATRIX Padding2;
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

        DirectX::XMVECTOR translation = { 0, 1.5, -7, 1 };

    public:
        TopStructure(
            std::shared_ptr<d3d12::Context> context,
            const std::vector<std::shared_ptr<BottomStructure>>& instance_list);
        virtual ~TopStructure() = default;

        void Update();
        void Render(
            ComPtr<ID3D12DescriptorHeap>& uav_heap,
            const D3D12_DISPATCH_RAYS_DESC& dispatch_desc);

        D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress();
    };
}
}
