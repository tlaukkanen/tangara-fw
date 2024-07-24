/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "drivers/nvs.hpp"

#include <cstdint>
#include <memory>

#include "cppbor.h"
#include "cppbor_parse.h"
#include "drivers/bluetooth.hpp"
#include "drivers/bluetooth_types.hpp"
#include "drivers/wm8523.hpp"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace drivers {

[[maybe_unused]] static constexpr char kTag[] = "nvm";
static constexpr uint8_t kSchemaVersion = 1;

static constexpr char kKeyVersion[] = "ver";
static constexpr char kKeyBluetoothPreferred[] = "bt_dev";
static constexpr char kKeyBluetoothVolumes[] = "bt_vols";
static constexpr char kKeyBluetoothNames[] = "bt_names";
static constexpr char kKeyOutput[] = "out";
static constexpr char kKeyBrightness[] = "bright";
static constexpr char kKeyAmpMaxVolume[] = "hp_vol_max";
static constexpr char kKeyAmpCurrentVolume[] = "hp_vol";
static constexpr char kKeyAmpLeftBias[] = "hp_bias";
static constexpr char kKeyPrimaryInput[] = "in_pri";
static constexpr char kKeyScrollSensitivity[] = "scroll";
static constexpr char kKeyLockPolarity[] = "lockpol";
static constexpr char kKeyDisplayCols[] = "dispcols";
static constexpr char kKeyDisplayRows[] = "disprows";
static constexpr char kKeyHapticMotorType[] = "hapticmtype";
static constexpr char kKeyLraCalibration[] = "lra_cali";
static constexpr char kKeyDbAutoIndex[] = "dbautoindex";
static constexpr char kKeyFastCharge[] = "fastchg";

static auto nvs_get_string(nvs_handle_t nvs, const char* key)
    -> std::optional<std::string> {
  size_t len = 0;
  if (nvs_get_blob(nvs, key, NULL, &len) != ESP_OK) {
    return {};
  }
  auto raw = std::unique_ptr<char[]>{new char[len]};
  if (nvs_get_blob(nvs, key, raw.get(), &len) != ESP_OK) {
    return {};
  }
  return {{raw.get(), len}};
}

template <>
auto Setting<uint16_t>::load(nvs_handle_t nvs) -> std::optional<uint16_t> {
  uint16_t out;
  if (nvs_get_u16(nvs, name_, &out) != ESP_OK) {
    return {};
  }
  return out;
}

template <>
auto Setting<uint16_t>::store(nvs_handle_t nvs, uint16_t v) -> void {
  nvs_set_u16(nvs, name_, v);
}

template <>
auto Setting<uint8_t>::load(nvs_handle_t nvs) -> std::optional<uint8_t> {
  uint8_t out;
  if (nvs_get_u8(nvs, name_, &out) != ESP_OK) {
    return {};
  }
  return out;
}

template <>
auto Setting<uint8_t>::store(nvs_handle_t nvs, uint8_t v) -> void {
  nvs_set_u8(nvs, name_, v);
}

template <>
auto Setting<int8_t>::load(nvs_handle_t nvs) -> std::optional<int8_t> {
  int8_t out;
  if (nvs_get_i8(nvs, name_, &out) != ESP_OK) {
    return {};
  }
  return out;
}

template <>
auto Setting<int8_t>::store(nvs_handle_t nvs, int8_t v) -> void {
  nvs_set_i8(nvs, name_, v);
}

template <>
auto Setting<bluetooth::MacAndName>::load(nvs_handle_t nvs)
    -> std::optional<bluetooth::MacAndName> {
  auto raw = nvs_get_string(nvs, name_);
  if (!raw) {
    return {};
  }
  auto [parsed, unused, err] = cppbor::parseWithViews(
      reinterpret_cast<const uint8_t*>(raw->data()), raw->size());
  if (parsed->type() != cppbor::ARRAY) {
    return {};
  }
  auto arr = parsed->asArray();
  auto mac = arr->get(1)->asViewBstr()->view();
  auto name = arr->get(0)->asViewTstr()->view();
  bluetooth::MacAndName res{
      .mac = {},
      .name = {name.begin(), name.end()},
  };
  std::copy(mac.begin(), mac.end(), res.mac.begin());
  return res;
}

template <>
auto Setting<bluetooth::MacAndName>::store(nvs_handle_t nvs,
                                           bluetooth::MacAndName v) -> void {
  cppbor::Array cbor{
      cppbor::Tstr{v.name},
      cppbor::Bstr{{v.mac.data(), v.mac.size()}},
  };
  auto encoded = cbor.encode();
  nvs_set_blob(nvs, name_, encoded.data(), encoded.size());
}

