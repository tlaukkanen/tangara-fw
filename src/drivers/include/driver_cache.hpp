#pragma once

#include <memory>
#include <mutex>

#include "dac.hpp"
#include "display.hpp"
#include "gpio_expander.hpp"
#include "storage.hpp"
#include "touchwheel.hpp"

namespace drivers {

class DriverCache {
 private:
  std::unique_ptr<GpioExpander> gpios_;
  std::weak_ptr<AudioDac> dac_;
  std::weak_ptr<Display> display_;
  std::weak_ptr<SdStorage> storage_;
  std::weak_ptr<TouchWheel> touchwheel_;
  // TODO(jacqueline): Haptics, samd

  std::mutex mutex_;

  template <typename T, typename F>
  auto Acquire(std::weak_ptr<T> ptr, F factory) -> std::shared_ptr<T> {
    std::shared_ptr<T> acquired = ptr.lock();
    if (acquired) {
      return acquired;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    acquired = ptr.lock();
    if (acquired) {
      return acquired;
    }
    acquired.reset(factory());
    ptr = acquired;
    return acquired;
  }

 public:
  DriverCache();
  ~DriverCache();

  auto AcquireGpios() -> GpioExpander*;
  auto AcquireDac() -> std::shared_ptr<AudioDac>;
  auto AcquireDisplay() -> std::shared_ptr<Display>;
  auto AcquireStorage() -> std::shared_ptr<SdStorage>;
  auto AcquireTouchWheel() -> std::shared_ptr<TouchWheel>;
};

}  // namespace drivers
