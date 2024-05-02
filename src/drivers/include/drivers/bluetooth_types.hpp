
#pragma once

#include <array>
#include <string>

#include "memory_resource.hpp"

namespace drivers {
namespace bluetooth {

typedef std::array<uint8_t, 6> mac_addr_t;

struct MacAndName {
  mac_addr_t mac;
  std::string name;

  bool operator==(const MacAndName&) const = default;
};

struct Device {
  mac_addr_t address;
  std::pmr::string name;
  uint32_t class_of_device;
  int8_t signal_strength;
};

enum class Event {
  kKnownDevicesChanged,
  kConnectionStateChanged,
  kPreferredDeviceChanged,
};

}  // namespace bluetooth
}  // namespace drivers