template <>
auto Setting<std::vector<bluetooth::MacAndName>>::load(nvs_handle_t nvs)
    -> std::optional<std::vector<bluetooth::MacAndName>> {
  auto raw = nvs_get_string(nvs, name_);
  if (!raw) {
    return {};
  }
  auto [parsed, unused, err] = cppbor::parseWithViews(
      reinterpret_cast<const uint8_t*>(raw->data()), raw->size());
  if (parsed->type() != cppbor::MAP) {
    return {};
  }
  std::vector<bluetooth::MacAndName> res;
  for (const auto& i : *parsed->asMap()) {
    auto mac = i.first->asViewBstr()->view();
    auto name = i.second->asViewTstr()->view();
    bluetooth::MacAndName entry{
        .mac = {},
        .name = {name.begin(), name.end()},
    };
    std::copy(mac.begin(), mac.end(), entry.mac.begin());
    res.push_back(entry);
  }
  return res;
}

template <>
auto Setting<std::vector<bluetooth::MacAndName>>::store(
    nvs_handle_t nvs,
    std::vector<bluetooth::MacAndName> v) -> void {
  cppbor::Map cbor{};
  for (const auto& i : v) {
    cbor.add(cppbor::Bstr{{i.mac.data(), i.mac.size()}}, cppbor::Tstr{i.name});
  }
  auto encoded = cbor.encode();
  nvs_set_blob(nvs, name_, encoded.data(), encoded.size());
}

template <>
auto Setting<NvsStorage::LraData>::load(nvs_handle_t nvs)
    -> std::optional<NvsStorage::LraData> {
  auto raw = nvs_get_string(nvs, name_);
  if (!raw) {
    return {};
  }
  auto [parsed, unused, err] = cppbor::parseWithViews(
      reinterpret_cast<const uint8_t*>(raw->data()), raw->size());
  if (parsed->type() != cppbor::ARRAY) {
    return {};
  }
  auto arr = parsed->asArray();
  NvsStorage::LraData data{
      .compensation = static_cast<uint8_t>(arr->get(0)->asUint()->value()),
      .back_emf = static_cast<uint8_t>(arr->get(1)->asUint()->value()),
      .gain = static_cast<uint8_t>(arr->get(2)->asUint()->value()),
  };
  return data;
}

template <>
auto Setting<NvsStorage::LraData>::store(nvs_handle_t nvs,
                                         NvsStorage::LraData v) -> void {
  cppbor::Array cbor{
      cppbor::Uint{v.compensation},
      cppbor::Uint{v.back_emf},
      cppbor::Uint{v.gain},
  };
  auto encoded = cbor.encode();
  nvs_set_blob(nvs, name_, encoded.data(), encoded.size());
}

auto NvsStorage::OpenSync() -> NvsStorage* {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
    ESP_LOGW(kTag, "partition needs initialisation");
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    ESP_LOGE(kTag, "failed to init nvm");
    return nullptr;
  }

  nvs_handle_t handle;
  if ((err = nvs_open("tangara", NVS_READWRITE, &handle)) != ESP_OK) {
    ESP_LOGE(kTag, "failed to open nvs namespace");
    return nullptr;
  }

  std::unique_ptr<NvsStorage> instance = std::make_unique<NvsStorage>(handle);
  if (instance->SchemaVersionSync() < kSchemaVersion &&
      !instance->DowngradeSchemaSync()) {
    ESP_LOGW(kTag, "failed to init namespace");
    return nullptr;
  }

  instance->Read();

  ESP_LOGI(kTag, "nvm storage initialised okay");
  return instance.release();
}

NvsStorage::NvsStorage(nvs_handle_t handle)
    : handle_(handle),
      lock_polarity_(kKeyLockPolarity),
      display_cols_(kKeyDisplayCols),
      display_rows_(kKeyDisplayRows),
      haptic_motor_type_(kKeyHapticMotorType),
      lra_calibration_(kKeyLraCalibration),
      fast_charge_(kKeyFastCharge),
      brightness_(kKeyBrightness),
      sensitivity_(kKeyScrollSensitivity),
      amp_max_vol_(kKeyAmpMaxVolume),
      amp_cur_vol_(kKeyAmpCurrentVolume),
      amp_left_bias_(kKeyAmpLeftBias),
      input_mode_(kKeyPrimaryInput),
      output_mode_(kKeyOutput),
      bt_preferred_(kKeyBluetoothPreferred),
      bt_names_(kKeyBluetoothNames),
      db_auto_index_(kKeyDbAutoIndex),
      bt_volumes_(),
      bt_volumes_dirty_(false) {}

