#pragma once

#include <memory>

#include "console.hpp"
#include "storage.hpp"

namespace console {

class AppConsole : public Console {
 public:
  AppConsole(){};
  virtual ~AppConsole(){};

 protected:
  virtual auto RegisterExtraComponents() -> void;
};

}  // namespace console
