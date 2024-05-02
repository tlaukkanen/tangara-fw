#include "bluetooth.hpp"

#include <stdint.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>

#include "esp_a2dp_api.h"
#include "esp_attr.h"
#include "esp_avrc_api.h"
#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_bt_api.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/timers.h"
#include "tinyfsm/include/tinyfsm.hpp"

#include "bluetooth_types.hpp"
#include "memory_resource.hpp"
#include "nvs.hpp"
#include "tasks.hpp"

namespace drivers {

[[maybe_unused]] static constexpr char kTag[] = "bluetooth";

DRAM_ATTR static StreamBufferHandle_t sStream = nullptr;
DRAM_ATTR static std::atomic<float> sVolumeFactor = 1.f;

static tasks::WorkerPool* sBgWorker;

auto gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) -> void {
  auto lock = bluetooth::BluetoothState::lock();
  tinyfsm::FsmList<bluetooth::BluetoothState>::dispatch(
      bluetooth::events::internal::Gap{.type = event, .param = param});
}

auto avrcp_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t* param)
    -> void {
  esp_avrc_ct_cb_param_t copy = *param;
  sBgWorker->Dispatch<void>([=]() {
    auto lock = bluetooth::BluetoothState::lock();
    tinyfsm::FsmList<bluetooth::BluetoothState>::dispatch(
        bluetooth::events::internal::Avrc{.type = event, .param = copy});
  });
}

auto a2dp_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param) -> void {
  auto lock = bluetooth::BluetoothState::lock();
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
  size_t bytes_received = xStreamBufferReceive(stream, buf, buf_size, 0);

  // Apply software volume scaling.
  int16_t* samples = reinterpret_cast<int16_t*>(buf);
  float factor = sVolumeFactor.load();
  for (size_t i = 0; i < bytes_received / 2; i++) {
    samples[i] *= factor;
  }

  return bytes_received;
}

Bluetooth::Bluetooth(NvsStorage& storage, tasks::WorkerPool& bg_worker) {
  sBgWorker = &bg_worker;
  bluetooth::BluetoothState::Init(storage);
}

auto Bluetooth::Enable() -> bool {
  auto lock = bluetooth::BluetoothState::lock();
  tinyfsm::FsmList<bluetooth::BluetoothState>::dispatch(
      bluetooth::events::Enable{});

  return !bluetooth::BluetoothState::is_in_state<bluetooth::Disabled>();
}

auto Bluetooth::Disable() -> void {
  // FIXME: the BT tasks unfortunately call back into us while holding an
  // internal lock, which then deadlocks with our fsm lock.
  // auto lock = bluetooth::BluetoothState::lock();
  tinyfsm::FsmList<bluetooth::BluetoothState>::dispatch(
      bluetooth::events::Disable{});
}

auto Bluetooth::IsEnabled() -> bool {
  auto lock = bluetooth::BluetoothState::lock();
  return !bluetooth::BluetoothState::is_in_state<bluetooth::Disabled>();
}

auto Bluetooth::IsConnected() -> bool {
  auto lock = bluetooth::BluetoothState::lock();
  return bluetooth::BluetoothState::is_in_state<bluetooth::Connected>();
}

auto Bluetooth::ConnectedDevice() -> std::optional<bluetooth::MacAndName> {
  auto lock = bluetooth::BluetoothState::lock();
  if (!bluetooth::BluetoothState::is_in_state<bluetooth::Connected>()) {
    return {};
  }
  return bluetooth::BluetoothState::preferred_device();
}

auto Bluetooth::KnownDevices() -> std::vector<bluetooth::Device> {
  auto lock = bluetooth::BluetoothState::lock();
  std::vector<bluetooth::Device> out = bluetooth::BluetoothState::devices();
  std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) -> bool {
    return a.signal_strength < b.signal_strength;
  });
  return out;
}

auto Bluetooth::SetPreferredDevice(std::optional<bluetooth::MacAndName> dev)
    -> void {
  auto lock = bluetooth::BluetoothState::lock();
  auto cur = bluetooth::BluetoothState::preferred_device();
  if (dev && cur && dev->mac == cur->mac) {
    return;
  }
  ESP_LOGI(kTag, "preferred is '%s' (%u%u%u%u%u%u)", dev->name.c_str(),
           dev->mac[0], dev->mac[1], dev->mac[2], dev->mac[3], dev->mac[4],
           dev->mac[5]);
  bluetooth::BluetoothState::preferred_device(dev);
  tinyfsm::FsmList<bluetooth::BluetoothState>::dispatch(
      bluetooth::events::PreferredDeviceChanged{});
}

