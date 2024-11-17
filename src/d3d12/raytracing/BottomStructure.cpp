#include "BottomStructure.hpp"

#include "Allocation.hpp"

namespace d3d12
{
namespace raytracing
{
    BottomStructure::BottomStructure(
        std::shared_ptr<d3d12::Context> context,
        ID3D12Resource* vertex_buffer,
        const size_t vertex_floats,
        ID3D12Resource* index_buffer,
        const size_t indices) :
        context(context)
    {
        geometry_desc =
        {
            .Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
            .Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,

            .Triangles =
            {
                .Transform3x4 = 0,

                .IndexFormat = index_buffer ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_UNKNOWN,
                .VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT,
                .IndexCount = static_cast<UINT>(indices),
                .VertexCount = static_cast<UINT>(vertex_floats) / 3,
                .IndexBuffer = index_buffer ? index_buffer->GetGPUVirtualAddress() : 0,
                .VertexBuffer =
                {
                    .StartAddress = vertex_buffer->GetGPUVirtualAddress(),
                    .StrideInBytes = sizeof(float) * 3
                }
            }
        };

        inputs =
        {
            .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
            .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
            .NumDescs = 1,
            .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
            .pGeometryDescs = &geometry_desc
        };
    }

    void BottomStructure::Initialize()
    {
        MakeAccelerationStructure(
            context, context->command_list.Get(), inputs, blas);
    }

    D3D12_GPU_VIRTUAL_ADDRESS BottomStructure::GetGPUVirtualAddress()
    {
        return blas->GetResource()->GetGPUVirtualAddress();
    }
}
}
