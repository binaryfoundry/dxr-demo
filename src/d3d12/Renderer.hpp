#pragma once

#include <memory>

#include "Frame.hpp"
#include "Context.hpp"
#include "Imgui.hpp"
#include "Scene.hpp"

#include "../Camera.hpp"
#include "../math/Math.hpp"
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
        void Render(Camera& camera, EntityList& entities);
        void Destroy();

    private:
        HWND hwnd;

        std::shared_ptr<d3d12::Context> context;
        std::unique_ptr<d3d12::Imgui> imgui;
        std::unique_ptr<d3d12::Scene> scene;

        void ResizeSwapChain();

        void EnableDebugLayer();
        void CreateDevice();
        void CreateAllocator();
        void CreateCommandQueue();
        void CreateSwapChain();
        void CreateBackBuffer();
        void CreateFence();
        void InitializeImguiAndScene();
        void CreateCommandList();

        void MoveToNextFrame();
        void ResetCommandList();
        void FlushGpu();
        void WaitForGpu();

        bool resize_swapchain = false;

        HANDLE fence_event;
    };
}
