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

    }

    void Renderer::Initialize(uint32_t w, uint32_t h)
    {
        context->width = w;
        context->height = h;

        InitDevice();
        InitSurfaces(hwnd);

        raytracing = std::make_unique<d3d12::Raytracing>(context);
        raytracing->Initialize();
    }

    void Renderer::SetSize(uint32_t w, uint32_t h)
    {
        context->width = w;
        context->height = h;

        Resize();
    }

    void Renderer::InitDevice()
    {
        if (ID3D12Debug* debug;
            SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
        {
            debug->EnableDebugLayer(), debug->Release();
        }

        D3D12CreateDevice(
            nullptr,
            D3D_FEATURE_LEVEL_12_1,
            IID_PPV_ARGS(&context->device));

        D3D12_COMMAND_QUEUE_DESC cmdQueueDesc =
        {
            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
        };

        context->device->CreateCommandQueue(
            &cmdQueueDesc,
            IID_PPV_ARGS(&context->cmdQueue));

        context->device->CreateFence(
            0,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&context->fence));
    }

    void Renderer::InitSurfaces(HWND hwnd)
    {
        IDXGIFactory2* factory;

        if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG,
            IID_PPV_ARGS(&factory))))
        {
            CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
        }

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc =
        {
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .SampleDesc = NO_AA,
            .BufferCount = 2,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        };

        IDXGISwapChain1* swapChain1;
        factory->CreateSwapChainForHwnd(
            context->cmdQueue,
            hwnd,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1);
        swapChain1->QueryInterface(
            &context->swapChain);
        swapChain1->Release();

        factory->Release();

        D3D12_DESCRIPTOR_HEAP_DESC uavHeapDesc =
        {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = 1,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        };

        context->device->CreateDescriptorHeap(
            &uavHeapDesc,
            IID_PPV_ARGS(&context->uavHeap));

        Resize();
    }

    void Renderer::Resize()
    {
        if (!context->swapChain) [[unlikely]]
        {
            return;
        }

        RECT rect;
        GetClientRect(hwnd, &rect);
        auto width = std::max<UINT>(rect.right - rect.left, 1);
        auto height = std::max<UINT>(rect.bottom - rect.top, 1);

        context->Flush();

        context->swapChain->ResizeBuffers(
            0,
            width,
            height,
            DXGI_FORMAT_UNKNOWN,
            0);

        if (context->renderTarget) [[likely]]
        {
            context->renderTarget->Release();
        }

        D3D12_RESOURCE_DESC rtDesc =
        {
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Width = width,
            .Height = height,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .SampleDesc = NO_AA,
            .Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
        };

        context->device->CreateCommittedResource(
            &DEFAULT_HEAP,
            D3D12_HEAP_FLAG_NONE,
            &rtDesc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nullptr,
            IID_PPV_ARGS(&context->renderTarget));

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc =
        {
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D
        };

        context->device->CreateUnorderedAccessView(
            context->renderTarget,
            nullptr,
            &uavDesc,
            context->uavHeap->GetCPUDescriptorHandleForHeapStart());
    }

    void Renderer::Render()
    {
        raytracing->Render();
    }

    void Renderer::Destroy()
    {

    }
}
