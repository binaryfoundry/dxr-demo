#pragma once

#include "../Context.hpp"

namespace d3d12
{
namespace raytracing
{
    class BottomStructure
    {
    private:
        std::shared_ptr<d3d12::Context> context;

        ID3D12Resource* blas = nullptr;
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs;
        D3D12_RAYTRACING_GEOMETRY_DESC geometry_desc;

    public:
        BottomStructure(
            std::shared_ptr<d3d12::Context> context,
            ID3D12Resource* vertex_buffer,
            const size_t vertex_floats,
            ID3D12Resource* index_buffer = nullptr,
            const size_t indices = 0);
        virtual ~BottomStructure() = default;

        void Initialize();

        D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress();
    };
}
}
