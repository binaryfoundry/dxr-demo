#pragma once

#include "../interfaces/IRenderer.hpp"

namespace d3d12
{
    class Renderer : public IRenderer
    {
    public:
        Renderer();
        virtual ~Renderer();

        void Initialize(uint32_t width, uint32_t height);
        void Resize(uint32_t width, uint32_t height);
        void Render();
        void Destroy();

    private:
    };
}
