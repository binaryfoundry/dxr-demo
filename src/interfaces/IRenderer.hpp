#pragma once

#include <cstdint>

#include "../Camera.hpp"
#include "../Entities.hpp"
#include "../math/Math.hpp"

class IRenderer
{
public:
    virtual void Initialize(uint32_t width, uint32_t height) = 0;
    virtual void SetSize(uint32_t width, uint32_t height) = 0;
    virtual void Render(Camera& camera, EntityList& entities) = 0;
    virtual void Destroy() = 0;
};
