#include "Renderer.hpp"

#include <Windows.h>
#include <DirectXMath.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxgi")

namespace d3d12
{
    Renderer::Renderer(HWND hwnd) :
        hwnd(hwnd),
        context(std::make_shared<d3d12::Context>())
    {

    }

    Renderer::~Renderer()
    {
        WaitForGpu();

        for (UINT frame_id = 0; frame_id < FRAME_COUNT; frame_id++)
        {
            auto& frame = context->frames[frame_id];
            frame.resources_to_release.clear();
            frame.handles_to_release.clear();
            //frame.constant_buffer_pool_.pool_.clear();
            //frame.constant_buffer_pool_backbuffer_.pool_.clear();
            frame.rtv_handle = DescriptorHandle();
        }

        if (context->allocator)
        {
            context->allocator->Release();
            context->allocator = nullptr;
        }
    }

    void Renderer::Initialize(uint32_t w, uint32_t h)
    {
        context->width = w;
        context->height = h;

        if (ID3D12Debug* debug;
            SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
        {
            debug->EnableDebugLayer(), debug->Release();
        }

        D3D12CreateDevice(
            nullptr,
            D3D_FEATURE_LEVEL_12_1,
            IID_PPV_ARGS(&context->device));

        D3D12MA::ALLOCATOR_DESC alloc_desc = {};
        alloc_desc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;
        alloc_desc.pDevice = context->device.Get();
        alloc_desc.PreferredBlockSize = 32ull * 1024 * 1024;

        D3D12MA::CreateAllocator(&alloc_desc, &context->allocator);

        context->descriptor_allocator.reset(
            new DescriptorAllocator(context->device.Get()));

        context->gpu_descriptor_ring_buffer.reset(
            new GPUDescriptorRingBuffer(context->device.Get()));

        D3D12_COMMAND_QUEUE_DESC cmd_queue_desc =
        {
            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
        };

        context->device->CreateCommandQueue(
            &cmd_queue_desc,
            IID_PPV_ARGS(&context->command_queue));

        IDXGIFactory2* factory;

        if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG,
            IID_PPV_ARGS(&factory))))
        {
            CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
        }

        DXGI_SWAP_CHAIN_DESC1 swap_chain_desc =
        {
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .SampleDesc = NO_AA,
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = FRAME_COUNT,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        };

        ComPtr<IDXGISwapChain1> swap_chain_1;
        factory->CreateSwapChainForHwnd(
            context->command_queue.Get(),
            hwnd,
            &swap_chain_desc,
            nullptr,
            nullptr,
            &swap_chain_1);

        factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

        swap_chain_1.As(&context->swap_chain);

        context->frame_index = context->swap_chain->GetCurrentBackBufferIndex();

        factory->Release();

        CreateBackBuffer();

        context->device->CreateFence(
            context->CurrentFrame().fence_value,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&context->fence));

        raytracing = std::make_unique<d3d12::Raytracing>(context);
        raytracing->Initialize();

        context->CurrentFrame().fence_value++;

        fence_event = CreateEvent(
            nullptr,
            FALSE,
            FALSE,
            nullptr);

        if (fence_event == nullptr)
        {
            //LogIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        context->device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            context->frames[0].command_allocator.Get(),
            nullptr,
            IID_PPV_ARGS(&context->command_list));

        context->command_list->Close();

        ResetCommandList();
    }

    void Renderer::Destroy()
    {
    }

    void Renderer::SetSize(uint32_t w, uint32_t h)
    {
        context->width = w;
        context->height = h;

        resize_swapchain = true;
    }

    void Renderer::CreateBackBuffer()
    {
        for (UINT n = 0; n < FRAME_COUNT; n++)
        {
            context->swap_chain->GetBuffer(
                n,
                IID_PPV_ARGS(&context->frames[n].render_target));

            context->frames[n].rtv_handle = context->descriptor_allocator->Create(
                D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

            D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
            rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            context->device->CreateRenderTargetView(
                context->frames[n].render_target.Get(),
                &rtv_desc,
                context->frames[n].rtv_handle.cpu_handle());

            context->device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&context->frames[n].command_allocator));
        }
    }

    void Renderer::MoveToNextFrame()
    {
        Frame& last_frame = context->CurrentFrame();
        const UINT64 current_fence_value = last_frame.fence_value;

        context->command_queue->Signal(
            context->fence.Get(),
            current_fence_value);

        context->frame_index = context->swap_chain->GetCurrentBackBufferIndex();

        Frame& cur_frame = context->CurrentFrame();

        if (context->fence->GetCompletedValue() < cur_frame.fence_value)
        {
            context->fence->SetEventOnCompletion(
                cur_frame.fence_value,
                fence_event);

            WaitForSingleObjectEx(fence_event, INFINITE, FALSE);
        }

        cur_frame.fence_value = current_fence_value + 1;

        cur_frame.resources_to_release.clear();
        cur_frame.handles_to_release.clear();

        ResetCommandList();
    }

    void Renderer::ResetCommandList()
    {
        context->CurrentFrame().command_allocator->Reset();

        context->command_list->Reset(
            context->CurrentFrame().command_allocator.Get(),
            nullptr);

        ID3D12DescriptorHeap* pp_heaps[] =
        {
            context->gpu_descriptor_ring_buffer->descriptor_heap()
        };

        context->command_list->SetDescriptorHeaps(
            _countof(pp_heaps),
            pp_heaps);
    }

    void Renderer::WaitForGpu()
    {
        context->command_queue->Signal(
            context->fence.Get(),
            context->CurrentFrame().fence_value);

        context->fence->SetEventOnCompletion(
            context->CurrentFrame().fence_value,
            fence_event);

        WaitForSingleObjectEx(
            fence_event,
            INFINITE,
            FALSE);

        context->CurrentFrame().fence_value++;
    }

    void Renderer::FlushGpu()
    {
        for (int i = 0; i < FRAME_COUNT; i++)
        {
            uint64_t fence_value_for_signal = ++context->frames[i].fence_value;

            context->command_queue->Signal(
                context->fence.Get(),
                fence_value_for_signal);

            if (context->fence->GetCompletedValue() < context->frames[i].fence_value)
            {
                context->fence->SetEventOnCompletion(
                    fence_value_for_signal,
                    fence_event);

                WaitForSingleObject(
                    fence_event,
                    INFINITE);
            }
        }
        context->frame_index = 0;
    }

    void Renderer::ResizeSwapChain()
    {
        if (!context->swap_chain) [[unlikely]]
        {
            return;
        }

        RECT rect;
        GetClientRect(hwnd, &rect);
        context->width = std::max<UINT>(rect.right - rect.left, 1);
        context->height = std::max<UINT>(rect.bottom - rect.top, 1);

        Frame& last_frame = context->CurrentFrame();
        if (context->device == nullptr || context->swap_chain == nullptr)
        {
            return;
        }

        FlushGpu();

        last_frame.command_allocator->Reset();

        context->command_list->Reset(
            last_frame.command_allocator.Get(),
            nullptr);

        for (int i = 0; i < FRAME_COUNT; i++)
        {
            context->frames[i].render_target.Reset();
        }

        context->swap_chain->ResizeBuffers(
            FRAME_COUNT,
            context->width,
            context->height,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            NULL);

        context->frame_index = 0;

        CreateBackBuffer();

        context->command_list->Close();
        std::vector<ID3D12CommandList*> cmds_lists =
        {
            context->command_list.Get()
        };

        context->command_queue->ExecuteCommandLists(
            cmds_lists.size(),
            cmds_lists.data());

        raytracing->Resize();
    }

    void Renderer::ReleaseWhenFrameComplete(D3D12MA::ResourcePtr&& resource)
    {
        context->CurrentFrame().resources_to_release.emplace_back(std::move(resource));
    }

    void Renderer::ReleaseWhenFrameComplete(DescriptorHandle&& handle)
    {
        context->CurrentFrame().handles_to_release.emplace_back(std::move(handle));
    }

    void Renderer::Render()
    {
        auto barrier_0 = CD3DX12_RESOURCE_BARRIER::Transition(
            context->CurrentFrame().render_target.Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        context->command_list->ResourceBarrier(
            1,
            &barrier_0);

        const D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle =
            context->CurrentFrame().rtv_handle.cpu_handle();

        context->command_list->OMSetRenderTargets(
            1,
            &rtv_handle,
            false,
            nullptr);

        const FLOAT clear_color[] = { 1.0f, 0.0f, 0.0f, 1.0f };
        context->command_list->ClearRenderTargetView(
            rtv_handle,
            clear_color,
            0,
            nullptr);

        CD3DX12_VIEWPORT viewport(
            0.0f,
            0.0f,
            static_cast<float>(context->width),
            static_cast<float>(context->height));
        context->command_list->RSSetViewports(
            1, &viewport);

        CD3DX12_RECT scissor_rect(
            (LONG)(0),
            (LONG)(0),
            (LONG)(context->width),
            (LONG)(context->height));
        context->command_list->RSSetScissorRects(
            1, &scissor_rect);

        raytracing->Render();

        auto barrier_1 = CD3DX12_RESOURCE_BARRIER::Transition(
            context->CurrentFrame().render_target.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);

        context->command_list->ResourceBarrier(
            1,
            &barrier_1);

        context->command_list->Close();

        ID3D12CommandList* pp_command_lists[] =
        {
            context->command_list.Get()
        };

        context->command_queue->ExecuteCommandLists(
            _countof(pp_command_lists),
            pp_command_lists);

        context->swap_chain->Present(1, 0);

        if (resize_swapchain)
        {
            ResizeSwapChain();
            resize_swapchain = false;
        }

        MoveToNextFrame();
    }

}