auto Bluetooth::PreferredDevice() -> std::optional<bluetooth::MacAndName> {
  auto lock = bluetooth::BluetoothState::lock();
  return bluetooth::BluetoothState::preferred_device();
}

auto Bluetooth::SetSource(StreamBufferHandle_t src) -> void {
  auto lock = bluetooth::BluetoothState::lock();
  if (src == bluetooth::BluetoothState::source()) {
    return;
  }
  bluetooth::BluetoothState::source(src);
  tinyfsm::FsmList<bluetooth::BluetoothState>::dispatch(
      bluetooth::events::SourceChanged{});
}

auto Bluetooth::SetVolumeFactor(float f) -> void {
  sVolumeFactor = f;
}

auto Bluetooth::SetEventHandler(std::function<void(bluetooth::Event)> cb)
    -> void {
  auto lock = bluetooth::BluetoothState::lock();
  bluetooth::BluetoothState::event_handler(cb);
}

static auto DeviceName() -> std::pmr::string {
  uint8_t mac[8]{0};
  esp_efuse_mac_get_default(mac);
  std::ostringstream name;
  name << "TANGARA " << std::hex << static_cast<int>(mac[0])
       << static_cast<int>(mac[1]);
  return std::pmr::string{name.str(), &memory::kSpiRamResource};
}

namespace bluetooth {

static constexpr uint8_t kDiscoveryTimeSeconds = 5;
static constexpr uint8_t kDiscoveryMaxResults = 0;

Scanner::Scanner() : enabled_(false), is_discovering_(false) {}

auto Scanner::ScanContinuously() -> void {
  if (enabled_) {
    return;
  }
  ESP_LOGI(kTag, "beginning continuous scan");
  enabled_ = true;
  if (enabled_ && !is_discovering_) {
    ScanOnce();
  }
}

auto Scanner::ScanOnce() -> void {
  if (is_discovering_) {
    return;
  }
  is_discovering_ = true;
  ESP_LOGI(kTag, "scanning...");
  esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY,
                             kDiscoveryTimeSeconds, kDiscoveryMaxResults);
}

auto Scanner::StopScanning() -> void {
  ESP_LOGI(kTag, "stopping scan");
  enabled_ = false;
}

auto Scanner::StopScanningNow() -> void {
  StopScanning();
  if (is_discovering_) {
    ESP_LOGI(kTag, "cancelling scan");
    is_discovering_ = false;
    esp_bt_gap_cancel_discovery();
  }
}

