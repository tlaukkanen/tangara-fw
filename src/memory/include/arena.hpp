#pragma once

#include <cstdint>
#include <optional>
#include <utility>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "span.hpp"
#include "sys/_stdint.h"

namespace memory {

class Arena;

/*
 * A pointer to data that has been given out by an Arena, plus extra accounting
 * information so that it can be returned properly.
 */
struct ArenaPtr {
  Arena* owner;
  std::byte* start;
  std::size_t size;
  // A convenience for keeping track of the subset of the block that has had
  // data placed within it.
  std::size_t used_size;
};

/*
 * A basic memory arena. This class mediates access to fixed-size blocks of
 * memory within a larger contiguous block. This is faster than re-allocating
 * smaller blocks every time they're needed, and lets us easily limit the
 * maximum size of the memory used.
 *
 * A single arena instance is safe to be used concurrently by multiple tasks,
 * however there is no built in synchronisation of the underlying memory.
 */
class Arena {
 public:
  Arena(std::size_t block_size, std::size_t num_blocks, uint32_t alloc_flags);
  ~Arena();

  /*
   * Attempts to receive an allocation from this arena. Returns absent if
   * there are no blocks left.
   */
  auto Acquire() -> std::optional<ArenaPtr>;

  /* Returns a previously allocated block to this arena. */
  auto Return(ArenaPtr) -> void;

  /* Returns the number of blocks that are currently free. */
  auto BlocksFree() -> std::size_t;

  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

 private:
  std::size_t block_size_;
  // The large memory allocation that is divided into blocks.
  std::byte* pool_;
  // A FreeRTOS queue containing the blocks that are currently unused.
  QueueHandle_t free_blocks_;
};

/*
 * Wrapper around an ArenaPtr that handles acquiring and returning the block
 * through RAII.
 */
class ArenaRef {
 public:
  static auto Acquire(Arena* a) -> std::optional<ArenaRef>;
  explicit ArenaRef(ArenaPtr ptr);
  ~ArenaRef();

  auto Release() -> ArenaPtr;

  ArenaRef(ArenaRef&&);
  ArenaRef(const ArenaRef&) = delete;
  Arena& operator=(const Arena&) = delete;

  ArenaPtr ptr;
};

}  // namespace memory
