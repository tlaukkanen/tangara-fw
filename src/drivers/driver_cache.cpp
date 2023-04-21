#include "driver_cache.hpp"

#include <memory>
#include <mutex>

#include "display.hpp"
#include "display_init.hpp"
#include "storage.hpp"
#include "touchwheel.hpp"

namespace drivers {

DriverCache::DriverCache() : gpios_(std::make_unique<GpioExpander>()) {}
DriverCache::~DriverCache() {}

auto DriverCache::AcquireGpios() -> GpioExpander* {
  return gpios_.get();
}

auto DriverCache::AcquireDac() -> std::shared_ptr<AudioDac> {
  return Acquire(dac_, [&]() -> AudioDac* {
    return AudioDac::create(AcquireGpios()).value_or(nullptr);
  });
}

auto DriverCache::AcquireDisplay() -> std::shared_ptr<Display> {
  return Acquire(display_, [&]() -> Display* {
    return Display::create(AcquireGpios(), displays::kST7735R);
  });
}

auto DriverCache::AcquireStorage() -> std::shared_ptr<SdStorage> {
  return Acquire(storage_, [&]() -> SdStorage* {
    return SdStorage::create(AcquireGpios()).value_or(nullptr);
  });
}

auto DriverCache::AcquireTouchWheel() -> std::shared_ptr<TouchWheel> {
  return Acquire(touchwheel_,
                 [&]() -> TouchWheel* { return new TouchWheel(); });
}

}  // namespace drivers
