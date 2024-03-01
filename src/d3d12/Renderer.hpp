#pragma once

#include <memory>
#include <algorithm>

#include "Frame.hpp"
#include "Context.hpp"
#include "Raytracing.hpp"
#include "../interfaces/IRenderer.hpp"

namespace d3d12
{
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
        HWND hwnd;

        std::shared_ptr<d3d12::Context> context;
        std::unique_ptr<d3d12::Raytracing> raytracing;

        void ResizeSwapChain();

        void CreateBackBuffer();
        void MoveToNextFrame();
        void ResetCommandList();
        void FlushGpu();
        void WaitForGpu();

        void ReleaseWhenFrameComplete(D3D12MA::ResourcePtr&& resource);
        void ReleaseWhenFrameComplete(DescriptorHandle&& handle);

        bool resize_swapchain = false;

        HANDLE fence_event;
    };
}