NvsStorage::~NvsStorage() {
  nvs_close(handle_);
  nvs_flash_deinit();
}

auto NvsStorage::Read() -> void {
  std::lock_guard<std::mutex> lock{mutex_};
  lock_polarity_.read(handle_);
  display_cols_.read(handle_);
  display_rows_.read(handle_);
  haptic_motor_type_.read(handle_);
  lra_calibration_.read(handle_);
  brightness_.read(handle_);
  sensitivity_.read(handle_);
  amp_max_vol_.read(handle_);
  amp_cur_vol_.read(handle_);
  amp_left_bias_.read(handle_);
  input_mode_.read(handle_);
  output_mode_.read(handle_);
  bt_preferred_.read(handle_);
  bt_names_.read(handle_);
  db_auto_index_.read(handle_);
  readBtVolumes();
}

auto NvsStorage::Write() -> bool {
  std::lock_guard<std::mutex> lock{mutex_};
  lock_polarity_.write(handle_);
  display_cols_.write(handle_);
  display_rows_.write(handle_);
  haptic_motor_type_.write(handle_);
  lra_calibration_.write(handle_);
  brightness_.write(handle_);
  sensitivity_.write(handle_);
  amp_max_vol_.write(handle_);
  amp_cur_vol_.write(handle_);
  amp_left_bias_.write(handle_);
  input_mode_.write(handle_);
  output_mode_.write(handle_);
  bt_preferred_.write(handle_);
  bt_names_.write(handle_);
  db_auto_index_.write(handle_);
  writeBtVolumes();
  return nvs_commit(handle_) == ESP_OK;
}

auto NvsStorage::DowngradeSchemaSync() -> bool {
  ESP_LOGW(kTag, "namespace needs downgrading");
  nvs_erase_all(handle_);
  nvs_set_u8(handle_, kKeyVersion, kSchemaVersion);
  return nvs_commit(handle_);
}

auto NvsStorage::SchemaVersionSync() -> uint8_t {
  uint8_t ret;
  if (nvs_get_u8(handle_, kKeyVersion, &ret) != ESP_OK) {
    return UINT8_MAX;
  }
  return ret;
}

auto NvsStorage::LockPolarity() -> bool {
  std::lock_guard<std::mutex> lock{mutex_};
  return lock_polarity_.get().value_or(0) > 0;
}

auto NvsStorage::LockPolarity(bool p) -> void {
  std::lock_guard<std::mutex> lock{mutex_};
  lock_polarity_.set(p);
}

auto NvsStorage::HapticMotorIsErm() -> bool {
  std::lock_guard<std::mutex> lock{mutex_};
  return haptic_motor_type_.get().value_or(0) > 0;
}

auto NvsStorage::HapticMotorIsErm(bool p) -> void {
  std::lock_guard<std::mutex> lock{mutex_};
  haptic_motor_type_.set(p);
}

auto NvsStorage::LraCalibration() -> std::optional<LraData> {
  std::lock_guard<std::mutex> lock{mutex_};
  return lra_calibration_.get();
}

auto NvsStorage::LraCalibration(const LraData& d) -> void {
  std::lock_guard<std::mutex> lock{mutex_};
  lra_calibration_.set(d);
}

auto NvsStorage::DisplaySize()
    -> std::pair<std::optional<uint16_t>, std::optional<uint16_t>> {
  std::lock_guard<std::mutex> lock{mutex_};
  return std::make_pair(display_cols_.get(), display_rows_.get());
}

auto NvsStorage::DisplaySize(
    std::pair<std::optional<uint16_t>, std::optional<uint16_t>> size) -> void {
  std::lock_guard<std::mutex> lock{mutex_};
  display_cols_.set(std::move(size.first));
  display_rows_.set(std::move(size.second));
}

auto NvsStorage::PreferredBluetoothDevice()
    -> std::optional<bluetooth::MacAndName> {
  std::lock_guard<std::mutex> lock{mutex_};
  return bt_preferred_.get();
}

auto NvsStorage::PreferredBluetoothDevice(
    std::optional<bluetooth::MacAndName> dev) -> void {
  std::lock_guard<std::mutex> lock{mutex_};
  bt_preferred_.set(std::move(dev));
}

