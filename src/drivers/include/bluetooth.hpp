
#pragma once

#include <array>
#include <atomic>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>
#include "bluetooth_types.hpp"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_gap_bt_api.h"
#include "nvs.hpp"
#include "tinyfsm.hpp"
#include "tinyfsm/include/tinyfsm.hpp"

namespace drivers {

/*
 * A handle used to interact with the bluetooth state machine.
 */
class Bluetooth {
 public:
  Bluetooth(NvsStorage* storage);

  auto Enable() -> bool;
  auto Disable() -> void;

  auto KnownDevices() -> std::vector<bluetooth::Device>;
  auto SetPreferredDevice(const bluetooth::mac_addr_t& mac) -> void;

  auto SetSource(StreamBufferHandle_t) -> void;
};

namespace bluetooth {

namespace events {
struct Enable : public tinyfsm::Event {};
struct Disable : public tinyfsm::Event {};

struct PreferredDeviceChanged : public tinyfsm::Event {};
struct SourceChanged : public tinyfsm::Event {};

namespace internal {
struct Gap : public tinyfsm::Event {
  esp_bt_gap_cb_event_t type;
  esp_bt_gap_cb_param_t* param;
};
struct A2dp : public tinyfsm::Event {
  esp_a2d_cb_event_t type;
  esp_a2d_cb_param_t* param;
};
struct Avrc : public tinyfsm::Event {
  esp_avrc_ct_cb_event_t type;
  esp_avrc_ct_cb_param_t* param;
};
}  // namespace internal
}  // namespace events

class BluetoothState : public tinyfsm::Fsm<BluetoothState> {
 public:
  static auto Init(NvsStorage* storage) -> void;

  static auto devices() -> std::vector<Device>;
  static auto preferred_device() -> std::optional<mac_addr_t>;
  static auto preferred_device(const mac_addr_t&) -> void;

  static auto source() -> StreamBufferHandle_t;
  static auto source(StreamBufferHandle_t) -> void;

  virtual ~BluetoothState(){};

  virtual void entry() {}
  virtual void exit() {}

  virtual void react(const events::Enable& ev){};
  virtual void react(const events::Disable& ev) = 0;
  virtual void react(const events::PreferredDeviceChanged& ev){};
  virtual void react(const events::SourceChanged& ev){};

  virtual void react(const events::internal::Gap& ev) = 0;
  virtual void react(const events::internal::A2dp& ev){};
  virtual void react(const events::internal::Avrc& ev){};

 protected:
  static NvsStorage* sStorage_;

  static std::mutex sDevicesMutex_;
  static std::map<mac_addr_t, Device> sDevices_;
  static std::optional<mac_addr_t> sPreferredDevice_;
  static mac_addr_t sCurrentDevice_;

  static std::atomic<StreamBufferHandle_t> sSource_;
};

class Disabled : public BluetoothState {
 public:
  void entry() override;

  void react(const events::Enable& ev) override;
  void react(const events::Disable& ev) override{};

  void react(const events::internal::Gap& ev) override {}
  void react(const events::internal::A2dp& ev) override {}
  using BluetoothState::react;
};

class Scanning : public BluetoothState {
 public:
  void entry() override;
  void exit() override;

  void react(const events::Disable& ev) override;
  void react(const events::PreferredDeviceChanged& ev) override;

  void react(const events::internal::Gap& ev) override;

  using BluetoothState::react;

 private:
  auto OnDeviceDiscovered(esp_bt_gap_cb_param_t*) -> void;
};

class Connecting : public BluetoothState {
 public:
  void entry() override;
  void exit() override;

  void react(const events::PreferredDeviceChanged& ev) override;

  void react(const events::Disable& ev) override;
  void react(const events::internal::Gap& ev) override;
  void react(const events::internal::A2dp& ev) override;

  using BluetoothState::react;
};

class Connected : public BluetoothState {
 public:
  void entry() override;
  void exit() override;

  void react(const events::PreferredDeviceChanged& ev) override;
  void react(const events::SourceChanged& ev) override;

  void react(const events::Disable& ev) override;
  void react(const events::internal::Gap& ev) override;
  void react(const events::internal::A2dp& ev) override;
  void react(const events::internal::Avrc& ev) override;

  using BluetoothState::react;
};

}  // namespace bluetooth

}  // namespace drivers
