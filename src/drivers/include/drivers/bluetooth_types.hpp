
#pragma once

#include <array>
#include <string>

#include "memory_resource.hpp"
#include <variant>

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

enum class SimpleEvent {
  kKnownDevicesChanged,
  kConnectionStateChanged,
  kPreferredDeviceChanged,
  // Passthrough events
  kPlayPause,
  kStop,
  kMute,
  kVolUp,
  kVolDown,
  kForward,
  kBackward,
  kRewind,
  kFastForward,
};

struct RemoteVolumeChanged {
  uint8_t new_vol;
};

using Event = std::variant<SimpleEvent, RemoteVolumeChanged>;

}  // namespace bluetooth
}  // namespace drivers
