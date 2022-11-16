#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "audio_common.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_pipeline.h"
#include "esp_err.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "result.hpp"

#include "audio_output.hpp"
#include "dac.hpp"
#include "storage.hpp"

namespace drivers {

/*
 * Sends an I2S audio stream to the DAC. Includes basic controls for pausing
 * and resuming the stream, as well as support for gapless playback of the next
 * queued song, but does not implement any kind of sophisticated queing or
 * playback control; these should be handled at a higher level.
 */
class AudioPlayback {
 public:
  enum Error { FATFS_INIT, I2S_INIT, PIPELINE_INIT };
  static auto create(std::unique_ptr<IAudioOutput> output)
      -> cpp::result<std::unique_ptr<AudioPlayback>, Error>;

  AudioPlayback(std::unique_ptr<IAudioOutput>& output,
                audio_pipeline_handle_t pipeline,
                audio_element_handle_t source_element,
                audio_event_iface_handle_t event_interface);
  ~AudioPlayback();

  /*
   * Replaces any currently playing file with the one given, and begins
   * playback.
   *
   * Any value set in `set_next_file` is cleared by this method.
   */
  auto Play(const std::string& filename) -> void;
  /* Toogle between resumed and paused. */
  auto Toggle() -> void;
  auto Resume() -> void;
  auto Pause() -> void;

  enum PlaybackState { PLAYING, PAUSED, STOPPED };
  auto GetPlaybackState() const -> PlaybackState;

  /*
   * Handles any pending events from the underlying audio pipeline. This must
   * be called regularly in order to handle configuring the I2S stream for
   * different audio types (e.g. sample rate, bit depth), and for gapless
   * playback.
   */
  auto ProcessEvents(uint16_t max_time_ms) -> void;

  /*
   * Sets the file that should be played immediately after the current file
   * finishes. This is used for gapless playback
   */
  auto SetNextFile(const std::string& filename) -> void;

  auto SetVolume(uint8_t volume) -> void;
  auto GetVolume() const -> uint8_t;

  // Not copyable or movable.
  AudioPlayback(const AudioPlayback&) = delete;
  AudioPlayback& operator=(const AudioPlayback&) = delete;

 private:
  PlaybackState playback_state_;

  enum Decoder { NONE, MP3, AMR, OPUS, OGG, FLAC, WAV, AAC };
  auto GetDecoderForFilename(std::string filename) const -> Decoder;
  auto CreateDecoder(Decoder decoder) const -> audio_element_handle_t;
  auto ReconfigurePipeline(Decoder decoder) -> void;

  std::unique_ptr<IAudioOutput> output_;

  std::string next_filename_ = "";

  audio_pipeline_handle_t pipeline_;
  audio_element_handle_t source_element_;
  audio_event_iface_handle_t event_interface_;

  audio_element_handle_t decoder_ = nullptr;
  Decoder decoder_type_ = NONE;
};

}  // namespace drivers
