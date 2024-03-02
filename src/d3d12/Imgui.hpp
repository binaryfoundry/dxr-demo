#pragma once

#include <memory>

#include "Context.hpp"

namespace d3d12
{
    class Imgui
    {
    public:
        Imgui(std::shared_ptr<Context> context);
        virtual ~Imgui();
        void Render();

    private:
        std::shared_ptr<Context> context;
        ID3D12DescriptorHeap* g_pd3dSrvDescHeap;
    };
}
