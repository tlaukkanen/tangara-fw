#include "bluetooth.hpp"

#include <stdint.h>

#include <atomic>
#include <ostream>
#include <sstream>

#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "tinyfsm/include/tinyfsm.hpp"

namespace drivers {

static constexpr char kTag[] = "bluetooth";

static std::atomic<StreamBufferHandle_t> sStream;

auto gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) -> void {
  tinyfsm::FsmList<bluetooth::BluetoothState>::dispatch(
      bluetooth::events::internal::Gap{.type = event, .param = param});
}

auto avrcp_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t* param)
    -> void {
  tinyfsm::FsmList<bluetooth::BluetoothState>::dispatch(
      bluetooth::events::internal::Avrc{.type = event, .param = param});
}

auto a2dp_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param) -> void {
  tinyfsm::FsmList<bluetooth::BluetoothState>::dispatch(
      bluetooth::events::internal::A2dp{.type = event, .param = param});
}

auto a2dp_data_cb(uint8_t* buf, int32_t buf_size) -> int32_t {
  if (buf == nullptr || buf_size <= 0) {
    return 0;
  }
  StreamBufferHandle_t stream = sStream.load();
  if (stream == nullptr) {
    return 0;
  }
  return xStreamBufferReceive(stream, buf, buf_size, 0);
}

Bluetooth::Bluetooth() {
  tinyfsm::FsmList<bluetooth::BluetoothState>::start();
}

auto Bluetooth::Enable() -> bool {
  tinyfsm::FsmList<bluetooth::BluetoothState>::dispatch(
      bluetooth::events::Enable{});

  return !bluetooth::BluetoothState::is_in_state<bluetooth::Disabled>();
}

auto Bluetooth::Disable() -> void {
  tinyfsm::FsmList<bluetooth::BluetoothState>::dispatch(
      bluetooth::events::Disable{});
}

auto DeviceName() -> std::string {
  uint8_t mac[8]{0};
  esp_efuse_mac_get_default(mac);
  std::ostringstream name;
  name << "TANGARA " << std::hex << mac[0] << mac[1];
  return name.str();
}

namespace bluetooth {

static bool sIsFirstEntry = true;

void Disabled::entry() {
  if (sIsFirstEntry) {
    // We only use BT Classic, to claw back ~60KiB from the BLE firmware.
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    sIsFirstEntry = false;
    return;
  }

  esp_bluedroid_disable();
  esp_bluedroid_deinit();
  esp_bt_controller_disable();
}

void Disabled::react(const events::Enable&) {
  esp_bt_controller_config_t config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  if (esp_bt_controller_init(&config) != ESP_OK) {
    ESP_LOGE(kTag, "initialize controller failed");
    return;
  }

  if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK) {
    ESP_LOGE(kTag, "enable controller failed");
    return;
  }

  if (esp_bluedroid_init() != ESP_OK) {
    ESP_LOGE(kTag, "initialize bluedroid failed");
    return;
  }

  if (esp_bluedroid_enable() != ESP_OK) {
    ESP_LOGE(kTag, "enable bluedroid failed");
    return;
  }

  // Enable Secure Simple Pairing
  esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
  esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
  esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

  // Set a reasonable name for the device.
  std::string name = DeviceName();
  esp_bt_dev_set_device_name(name.c_str());

  // Initialise GAP. This controls advertising our device, and scanning for
  // other devices.
  esp_bt_gap_register_callback(gap_cb);

  // Initialise AVRCP. This handles playback controls; play/pause/volume/etc.
  // esp_avrc_ct_init();
  // esp_avrc_ct_register_callback(avrcp_cb);

  // Initialise A2DP. This handles streaming audio. Currently ESP-IDF's SBC
  // encoder only supports 2 channels of interleaved 16 bit samples, at
  // 44.1kHz, so there is no additional configuration to be done for the
  // stream itself.
  esp_a2d_source_init();
  esp_a2d_register_callback(a2dp_cb);
  esp_a2d_source_register_data_callback(a2dp_data_cb);

  // Don't let anyone interact with us before we're ready.
  esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);

  transit<Scanning>();
}

static constexpr uint8_t kDiscoveryTimeSeconds = 10;
static constexpr uint8_t kDiscoveryMaxResults = 0;

void Scanning::entry() {
  ESP_LOGI(kTag, "scanning for devices");
  esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY,
                             kDiscoveryTimeSeconds, kDiscoveryMaxResults);
}

void Scanning::exit() {
  esp_bt_gap_cancel_discovery();
}

auto OnDeviceDiscovered(esp_bt_gap_cb_param_t* param) -> void {
  ESP_LOGI(kTag, "device discovered");
}

void Scanning::react(const events::internal::Gap& ev) {
  switch (ev.type) {
    case ESP_BT_GAP_DISC_RES_EVT:
      OnDeviceDiscovered(ev.param);
      break;
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
      if (ev.param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
        ESP_LOGI(kTag, "still scanning");
        esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY,
                                   kDiscoveryTimeSeconds, kDiscoveryMaxResults);
      }
      break;
    case ESP_BT_GAP_MODE_CHG_EVT:
      // todo: mode change. is this important?
      ESP_LOGI(kTag, "GAP mode changed");
      break;
    default:
      ESP_LOGW(kTag, "unhandled GAP event: %u", ev.type);
  }
}

void Connecting::entry() {
  ESP_LOGI(kTag, "connecting to device");
  esp_a2d_source_connect(nullptr);
}

void Connecting::exit() {}

void Connecting::react(const events::internal::Gap& ev) {
  switch (ev.type) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
      // todo: auth completed. check if we succeeded.
      break;
    case ESP_BT_GAP_PIN_REQ_EVT:
      // todo: device needs a pin to connect.
      break;
    case ESP_BT_GAP_CFM_REQ_EVT:
      // todo: device needs user to click okay.
      break;
    case ESP_BT_GAP_KEY_NOTIF_EVT:
      // todo: device is telling us a password?
      break;
    case ESP_BT_GAP_KEY_REQ_EVT:
      // todo: device needs a password
      break;
    case ESP_BT_GAP_MODE_CHG_EVT:
      // todo: mode change. is this important?
      break;
    default:
      ESP_LOGW(kTag, "unhandled GAP event: %u", ev.type);
  }
}

void Connecting::react(const events::internal::A2dp& ev) {
  switch (ev.type) {
    case ESP_A2D_CONNECTION_STATE_EVT:
      // todo: connection state changed. we might be connected!
      break;
    default:
      ESP_LOGW(kTag, "unhandled A2DP event: %u", ev.type);
  }
}

void Connected::react(const events::internal::A2dp& ev) {
  switch (ev.type) {
    case ESP_A2D_CONNECTION_STATE_EVT:
      // todo: connection state changed. we might have dropped
      break;
    case ESP_A2D_AUDIO_STATE_EVT:
      // todo: audio state changed. who knows, dude.
      break;
    default:
      ESP_LOGW(kTag, "unhandled A2DP event: %u", ev.type);
  }
}

void Connected::react(const events::internal::Avrc& ev) {
  switch (ev.type) {
    case ESP_AVRC_CT_CONNECTION_STATE_EVT:
      // todo: avrc connected. send our capabilities.
    default:
      ESP_LOGW(kTag, "unhandled AVRC event: %u", ev.type);
  }
}

}  // namespace bluetooth

}  // namespace drivers

FSM_INITIAL_STATE(drivers::bluetooth::BluetoothState,
                  drivers::bluetooth::Disabled)