auto NvsStorage::BluetoothVolume(const bluetooth::mac_addr_t& mac) -> uint8_t {
  std::lock_guard<std::mutex> lock{mutex_};
  // Note we don't set the dirty flag here, even though it's an LRU cache, so
  // that we can avoid constantly re-writing this setting to flash when the
  // user hasn't actually been changing their volume.
  return bt_volumes_.Get(mac).value_or(50);
}

auto NvsStorage::BluetoothVolume(const bluetooth::mac_addr_t& mac, uint8_t vol)
    -> void {
  std::lock_guard<std::mutex> lock{mutex_};
  bt_volumes_dirty_ = true;
  bt_volumes_.Put(mac, vol);
}

auto NvsStorage::BluetoothNames() -> std::vector<bluetooth::MacAndName> {
  std::lock_guard<std::mutex> lock{mutex_};
  return bt_names_.get().value_or(std::vector<bluetooth::MacAndName>{});
}

auto NvsStorage::BluetoothName(const bluetooth::mac_addr_t& mac,
                               std::optional<std::string> name) -> void {
  std::lock_guard<std::mutex> lock{mutex_};
  auto val = bt_names_.get();
  if (!val) {
    val.emplace();
  }

  bool mut = false;
  bool found = false;
  for (auto it = val->begin(); it != val->end(); it++) {
    if (it->mac == mac) {
      if (name) {
        it->name = *name;
      } else {
        val->erase(it);
      }
      found = true;
      mut = true;
      break;
    }
  }

  if (!found && name) {
    val->push_back(bluetooth::MacAndName{
        .mac = mac,
        .name = *name,
    });
    mut = true;
  }

  if (mut) {
    bt_names_.set(*val);
  }
}

auto NvsStorage::OutputMode() -> Output {
  std::lock_guard<std::mutex> lock{mutex_};
  switch (output_mode_.get().value_or(0xFF)) {
    case static_cast<uint8_t>(Output::kBluetooth):
      return Output::kBluetooth;
    case static_cast<uint8_t>(Output::kHeadphones):
    default:
      return Output::kHeadphones;
  }
}

auto NvsStorage::OutputMode(Output out) -> void {
  std::lock_guard<std::mutex> lock{mutex_};
  output_mode_.set(static_cast<uint8_t>(out));
  // Always write this immediately, to guard against any crashes caused by
  // toggling the output mode.
  output_mode_.write(handle_);
  nvs_commit(handle_);
}

auto NvsStorage::FastCharge() -> bool {
  std::lock_guard<std::mutex> lock{mutex_};
  return fast_charge_.get().value_or(true);
}

auto NvsStorage::FastCharge(bool en) -> void {
  std::lock_guard<std::mutex> lock{mutex_};
  fast_charge_.set(en);
}

auto NvsStorage::ScreenBrightness() -> uint_fast8_t {
  std::lock_guard<std::mutex> lock{mutex_};
  return std::clamp<uint8_t>(brightness_.get().value_or(50), 0, 100);
}

auto NvsStorage::ScreenBrightness(uint_fast8_t val) -> void {
  std::lock_guard<std::mutex> lock{mutex_};
  brightness_.set(val);
}

auto NvsStorage::ScrollSensitivity() -> uint_fast8_t {
  std::lock_guard<std::mutex> lock{mutex_};
  return std::clamp<uint8_t>(sensitivity_.get().value_or(128), 0, 255);
}

auto NvsStorage::ScrollSensitivity(uint_fast8_t val) -> void {
  std::lock_guard<std::mutex> lock{mutex_};
  sensitivity_.set(val);
}

auto NvsStorage::AmpMaxVolume() -> uint16_t {
  std::lock_guard<std::mutex> lock{mutex_};
  return amp_max_vol_.get().value_or(wm8523::kDefaultMaxVolume);
}

auto NvsStorage::AmpMaxVolume(uint16_t val) -> void {
  std::lock_guard<std::mutex> lock{mutex_};
  amp_max_vol_.set(val);
}

auto NvsStorage::AmpCurrentVolume() -> uint16_t {
  std::lock_guard<std::mutex> lock{mutex_};
  return amp_cur_vol_.get().value_or(wm8523::kDefaultVolume);
}

auto NvsStorage::AmpCurrentVolume(uint16_t val) -> void {
  std::lock_guard<std::mutex> lock{mutex_};
  amp_cur_vol_.set(val);
}

auto NvsStorage::AmpLeftBias() -> int_fast8_t {
  std::lock_guard<std::mutex> lock{mutex_};
  return amp_left_bias_.get().value_or(0);
}

