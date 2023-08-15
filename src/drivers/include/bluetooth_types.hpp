
#pragma once

#include <array>
#include <string>

namespace drivers {
namespace bluetooth {

typedef std::array<uint8_t, 6> mac_addr_t;

struct Device {
  mac_addr_t address;
  std::string name;
  uint32_t class_of_device;
  int8_t signal_strength;
};

}  // namespace bluetooth
}  // namespace drivers
