#include "bluetooth.hpp"

#include <stdint.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>

#include "bluetooth_types.hpp"
#include "esp_a2dp_api.h"
#include "esp_attr.h"
#include "esp_avrc_api.h"
#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "freertos/portmacro.h"
#include "memory_resource.hpp"
#include "nvs.hpp"
#include "tinyfsm/include/tinyfsm.hpp"

namespace drivers {

static constexpr char kTag[] = "bluetooth";

DRAM_ATTR static StreamBufferHandle_t sStream = nullptr;

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

IRAM_ATTR auto a2dp_data_cb(uint8_t* buf, int32_t buf_size) -> int32_t {
  if (buf == nullptr || buf_size <= 0) {
    return 0;
  }
  StreamBufferHandle_t stream = sStream;
  if (stream == nullptr) {
    return 0;
  }
  return xStreamBufferReceive(stream, buf, buf_size, 0);
}

Bluetooth::Bluetooth(NvsStorage& storage) {
  bluetooth::BluetoothState::Init(storage);
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

auto Bluetooth::IsEnabled() -> bool {
  return !bluetooth::BluetoothState::is_in_state<bluetooth::Disabled>();
}

auto Bluetooth::KnownDevices() -> std::vector<bluetooth::Device> {
  std::vector<bluetooth::Device> out = bluetooth::BluetoothState::devices();
  std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) -> bool {
    return a.signal_strength < b.signal_strength;
  });
  return out;
}

auto Bluetooth::SetPreferredDevice(const bluetooth::mac_addr_t& mac) -> void {
  if (mac == bluetooth::BluetoothState::preferred_device()) {
    return;
  }
  bluetooth::BluetoothState::preferred_device(mac);
  tinyfsm::FsmList<bluetooth::BluetoothState>::dispatch(
      bluetooth::events::PreferredDeviceChanged{});
}

auto Bluetooth::SetSource(StreamBufferHandle_t src) -> void {
  if (src == bluetooth::BluetoothState::source()) {
    return;
  }
  bluetooth::BluetoothState::source(src);
  tinyfsm::FsmList<bluetooth::BluetoothState>::dispatch(
      bluetooth::events::SourceChanged{});
}

auto Bluetooth::SetEventHandler(std::function<void(bluetooth::Event)> cb)
    -> void {
  bluetooth::BluetoothState::event_handler(cb);
}

auto DeviceName() -> std::pmr::string {
  uint8_t mac[8]{0};
  esp_efuse_mac_get_default(mac);
  std::ostringstream name;
  name << "TANGARA " << std::hex << static_cast<int>(mac[0])
       << static_cast<int>(mac[1]);
  return std::pmr::string{name.str(), &memory::kSpiRamResource};
}

namespace bluetooth {

NvsStorage* BluetoothState::sStorage_;

std::mutex BluetoothState::sDevicesMutex_{};
std::map<mac_addr_t, Device> BluetoothState::sDevices_{};
std::optional<mac_addr_t> BluetoothState::sPreferredDevice_{};
mac_addr_t BluetoothState::sCurrentDevice_;

std::atomic<StreamBufferHandle_t> BluetoothState::sSource_;
std::function<void(Event)> BluetoothState::sEventHandler_;

auto BluetoothState::Init(NvsStorage& storage) -> void {
  sStorage_ = &storage;
  sPreferredDevice_ = storage.PreferredBluetoothDevice().get();
  tinyfsm::FsmList<bluetooth::BluetoothState>::start();
}

auto BluetoothState::devices() -> std::vector<Device> {
  std::lock_guard lock{sDevicesMutex_};
  std::vector<Device> out;
  for (const auto& device : sDevices_) {
    out.push_back(device.second);
  }
  return out;
}

auto BluetoothState::preferred_device() -> std::optional<mac_addr_t> {
  std::lock_guard lock{sDevicesMutex_};
  return sPreferredDevice_;
}

auto BluetoothState::preferred_device(const mac_addr_t& addr) -> void {
  std::lock_guard lock{sDevicesMutex_};
  sPreferredDevice_ = addr;
}

auto BluetoothState::source() -> StreamBufferHandle_t {
  std::lock_guard lock{sDevicesMutex_};
  return sSource_.load();
}

auto BluetoothState::source(StreamBufferHandle_t src) -> void {
  std::lock_guard lock{sDevicesMutex_};
  sSource_.store(src);
}

auto BluetoothState::event_handler(std::function<void(Event)> cb) -> void {
  std::lock_guard lock{sDevicesMutex_};
  sEventHandler_ = cb;
}

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
  std::pmr::string name = DeviceName();
  esp_bt_dev_set_device_name(name.c_str());

