#include "arena.hpp"

#include <cstdint>
#include <optional>

#include "esp_heap_caps.h"
#include "freertos/queue.h"
#include "span.hpp"

namespace memory {

Arena::Arena(std::size_t block_size,
             std::size_t num_blocks,
             uint32_t alloc_flags)
    : block_size_(block_size) {
  pool_ = static_cast<std::byte*>(
      heap_caps_malloc(block_size * num_blocks, alloc_flags));
  free_blocks_ = xQueueCreate(num_blocks, sizeof(void*));
  for (int i = 0; i < num_blocks; i++) {
    std::byte* block = pool_ + (i * block_size);
    xQueueSend(free_blocks_, &block, 0);
  }
}

Arena::~Arena() {
  // TODO: assert queue is full?
  vQueueDelete(free_blocks_);
  free(pool_);
}

auto Arena::Acquire() -> std::optional<ArenaPtr> {
  std::byte* block;
  bool result = xQueueReceive(free_blocks_, &block, 0);
  if (result) {
    ArenaPtr ptr{this, block, block_size_, 0};
    return ptr;
  } else {
    return {};
  }
}

auto Arena::Return(ArenaPtr ptr) -> void {
  assert(ptr.owner == this);
  xQueueSend(free_blocks_, &ptr.start, 0);
}

auto ArenaRef::Acquire(Arena* a) -> std::optional<ArenaRef> {
  auto ptr = a->Acquire();
  if (ptr) {
    ArenaRef ref{*ptr};
    return ref;
  }
  return {};
}

ArenaRef::ArenaRef(ArenaPtr p) : ptr(p) {}

ArenaRef::ArenaRef(ArenaRef&& other) : ptr(other.Release()) {}

auto ArenaRef::Release() -> ArenaPtr {
  auto ret = ptr;
  ptr.owner = nullptr;
  ptr.start = nullptr;
  ptr.size = 0;
  ptr.used_size = 0;
  return ret;
}

ArenaRef::~ArenaRef() {
  if (ptr.owner != nullptr) {
    ptr.owner->Return(ptr);
  }
}

}  // namespace memory
