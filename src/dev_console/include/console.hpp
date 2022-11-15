#pragma once

#include <cstdint>

namespace console {

class Console {
 public:
  Console();
  virtual ~Console();

  auto Launch() -> void;

 protected:
  virtual auto GetStackSizeKiB() -> uint16_t { return 0; }
  virtual auto RegisterExtraComponents() -> void {}

 private:
  auto RegisterCommonComponents() -> void;
};

}  // namespace console
