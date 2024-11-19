#include "Descriptor.hpp"
#include <cassert>

DescriptorHandle::DescriptorHandle() : descriptor_pool_(nullptr), descriptor_heap_(nullptr), cpu_handle_({ 0 }) {}

DescriptorHandle::DescriptorHandle(DescriptorPool* pool, ID3D12DescriptorHeap* heap, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle) :
  descriptor_pool_(pool), descriptor_heap_(heap), cpu_handle_(cpu_handle) {}

// Move constructor (copy is not allowed)
DescriptorHandle::DescriptorHandle(DescriptorHandle&& other) : descriptor_pool_(nullptr), descriptor_heap_(nullptr), cpu_handle_({ 0 }) {
  descriptor_pool_ = other.descriptor_pool_;
  descriptor_heap_ = other.descriptor_heap_;
  cpu_handle_ = other.cpu_handle_;

  other.descriptor_pool_ = nullptr;
  other.descriptor_heap_ = nullptr;
  other.cpu_handle_ = { 0 };
}

// Move assignment (assignment is not allowed)
DescriptorHandle& DescriptorHandle::operator=(DescriptorHandle&& other) {
  // self assignment ignored
  if (this != &other) {
    // If we have existing handle, make sure to free it
    if (descriptor_pool_)
      descriptor_pool_->Free(this);

    descriptor_pool_ = other.descriptor_pool_;
    descriptor_heap_ = other.descriptor_heap_;
    cpu_handle_ = other.cpu_handle_;

    other.descriptor_pool_ = nullptr;
    other.descriptor_heap_ = nullptr;
    other.cpu_handle_ = { 0 };
  }

  return *this;
}

DescriptorHandle::~DescriptorHandle() {
  if (descriptor_pool_)
    descriptor_pool_->Free(this);
}

DescriptorPool::DescriptorPool(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type) : device_(device), type_(type) {}

DescriptorHandle DescriptorPool::Create() {
  if (!free_list_.empty()) {
    DescriptorHandle result = std::move(free_list_.top());
    free_list_.pop();
    return result;
  }

  const size_t count = 1;

  if (current_heap_ == nullptr || remaining_free_handles_ < count) {
    current_heap_ = CreateNewHeap();
    current_handle_ = current_heap_->GetCPUDescriptorHandleForHeapStart();
    remaining_free_handles_ = num_descriptors_per_heap_;

    if (descriptor_size_ == 0)
      descriptor_size_ = device_->GetDescriptorHandleIncrementSize(type_);
  }

  D3D12_CPU_DESCRIPTOR_HANDLE new_handle = current_handle_;
  current_handle_.ptr += count * descriptor_size_;
  remaining_free_handles_ -= count;

  return DescriptorHandle(this, current_heap_, new_handle);
}

ID3D12DescriptorHeap* DescriptorPool::CreateNewHeap() {
  D3D12_DESCRIPTOR_HEAP_DESC desc;
  desc.Type = type_;
  desc.NumDescriptors = num_descriptors_per_heap_;
  desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  desc.NodeMask = 1;

  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
  device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));
  active_heaps_.emplace_back(heap);
  return heap.Get();
}

void DescriptorPool::Free(DescriptorHandle* handle) {
  // put the handle back on free list
  free_list_.push(std::move(*handle));
}

DescriptorAllocator::DescriptorAllocator(ID3D12Device* device) {
  for (size_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
    pool_by_type_[i].reset(new DescriptorPool(device, (D3D12_DESCRIPTOR_HEAP_TYPE)i));
}

DescriptorHandle DescriptorAllocator::Create(D3D12_DESCRIPTOR_HEAP_TYPE type) {
  return pool_by_type_[(size_t)type]->Create();
}