  // Initialise GAP. This controls advertising our device, and scanning for
  // other devices.
  esp_bt_gap_register_callback(gap_cb);

  // Initialise AVRCP. This handles playback controls; play/pause/volume/etc.
  esp_avrc_ct_init();
  esp_avrc_ct_register_callback(avrcp_cb);

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

void Scanning::react(const events::Disable& ev) {
  transit<Disabled>();
}

auto Scanning::OnDeviceDiscovered(esp_bt_gap_cb_param_t* param) -> void {
  Device device{};
  std::copy(std::begin(param->disc_res.bda), std::end(param->disc_res.bda),
            device.address.begin());

  // Discovery results come back to us as a grab-bag of different key/value
  // pairs. Parse these into a more structured format first so that they're
  // easier to work with.
  uint8_t* eir = nullptr;
  for (size_t i = 0; i < param->disc_res.num_prop; i++) {
    esp_bt_gap_dev_prop_t& property = param->disc_res.prop[i];
    switch (property.type) {
      case ESP_BT_GAP_DEV_PROP_BDNAME:
        // Ignored -- we get the device name from the EIR field instead.
        break;
      case ESP_BT_GAP_DEV_PROP_COD:
        device.class_of_device = *reinterpret_cast<uint32_t*>(property.val);
        break;
      case ESP_BT_GAP_DEV_PROP_RSSI:
        device.signal_strength = *reinterpret_cast<int8_t*>(property.val);
        break;
      case ESP_BT_GAP_DEV_PROP_EIR:
        eir = reinterpret_cast<uint8_t*>(property.val);
        break;
      default:
        ESP_LOGW(kTag, "unknown GAP param %u", property.type);
    }
  }

  // Ignore devices with missing or malformed data.
  if (!esp_bt_gap_is_valid_cod(device.class_of_device) || eir == nullptr) {
    return;
  }

  // Note: ESP-IDF example code does additional filterering by class of device
  // at this point. We don't! Per the Bluetooth spec; "No assumptions should be
  // made about specific functionality or characteristics of any application
  // based solely on the assignment of the Major or Minor device class."

  // Resolve the name of the device.
  uint8_t* name;
  uint8_t length;
  name = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME,
                                     &length);
  if (!name) {
    name = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME,
                                       &length);
  }

  if (!name) {
    return;
  }

  device.name = std::pmr::string{reinterpret_cast<char*>(name),
                                 static_cast<size_t>(length)};

  bool is_preferred = false;
  {
    std::lock_guard<std::mutex> lock{sDevicesMutex_};
    sDevices_[device.address] = device;

    if (device.address == sPreferredDevice_) {
      sCurrentDevice_ = device.address;
      is_preferred = true;
    }

    if (sEventHandler_) {
      std::invoke(sEventHandler_, Event::kKnownDevicesChanged);
    }
  }

  if (is_preferred) {
    transit<Connecting>();
  }
}

void Scanning::react(const events::PreferredDeviceChanged& ev) {
  bool is_discovered = false;
  {
    std::lock_guard<std::mutex> lock{sDevicesMutex_};
    if (sPreferredDevice_ && sDevices_.contains(sPreferredDevice_.value())) {
      is_discovered = true;
    }
  }
  if (is_discovered) {
    transit<Connecting>();
  }
}

