#pragma once

#include <d3d12.h>

#include "Context.hpp"
#include "Allocator.hpp"
#include "Descriptor.hpp"

namespace d3d12
{
    struct ConstantBuffer
    {
        D3D12MA::ResourcePtr buffer;
        DescriptorHandle cbv_handle;
    };

    template<typename T>
    class ConstantBufferPool
    {
    public:
        std::vector<ConstantBuffer> pool;
        size_t next_idx_ = 0;

        void Reset()
        {
            next_idx_ = 0;
        }

        // Gets a ConstantBuffer from the pool or creates one if none available.
        ConstantBuffer* GetBufferForWriting(Context* ctx)
        {
            if (pool.size() > next_idx_)
            {
                return &pool[next_idx_++];
            }

            // No available entries in pool, need to create new
            ConstantBuffer cb;

            D3D12MA::ALLOCATION_DESC allocation_desc = {};
            allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

            auto buffer = CD3DX12_RESOURCE_DESC::Buffer(sizeof(T));

            D3D12MA::Allocation* alloc = nullptr;
            // LogIfFailed
            ctx->allocator->CreateResource(
                &allocation_desc,
                &buffer,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                NULL,
                &alloc,
                __uuidof(ID3D12Resource),
                nullptr);

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
            cbv_desc.BufferLocation = alloc->GetResource()->GetGPUVirtualAddress();
            cbv_desc.SizeInBytes = sizeof(T);

            cb.cbv_handle = ctx->descriptor_allocator->Create(
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            ctx->device->CreateConstantBufferView(
                &cbv_desc,
                cb.cbv_handle.cpu_handle());

            // Take ownership of alloc
            cb.buffer.reset(alloc);

            pool.emplace_back(std::move(cb));
            next_idx_++;
            return &pool.back();
        }

        DescriptorHandle* GetBuffer(Context* ctx, const void* uniforms)
        {
            ConstantBuffer* cb = GetBufferForWriting(ctx);

            ID3D12Resource* resource = cb->buffer->GetResource();
            UINT8* pData;
            CD3DX12_RANGE readRange(0, 0);
            //LogIfFailed
            resource->Map(0, &readRange, reinterpret_cast<void**>(&pData));
            memcpy(pData, uniforms, sizeof(T));
            resource->Unmap(0, nullptr);

            return &cb->cbv_handle;
        }
    };
}
