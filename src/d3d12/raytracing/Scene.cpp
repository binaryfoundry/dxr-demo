#include "Scene.hpp"

namespace d3d12
{
namespace raytracing
{
    constexpr UINT NUM_INSTANCES = 3;

    constexpr float quad_vtx[] =
    {
        -1, 0, -1, -1, 0,  1, 1, 0, 1,
        -1, 0, -1,  1, 0, -1, 1, 0, 1
    };

    constexpr float cube_vtx[] =
    {
        -1, -1, -1, 1, -1, -1, -1, 1, -1, 1, 1, -1,
        -1, -1,  1, 1, -1,  1, -1, 1,  1, 1, 1,  1
    };

    constexpr short cube_idx[] =
    {
        4, 6, 0, 2, 0, 6, 0, 1, 4, 5, 4, 1,
        0, 2, 1, 3, 1, 2, 1, 3, 5, 7, 5, 3,
        2, 6, 3, 7, 3, 6, 4, 5, 6, 7, 6, 5
    };

    Scene::Scene(std::shared_ptr<d3d12::Context> context) :
        context(context)
    {
        InitMeshes();

        InitAccelerationStructure();
    }

    void Scene::MoveToNextFrame()
    {
        tlas->MoveToNextFrame();
    }

    void Scene::InitMeshes()
    {
        auto make_and_copy = [&](auto& data) {
            auto desc = BASIC_BUFFER_DESC;
            desc.Width = sizeof(data);
            ID3D12Resource* res;
            context->device->CreateCommittedResource(
                &UPLOAD_HEAP,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&res));

            void* ptr;
            res->Map(0, nullptr, &ptr);
            memcpy(ptr, data, sizeof(data));
            res->Unmap(0, nullptr);
            return res;
        };

        quad_vb = make_and_copy(quad_vtx);
        cube_vb = make_and_copy(cube_vtx);
        cube_ib = make_and_copy(cube_idx);
    }

    void Scene::InitAccelerationStructure()
    {
        quad_blas = std::make_shared<BottomStructure>(
            context,
            quad_vb,
            std::size(quad_vtx));

        cube_blas = std::make_shared<BottomStructure>(
            context,
            cube_vb,
            std::size(cube_vtx),
            cube_ib,
            std::size(cube_idx));

        blas_init_list.push_back(quad_blas);
        blas_init_list.push_back(cube_blas);

        std::vector<std::shared_ptr<BottomStructure>> instance_list;
        instance_list.push_back(cube_blas);
        instance_list.push_back(quad_blas);
        instance_list.push_back(quad_blas);

        tlas = std::make_shared<TopStructure>(
            context, instance_list);

        tlas_init_list.push_back(tlas);
    }

    void Scene::Update()
    {
        D3D12_RESOURCE_BARRIER uavBarrier = {};
        uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        uavBarrier.UAV.pResource = nullptr; // Applies to all UAV writes

        if (!blas_init_list.empty())
        {

            for (const auto& blas : blas_init_list)
            {
                blas->Initialize();
            }

            context->command_list->ResourceBarrier(1, &uavBarrier);

            blas_init_list.clear();
        }

        // TODO only need one TLAS
        if (!tlas_init_list.empty())
        {
            for (const auto& tlas : tlas_init_list)
            {
                tlas->Initialize();
            }

            context->command_list->ResourceBarrier(1, &uavBarrier);

            tlas_init_list.clear();
        }

        tlas->Update();
    }

    void Scene::Render(
        ComPtr<ID3D12DescriptorHeap>& uav_heap,
        const D3D12_DISPATCH_RAYS_DESC& dispatch_desc)
    {
        tlas->Render(uav_heap, dispatch_desc);
    }

}
}