auto Scanner::HandleGapEvent(const events::internal::Gap& ev) -> void {
  switch (ev.type) {
    case ESP_BT_GAP_DISC_RES_EVT:
      if (ev.param != nullptr) {
        // Handle device discovery even if we've been told to stop discovering.
        HandleDeviceDiscovery(*ev.param);
      }
      break;
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
      if (ev.param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
        ESP_LOGI(kTag, "discovery finished");
        if (enabled_) {
          ESP_LOGI(kTag, "restarting discovery");
          esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY,
                                     kDiscoveryTimeSeconds,
                                     kDiscoveryMaxResults);
        } else {
          is_discovering_ = false;
        }
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

auto Scanner::HandleDeviceDiscovery(const esp_bt_gap_cb_param_t& param)
    -> void {
  Device device{};
  std::copy(std::begin(param.disc_res.bda), std::end(param.disc_res.bda),
            device.address.begin());

  // Discovery results come back to us as a grab-bag of different key/value
  // pairs. Parse these into a more structured format first so that they're
  // easier to work with.
  uint8_t* eir = nullptr;
  for (size_t i = 0; i < param.disc_res.num_prop; i++) {
    esp_bt_gap_dev_prop_t& property = param.disc_res.prop[i];
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
  events::DeviceDiscovered ev{.device = device};
  tinyfsm::FsmList<bluetooth::BluetoothState>::dispatch(ev);
}

NvsStorage* BluetoothState::sStorage_;
Scanner* BluetoothState::sScanner_;

std::mutex BluetoothState::sFsmMutex{};
std::map<mac_addr_t, Device> BluetoothState::sDevices_{};
std::optional<MacAndName> BluetoothState::sPreferredDevice_{};
std::optional<MacAndName> BluetoothState::sConnectingDevice_{};
int BluetoothState::sConnectAttemptsRemaining_{0};

std::atomic<StreamBufferHandle_t> BluetoothState::sSource_;
std::function<void(Event)> BluetoothState::sEventHandler_;

auto BluetoothState::Init(NvsStorage& storage) -> void {
  sStorage_ = &storage;
  sPreferredDevice_ = storage.PreferredBluetoothDevice();
  tinyfsm::FsmList<bluetooth::BluetoothState>::start();
}

auto BluetoothState::lock() -> std::lock_guard<std::mutex> {
  return std::lock_guard<std::mutex>{sFsmMutex};
}

auto BluetoothState::devices() -> std::vector<Device> {
  std::vector<Device> out;
  for (const auto& device : sDevices_) {
    out.push_back(device.second);
  }
  return out;
}

auto BluetoothState::preferred_device() -> std::optional<MacAndName> {
  return sPreferredDevice_;
}

auto BluetoothState::preferred_device(std::optional<MacAndName> addr) -> void {
  sPreferredDevice_ = addr;
}

auto BluetoothState::source() -> StreamBufferHandle_t {
  return sSource_.load();
}

auto BluetoothState::source(StreamBufferHandle_t src) -> void {
  sSource_.store(src);
}

auto BluetoothState::event_handler(std::function<void(Event)> cb) -> void {
  sEventHandler_ = cb;
}

auto BluetoothState::react(const events::DeviceDiscovered& ev) -> void {
  bool is_preferred = false;
  bool already_known = sDevices_.contains(ev.device.address);
  sDevices_[ev.device.address] = ev.device;

  if (sPreferredDevice_ && ev.device.address == sPreferredDevice_->mac) {
    is_preferred = true;
  }

  if (sEventHandler_ && !already_known) {
    std::invoke(sEventHandler_, Event::kKnownDevicesChanged);
  }

  if (is_preferred && sPreferredDevice_) {
    connect(*sPreferredDevice_);
  }
}

auto BluetoothState::connect(const MacAndName& dev) -> bool {
  if (sConnectingDevice_ && sConnectingDevice_->mac == dev.mac) {
    sConnectAttemptsRemaining_--;
  } else {
    sConnectAttemptsRemaining_ = 3;
  }

  if (sConnectAttemptsRemaining_ == 0) {
    return false;
  }

  sConnectingDevice_ = dev;
  ESP_LOGI(kTag, "connecting to '%s' (%u%u%u%u%u%u)", dev.name.c_str(),
           dev.mac[0], dev.mac[1], dev.mac[2], dev.mac[3], dev.mac[4],
           dev.mac[5]);
  if (esp_a2d_source_connect(sConnectingDevice_->mac.data()) != ESP_OK) {
    return false;
  }

  transit<Connecting>();
  return true;
}

static bool sIsFirstEntry = true;

void Disabled::entry() {
  if (sIsFirstEntry) {
    // We only use BT Classic, to claw back ~60KiB from the BLE firmware.
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    sScanner_ = new Scanner();
    sIsFirstEntry = false;
    return;
  }

  sScanner_->StopScanningNow();

  esp_a2d_source_deinit();
  esp_avrc_ct_deinit();
  esp_bluedroid_disable();
  esp_bluedroid_deinit();
  esp_bt_controller_disable();
  esp_bt_controller_deinit();
}

void Disabled::react(const events::Enable&) {
  esp_bt_controller_config_t config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  esp_err_t err;
  if ((err = esp_bt_controller_init(&config) != ESP_OK)) {
    ESP_LOGE(kTag, "initialize controller failed %s", esp_err_to_name(err));
    return;
  }

  if ((err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK)) {
    ESP_LOGE(kTag, "enable controller failed %s", esp_err_to_name(err));
    return;
  }

  esp_bluedroid_config_t cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
  if ((err = esp_bluedroid_init_with_cfg(&cfg) != ESP_OK)) {
    ESP_LOGE(kTag, "initialize bluedroid failed %s", esp_err_to_name(err));
    return;
  }

  if ((err = esp_bluedroid_enable() != ESP_OK)) {
    ESP_LOGE(kTag, "enable bluedroid failed %s", esp_err_to_name(err));
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

  ESP_LOGI(kTag, "bt enabled");
  if (sPreferredDevice_) {
    ESP_LOGI(kTag, "connecting to preferred device '%s'",
             sPreferredDevice_->name.c_str());
    connect(*sPreferredDevice_);
  } else {
    ESP_LOGI(kTag, "scanning for devices");
    transit<Idle>();
  }
}

void Idle::entry() {
  ESP_LOGI(kTag, "bt is idle");
  sScanner_->ScanContinuously();
}

void Idle::exit() {
  sScanner_->StopScanning();
}

void Idle::react(const events::Disable& ev) {
  transit<Disabled>();
}

void Idle::react(const events::PreferredDeviceChanged& ev) {
  bool is_discovered = false;
  if (sPreferredDevice_ && sDevices_.contains(sPreferredDevice_->mac)) {
    is_discovered = true;
  }
  if (is_discovered) {
    connect(*sPreferredDevice_);
  }
}

void Idle::react(const events::internal::Gap& ev) {
  sScanner_->HandleGapEvent(ev);
}

TimerHandle_t sTimeoutTimer;

static void timeoutCallback(TimerHandle_t) {
  sBgWorker->Dispatch<void>([]() {
    auto lock = bluetooth::BluetoothState::lock();
    tinyfsm::FsmList<bluetooth::BluetoothState>::dispatch(
        events::ConnectTimedOut{});
  });
}

void Connecting::entry() {
  sTimeoutTimer = xTimerCreate("bt_timeout", pdMS_TO_TICKS(15000), false, NULL,
                               timeoutCallback);
  xTimerStart(sTimeoutTimer, portMAX_DELAY);

  sScanner_->StopScanning();
  if (sEventHandler_) {
    std::invoke(sEventHandler_, Event::kConnectionStateChanged);
  }
}

void Connecting::exit() {
  xTimerDelete(sTimeoutTimer, portMAX_DELAY);

  if (sEventHandler_) {
    std::invoke(sEventHandler_, Event::kConnectionStateChanged);
  }
}

void Connecting::react(const events::ConnectTimedOut& ev) {
  ESP_LOGI(kTag, "timed out awaiting connection");
  esp_a2d_source_disconnect(sConnectingDevice_->mac.data());
  if (!connect(*sConnectingDevice_)) {
    transit<Idle>();
  }
}

void Connecting::react(const events::Disable& ev) {
  // TODO: disconnect gracefully
  transit<Disabled>();
}

void Connecting::react(const events::PreferredDeviceChanged& ev) {
  // TODO. Cancel out and start again.
}

void Connecting::react(const events::internal::Gap& ev) {
  sScanner_->HandleGapEvent(ev);
  switch (ev.type) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
      if (ev.param->auth_cmpl.stat != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(kTag, "auth failed");
        transit<Idle>();
      }
      break;
    case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
      // ACL connection complete. We're now ready to send data to this
      // device(?)
      break;
    case ESP_BT_GAP_PIN_REQ_EVT:
      ESP_LOGW(kTag, "device needs a pin to connect");
      transit<Idle>();
      break;
    case ESP_BT_GAP_CFM_REQ_EVT:
      // FIXME: Expose a UI for this instead of auto-accepting.
      ESP_LOGW(kTag, "CFM request, PIN is: %lu", ev.param->cfm_req.num_val);
      esp_bt_gap_ssp_confirm_reply(ev.param->cfm_req.bda, true);
      break;
    case ESP_BT_GAP_KEY_NOTIF_EVT:
      ESP_LOGW(kTag, "the device is telling us a password??");
      transit<Idle>();
      break;
    case ESP_BT_GAP_KEY_REQ_EVT:
      ESP_LOGW(kTag, "the device wants a password!");
      transit<Idle>();
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

  transaction_num_ = 0;
  connected_to_ = sConnectingDevice_->mac;
  sPreferredDevice_ = sConnectingDevice_;
  sConnectingDevice_ = {};

  auto stored_pref = sStorage_->PreferredBluetoothDevice();
  if (!stored_pref || (sPreferredDevice_->name != stored_pref->name ||
                       sPreferredDevice_->mac != stored_pref->mac)) {
    sStorage_->PreferredBluetoothDevice(sPreferredDevice_);
  }
  // TODO: if we already have a source, immediately start playing
}

void Connected::exit() {
  ESP_LOGI(kTag, "exiting connected state");
  esp_a2d_source_disconnect(connected_to_.data());
}

void Connected::react(const events::Disable& ev) {
  transit<Disabled>();
}

void Connected::react(const events::PreferredDeviceChanged& ev) {
  sConnectingDevice_ = sPreferredDevice_;
  transit<Connecting>();
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
  sScanner_->HandleGapEvent(ev);
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
      if (ev.param.conn_stat.connected) {
        // TODO: tell the target about our capabilities
      }
      // Don't worry about disconnect events; if there's a serious problem
      // then the entire bluetooth connection will drop out, which is handled
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
