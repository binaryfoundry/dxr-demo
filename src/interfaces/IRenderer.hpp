#pragma once

#include <cstdint>

class IRenderer
{
public:
    virtual void Initialize(uint32_t width, uint32_t height) = 0;
    virtual void SetSize(uint32_t width, uint32_t height) = 0;
    virtual void Render() = 0;
    virtual void Destroy() = 0;
};
