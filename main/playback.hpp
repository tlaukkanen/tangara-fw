#pragma once

#include "dac.hpp"
#include "storage.hpp"

#include <cstdint>
#include <memory>
#include <string>

#include "audio_common.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_pipeline.h"
#include "esp_err.h"
#include "fatfs_stream.h"
#include "result.hpp"
#include "i2s_stream.h"
#include "mp3_decoder.h"

namespace gay_ipod {

class DacAudioPlayback {
 public:
  enum Error { PIPELINE_INIT };
  static auto create(AudioDac* dac)
      -> cpp::result<std::unique_ptr<DacAudioPlayback>, Error>;

  DacAudioPlayback(AudioDac* dac,
                   audio_pipeline_handle_t pipeline,
                   audio_element_handle_t fatfs_stream_reader,
                   audio_element_handle_t i2s_stream_writer,
                   audio_event_iface_handle_t event_interface,
                   audio_element_handle_t mp3_decoder);
  ~DacAudioPlayback();

  void Play(const std::string& filename);
  void Resume();
  void Pause();

  // For debug :)
  void WaitForSongEnd();

  /* for gapless */
  void set_next_file(const std::string& filename);

  void set_volume(uint8_t volume);
  auto volume() -> uint8_t;

  auto HandleEvent(audio_event_iface_msg_t* event, void* data) -> esp_err_t;

  // Not copyable or movable.
  DacAudioPlayback(const DacAudioPlayback&) = delete;
  DacAudioPlayback& operator=(const DacAudioPlayback&) = delete;

 private:
  AudioDac* dac_;
  std::mutex playback_lock_;

  std::string next_filename_;
  uint8_t volume_;

  audio_pipeline_handle_t pipeline_;
  audio_element_handle_t fatfs_stream_reader_;
  audio_element_handle_t i2s_stream_writer_;
  audio_event_iface_handle_t event_interface_;

  audio_element_handle_t mp3_decoder_;
};

}  // namespace gay_ipod
