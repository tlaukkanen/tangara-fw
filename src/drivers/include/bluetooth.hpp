
#pragma once

#include <string>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_gap_bt_api.h"
#include "tinyfsm.hpp"
#include "tinyfsm/include/tinyfsm.hpp"

namespace drivers {

/*
 * A handle used to interact with the bluetooth state machine.
 */
class Bluetooth {
 public:
  Bluetooth();

  auto Enable() -> bool;
  auto Disable() -> void;

  auto SetSource(StreamBufferHandle_t) -> void;
};

namespace bluetooth {

namespace events {
struct Enable : public tinyfsm::Event {};
struct Disable : public tinyfsm::Event {};

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
  virtual ~BluetoothState(){};

  virtual void entry() {}
  virtual void exit() {}

  virtual void react(const events::Enable& ev){};
  virtual void react(const events::Disable& ev) = 0;

  virtual void react(const events::internal::Gap& ev) = 0;
  virtual void react(const events::internal::A2dp& ev) = 0;
  virtual void react(const events::internal::Avrc& ev){};
};

class Disabled : public BluetoothState {
  void entry() override;

  void react(const events::Enable& ev) override;
  void react(const events::Disable& ev) override{};

  void react(const events::internal::Gap& ev) override {}
  void react(const events::internal::A2dp& ev) override {}
};

class Scanning : public BluetoothState {
  void entry() override;
  void exit() override;

  void react(const events::Disable& ev) override;

  void react(const events::internal::Gap& ev) override;
  void react(const events::internal::A2dp& ev) override;
};

class Connecting : public BluetoothState {
  void entry() override;
  void exit() override;

  void react(const events::Disable& ev) override;
  void react(const events::internal::Gap& ev) override;
  void react(const events::internal::A2dp& ev) override;
};

class Connected : public BluetoothState {
  void entry() override;
  void exit() override;

  void react(const events::Disable& ev) override;
  void react(const events::internal::Gap& ev) override;
  void react(const events::internal::A2dp& ev) override;
  void react(const events::internal::Avrc& ev) override;
};

}  // namespace bluetooth

}  // namespace drivers
