#pragma once

#include <array>
#include <memory>

#include "Context.hpp"

#include "raytracing/Scene.hpp"

namespace d3d12
{
    class Raytracing
    {
    public:
        Raytracing(
            std::shared_ptr<d3d12::Context> context);
        virtual ~Raytracing() = default;

        void Initialize();

        void Render();
        void Resize();

        void MoveToNextFrame();

    private:
        std::shared_ptr<d3d12::Context> context;

        std::unique_ptr<raytracing::Scene> scene;

        ComPtr<ID3D12DescriptorHeap> uav_heap;
        ComPtr<ID3D12Resource> render_target;

        ID3D12RootSignature* root_signature = nullptr;
        ID3D12StateObject* pso = nullptr;
        ID3D12Resource* shader_ids = nullptr;

        void InitRootSignature();
        void InitPipeline();

        void Flush();
    };
}
