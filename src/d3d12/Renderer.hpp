#pragma once

#include <memory>
#include <algorithm>

#include "Frame.hpp"
#include "Context.hpp"
#include "Imgui.hpp"
#include "Raytracing.hpp"
#include "../interfaces/IRenderer.hpp"

namespace d3d12
{
    class Renderer : public IRenderer
    {
    public:
        Renderer(HWND hwnd);
        virtual ~Renderer();

        void Initialize(const uint32_t width, const uint32_t height);
        void SetSize(const uint32_t width, const uint32_t height);
        void Render();
        void Destroy();

    private:
        HWND hwnd;

        std::shared_ptr<d3d12::Context> context;
        std::unique_ptr<d3d12::Imgui> imgui;
        std::unique_ptr<d3d12::Raytracing> raytracing;

        void ResizeSwapChain();

        void CreateBackBuffer();
        void MoveToNextFrame();
        void ResetCommandList();
        void FlushGpu();
        void WaitForGpu();

        bool resize_swapchain = false;

        HANDLE fence_event;
    };
}