void Scanning::react(const events::internal::Gap& ev) {
  switch (ev.type) {
    case ESP_BT_GAP_DISC_RES_EVT:
      OnDeviceDiscovered(ev.param);
      break;
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
      if (ev.param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
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
  esp_a2d_source_connect(sPreferredDevice_.value().data());
}

void Connecting::exit() {}

void Connecting::react(const events::Disable& ev) {
  // TODO: disconnect gracefully
}

void Connecting::react(const events::PreferredDeviceChanged& ev) {
  // TODO. Cancel out and start again.
}

void Connecting::react(const events::internal::Gap& ev) {
  switch (ev.type) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
      if (ev.param->auth_cmpl.stat != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(kTag, "auth failed");
        sPreferredDevice_ = {};
        transit<Scanning>();
      }
      break;
    case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
      // ACL connection complete. We're now ready to send data to this
      // device(?)
      break;
    case ESP_BT_GAP_PIN_REQ_EVT:
      ESP_LOGW(kTag, "device needs a pin to connect");
      sPreferredDevice_ = {};
      transit<Scanning>();
      break;
    case ESP_BT_GAP_CFM_REQ_EVT:
      ESP_LOGW(kTag, "user needs to do cfm. idk man.");
      sPreferredDevice_ = {};
      transit<Scanning>();
      break;
    case ESP_BT_GAP_KEY_NOTIF_EVT:
      ESP_LOGW(kTag, "the device is telling us a password??");
      sPreferredDevice_ = {};
      transit<Scanning>();
      break;
    case ESP_BT_GAP_KEY_REQ_EVT:
      ESP_LOGW(kTag, "the device wants a password!");
      sPreferredDevice_ = {};
      transit<Scanning>();
      break;
    case ESP_BT_GAP_MODE_CHG_EVT:
      ESP_LOGI(kTag, "GAP mode changed");
      break;
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
      // Discovery state changed. Probably because we stopped scanning, but
      // either way this isn't actionable or useful.
      break;
    case ESP_BT_GAP_DISC_RES_EVT:
      // New device discovered. We could actually process this so that the
      // device list remains fresh whilst we're connecting, but for now just
      // ignore it.
      break;
    default:
      ESP_LOGW(kTag, "unhandled GAP event: %u", ev.type);
  }
}

void Connecting::react(const events::internal::A2dp& ev) {
  switch (ev.type) {
    case ESP_A2D_CONNECTION_STATE_EVT:
      if (ev.param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
        ESP_LOGI(kTag, "connected okay!");
        transit<Connected>();
      }
      break;
    case ESP_A2D_REPORT_SNK_DELAY_VALUE_EVT:
      // The sink is telling us how much of a delay to expect with playback.
      // We don't care about this yet.
      break;
    default:
      ESP_LOGW(kTag, "unhandled A2DP event: %u", ev.type);
  }
}

void Connected::entry() {
  ESP_LOGI(kTag, "entering connected state");

  auto stored_pref = sStorage_->PreferredBluetoothDevice().get();
  if (stored_pref != sPreferredDevice_) {
    sStorage_->PreferredBluetoothDevice(sPreferredDevice_);
  }
  // TODO: if we already have a source, immediately start playing
}

void Connected::exit() {
  ESP_LOGI(kTag, "exiting connected state");
}

void Connected::react(const events::Disable& ev) {
  // TODO: disconnect gracefully
}

void Connected::react(const events::PreferredDeviceChanged& ev) {
  // TODO: disconnect, move to connecting? or scanning?
}

void Connected::react(const events::SourceChanged& ev) {
  sStream = sSource_;
  if (sStream != nullptr) {
    ESP_LOGI(kTag, "checking source is ready");
    esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
  } else {
    esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
  }
}

void Connected::react(const events::internal::Gap& ev) {
  switch (ev.type) {
    case ESP_BT_GAP_MODE_CHG_EVT:
      // todo: is this important?
      ESP_LOGI(kTag, "GAP mode changed");
      break;
    default:
      ESP_LOGW(kTag, "unhandled GAP event: %u", ev.type);
  }
}

void Connected::react(const events::internal::A2dp& ev) {
  switch (ev.type) {
    case ESP_A2D_CONNECTION_STATE_EVT:
      if (ev.param->conn_stat.state != ESP_A2D_CONNECTION_STATE_CONNECTED &&
          ev.param->conn_stat.state != ESP_A2D_CONNECTION_STATE_DISCONNECTING) {
        ESP_LOGE(kTag, "a2dp connection dropped :(");
        transit<Connecting>();
      }
      break;
    case ESP_A2D_AUDIO_STATE_EVT:
      // todo: audio state changed. who knows, dude.
      break;
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
      // Sink is responding to our media control request.
      if (ev.param->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY) {
        // TODO: check if success
        ESP_LOGI(kTag, "starting playback");
        esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
      }
      break;
    default:
      ESP_LOGW(kTag, "unhandled A2DP event: %u", ev.type);
  }
}

void Connected::react(const events::internal::Avrc& ev) {
  switch (ev.type) {
    case ESP_AVRC_CT_CONNECTION_STATE_EVT:
      if (ev.param->conn_stat.connected) {
        // TODO: tell the target about our capabilities
      }
      // Don't worry about disconnect events; if there's a serious problem then
      // the entire bluetooth connection will drop out, which is handled
      // elsewhere.
      break;
    case ESP_AVRC_CT_REMOTE_FEATURES_EVT:
      // The remote device is telling us about its capabilities! We don't
      // currently care about any of them.
      break;
    default:
      ESP_LOGW(kTag, "unhandled AVRC event: %u", ev.type);
  }
}

}  // namespace bluetooth

}  // namespace drivers

FSM_INITIAL_STATE(drivers::bluetooth::BluetoothState,
                  drivers::bluetooth::Disabled)
