
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
#include <stdint.h>
#include "bluetooth_types.hpp"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_gap_bt_api.h"
#include "nvs.hpp"
#include "tasks.hpp"
#include "tinyfsm.hpp"
#include "tinyfsm/include/tinyfsm.hpp"

namespace drivers {

/*
 * A handle used to interact with the bluetooth state machine.
 */
class Bluetooth {
 public:
  Bluetooth(NvsStorage& storage, tasks::WorkerPool&);

  auto Enable() -> bool;
  auto Disable() -> void;
  auto IsEnabled() -> bool;

  auto IsConnected() -> bool;
  auto ConnectedDevice() -> std::optional<bluetooth::Device>;

  auto KnownDevices() -> std::vector<bluetooth::Device>;

  auto SetPreferredDevice(std::optional<bluetooth::MacAndName> dev) -> void;
  auto PreferredDevice() -> std::optional<bluetooth::MacAndName>;

  auto SetSource(StreamBufferHandle_t) -> void;
  auto SetVolume(uint8_t) -> void;

  auto SetEventHandler(std::function<void(bluetooth::Event)> cb) -> void;
};

namespace bluetooth {

namespace events {
struct Enable : public tinyfsm::Event {};
struct Disable : public tinyfsm::Event {};

struct ConnectTimedOut : public tinyfsm::Event {};
struct PreferredDeviceChanged : public tinyfsm::Event {};
struct SourceChanged : public tinyfsm::Event {};
struct DeviceDiscovered : public tinyfsm::Event {
  const Device& device;
};
struct ChangeVolume : public tinyfsm::Event {
  const uint8_t volume;
};

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
  esp_avrc_ct_cb_param_t param;
};
}  // namespace internal
}  // namespace events

/*
 * Utility for managing scanning, independent of the current connection state.
 */
class Scanner {
 public:
  Scanner();

  auto ScanContinuously() -> void;
  auto ScanOnce() -> void;
  auto StopScanning() -> void;
  auto StopScanningNow() -> void;

  auto HandleGapEvent(const events::internal::Gap&) -> void;

 private:
  bool enabled_;
  bool is_discovering_;

  auto HandleDeviceDiscovery(const esp_bt_gap_cb_param_t& param) -> void;
};

class BluetoothState : public tinyfsm::Fsm<BluetoothState> {
 public:
  static auto Init(NvsStorage& storage) -> void;

  static auto lock() -> std::lock_guard<std::mutex>;

  static auto devices() -> std::vector<Device>;

  static auto preferred_device() -> std::optional<bluetooth::MacAndName>;
  static auto preferred_device(std::optional<bluetooth::MacAndName>) -> void;

  static auto scanning() -> bool;
  static auto discovery() -> bool;
  static auto discovery(bool) -> void;

  static auto source() -> StreamBufferHandle_t;
  static auto source(StreamBufferHandle_t) -> void;

  static auto event_handler(std::function<void(Event)>) -> void;

  virtual ~BluetoothState(){};

  virtual void entry() {}
  virtual void exit() {}

  virtual void react(const events::Enable& ev){};
  virtual void react(const events::Disable& ev) = 0;
  virtual void react(const events::ConnectTimedOut& ev){};
  virtual void react(const events::PreferredDeviceChanged& ev){};
  virtual void react(const events::SourceChanged& ev){};
  virtual void react(const events::ChangeVolume&) {}

  virtual void react(const events::DeviceDiscovered&);

  virtual void react(const events::internal::Gap& ev) = 0;
  virtual void react(const events::internal::A2dp& ev){};
  virtual void react(const events::internal::Avrc& ev){};

 protected:
  static NvsStorage* sStorage_;
  static Scanner* sScanner_;

  static std::mutex sFsmMutex;
  static std::map<mac_addr_t, Device> sDevices_;
  static std::optional<bluetooth::MacAndName> sPreferredDevice_;

  static std::optional<bluetooth::MacAndName> sConnectingDevice_;
  static int sConnectAttemptsRemaining_;

  static std::atomic<StreamBufferHandle_t> sSource_;
  static std::function<void(Event)> sEventHandler_;

  auto connect(const bluetooth::MacAndName&) -> bool;
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

class Idle : public BluetoothState {
 public:
  void entry() override;
  void exit() override;

  void react(const events::Disable& ev) override;
  void react(const events::PreferredDeviceChanged& ev) override;

  void react(const events::internal::Gap& ev) override;

  using BluetoothState::react;
};

class Connecting : public BluetoothState {
 public:
  void entry() override;
  void exit() override;

  void react(const events::PreferredDeviceChanged& ev) override;

  void react(const events::ConnectTimedOut& ev) override;
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
  void react(const events::ChangeVolume&) override;

  void react(const events::Disable& ev) override;
  void react(const events::internal::Gap& ev) override;
  void react(const events::internal::A2dp& ev) override;
  void react(const events::internal::Avrc& ev) override;

  using BluetoothState::react;

 private:
  uint8_t transaction_num_;
  mac_addr_t connected_to_;
};

}  // namespace bluetooth

}  // namespace drivers
