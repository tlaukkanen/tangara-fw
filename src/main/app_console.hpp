#pragma once

#include <memory>

#include "storage.hpp"
#include "console.hpp"

namespace console {

class AppConsole : public Console {
 public:
  AppConsole() {};
  virtual ~AppConsole() {};

 protected:
  virtual auto RegisterExtraComponents() -> void;
};

}  // namespace console
