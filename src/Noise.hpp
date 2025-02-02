#pragma once

#include <vector>
#include <memory>

#include "math/Math.hpp"

namespace Noise
{
    void generate(
        std::unique_ptr<std::vector<TexDataByteRGBA>>& texture_data,
        const uint16_t sample,
        const uint16_t max_samples);
}