auto NvsStorage::AmpLeftBias(int_fast8_t val) -> void {
  std::lock_guard<std::mutex> lock{mutex_};
  amp_left_bias_.set(val);
}

auto NvsStorage::PrimaryInput() -> InputModes {
  std::lock_guard<std::mutex> lock{mutex_};
  switch (input_mode_.get().value_or(3)) {
    case static_cast<uint8_t>(InputModes::kButtonsOnly):
      return InputModes::kButtonsOnly;
    case static_cast<uint8_t>(InputModes::kButtonsWithWheel):
      return InputModes::kButtonsWithWheel;
    case static_cast<uint8_t>(InputModes::kDirectionalWheel):
      return InputModes::kDirectionalWheel;
    case static_cast<uint8_t>(InputModes::kRotatingWheel):
      return InputModes::kRotatingWheel;
    default:
      return InputModes::kRotatingWheel;
  }
}

auto NvsStorage::PrimaryInput(InputModes mode) -> void {
  std::lock_guard<std::mutex> lock{mutex_};
  input_mode_.set(static_cast<uint8_t>(mode));
}

auto NvsStorage::DbAutoIndex() -> bool {
  std::lock_guard<std::mutex> lock{mutex_};
  return db_auto_index_.get().value_or(true);
}

auto NvsStorage::DbAutoIndex(bool en) -> void {
  std::lock_guard<std::mutex> lock{mutex_};
  db_auto_index_.set(static_cast<uint8_t>(en));
}

class VolumesParseClient : public cppbor::ParseClient {
 public:
  VolumesParseClient(util::LruCache<10, bluetooth::mac_addr_t, uint8_t>& out)
      : state_(State::kInit), mac_(), vol_(), out_(out) {}

  ParseClient* item(std::unique_ptr<cppbor::Item>& item,
                    const uint8_t* hdrBegin,
                    const uint8_t* valueBegin,
                    const uint8_t* end) override {
    if (item->type() == cppbor::ARRAY) {
      if (state_ == State::kInit) {
        state_ = State::kRoot;
      } else if (state_ == State::kRoot) {
        state_ = State::kPair;
      }
    } else if (item->type() == cppbor::BSTR && state_ == State::kPair) {
      auto data = item->asBstr()->value();
      mac_.emplace();
      std::copy(data.begin(), data.end(), mac_->begin());
    } else if (item->type() == cppbor::UINT && state_ == State::kPair) {
      vol_ =
          std::clamp<uint64_t>(item->asUint()->unsignedValue(), 0, UINT16_MAX);
    }
    return this;
  }

  ParseClient* itemEnd(std::unique_ptr<cppbor::Item>& item,
                       const uint8_t* hdrBegin,
                       const uint8_t* valueBegin,
                       const uint8_t* end) override {
    if (item->type() == cppbor::ARRAY) {
      if (state_ == State::kRoot) {
        state_ = State::kFinished;
      } else if (state_ == State::kPair) {
        if (vol_ && mac_) {
          out_.Put(*mac_, *vol_);
        }
        mac_.reset();
        vol_.reset();
        state_ = State::kRoot;
      }
    }
    return this;
  }

  void error(const uint8_t* position,
             const std::string& errorMessage) override {}

 private:
  enum class State {
    kInit,
    kRoot,
    kPair,
    kFinished,
  };

  State state_;
  std::optional<bluetooth::mac_addr_t> mac_;
  std::optional<uint8_t> vol_;
  util::LruCache<10, bluetooth::mac_addr_t, uint8_t>& out_;
};

auto NvsStorage::readBtVolumes() -> void {
  bt_volumes_.Clear();
  auto raw = nvs_get_string(handle_, kKeyBluetoothVolumes);
  if (!raw) {
    return;
  }
  VolumesParseClient client{bt_volumes_};
  auto data = reinterpret_cast<const uint8_t*>(raw->data());
  cppbor::parse(data, data + raw->size(), &client);
}

auto NvsStorage::writeBtVolumes() -> void {
  if (!bt_volumes_dirty_) {
    return;
  }
  bt_volumes_dirty_ = false;

  cppbor::Array enc;
  auto vols_list = bt_volumes_.Get();
  for (auto vol = vols_list.rbegin(); vol < vols_list.rend(); vol++) {
    enc.add(cppbor::Array{cppbor::Bstr{{vol->first.data(), vol->first.size()}},
                          cppbor::Uint{vol->second}});
  }
  std::string encoded = enc.toString();
  nvs_set_blob(handle_, kKeyBluetoothVolumes, encoded.data(), encoded.size());
}

}  // namespace drivers
