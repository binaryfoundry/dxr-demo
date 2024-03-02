#include "Imgui.hpp"

#include "examples/imgui_impl_dx12.h"
#include "examples/imgui_impl_dx12.cpp"

namespace d3d12
{
    Imgui::Imgui(std::shared_ptr<Context> context) :
        context(context)
    {
        ID3D12DescriptorHeap* desc_heap;

        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 1;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (context->device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&desc_heap)) != S_OK)
        {
            throw std::exception("err");
        }

        ImGui_ImplDX12_Init(
            context->device.Get(),
            FRAME_COUNT,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            desc_heap,
            desc_heap->GetCPUDescriptorHandleForHeapStart(),
            desc_heap->GetGPUDescriptorHandleForHeapStart());

        g_pd3dSrvDescHeap = desc_heap;

        ImGui_ImplDX12_CreateDeviceObjects();
    }

    Imgui::~Imgui()
    {
        ImGui_ImplDX12_Shutdown();
    }

    void Imgui::Render()
    {
        ImGui_ImplDX12_NewFrame();

        ImGui::Render();

        context->command_list->SetDescriptorHeaps(
            1, &g_pd3dSrvDescHeap);

        ImGui_ImplDX12_RenderDrawData(
            ImGui::GetDrawData(),
            context->command_list.Get());
    }
}
