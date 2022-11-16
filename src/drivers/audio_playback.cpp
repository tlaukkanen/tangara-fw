#include "audio_playback.hpp"

#include "audio_output.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string_view>

#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_pipeline.h"
#include "esp_err.h"

#include "aac_decoder.h"
#include "amr_decoder.h"
#include "flac_decoder.h"
#include "mp3_decoder.h"
#include "ogg_decoder.h"
#include "opus_decoder.h"
#include "wav_decoder.h"

static const char* kTag = "PLAYBACK";
static const char* kSource = "src";
static const char* kDecoder = "dec";
static const char* kSink = "sink";

static audio_element_status_t toStatus(void* status) {
  uintptr_t as_pointer_int = reinterpret_cast<uintptr_t>(status);
  return static_cast<audio_element_status_t>(as_pointer_int);
}

static bool endsWith(std::string_view str, std::string_view suffix) {
  return str.size() >= suffix.size() &&
         0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

static void toLower(std::string& str) {
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char c) { return std::tolower(c); });
}

namespace drivers {

auto AudioPlayback::create(std::unique_ptr<IAudioOutput> output)
    -> cpp::result<std::unique_ptr<AudioPlayback>, Error> {
  audio_pipeline_handle_t pipeline;
  audio_element_handle_t fatfs_stream_reader;
  audio_event_iface_handle_t event_interface;

  audio_pipeline_cfg_t pipeline_config =
      audio_pipeline_cfg_t(DEFAULT_AUDIO_PIPELINE_CONFIG());
  pipeline = audio_pipeline_init(&pipeline_config);
  if (pipeline == NULL) {
    return cpp::fail(Error::PIPELINE_INIT);
  }

  fatfs_stream_cfg_t fatfs_stream_config =
      fatfs_stream_cfg_t(FATFS_STREAM_CFG_DEFAULT());
  fatfs_stream_config.type = AUDIO_STREAM_READER;
  fatfs_stream_reader = fatfs_stream_init(&fatfs_stream_config);
  if (fatfs_stream_reader == NULL) {
    return cpp::fail(Error::FATFS_INIT);
  }

  audio_event_iface_cfg_t event_config = AUDIO_EVENT_IFACE_DEFAULT_CFG();
  event_interface = audio_event_iface_init(&event_config);

  audio_pipeline_set_listener(pipeline, event_interface);
  audio_element_msg_set_listener(fatfs_stream_reader, event_interface);
  audio_element_msg_set_listener(output->GetAudioElement(), event_interface);

  audio_pipeline_register(pipeline, fatfs_stream_reader, kSource);
  audio_pipeline_register(pipeline, output->GetAudioElement(), kSink);

  return std::make_unique<AudioPlayback>(output, pipeline, fatfs_stream_reader,
                                         event_interface);
}

AudioPlayback::AudioPlayback(std::unique_ptr<IAudioOutput>& output,
                             audio_pipeline_handle_t pipeline,
                             audio_element_handle_t source_element,
                             audio_event_iface_handle_t event_interface)
    : output_(std::move(output)),
      pipeline_(pipeline),
      source_element_(source_element),
      event_interface_(event_interface) {}

AudioPlayback::~AudioPlayback() {
  audio_pipeline_remove_listener(pipeline_);
  audio_element_msg_remove_listener(source_element_, event_interface_);
  audio_element_msg_remove_listener(output_->GetAudioElement(),
                                    event_interface_);

  audio_pipeline_stop(pipeline_);
  audio_pipeline_wait_for_stop(pipeline_);
  audio_pipeline_terminate(pipeline_);

  ReconfigurePipeline(NONE);

  audio_pipeline_unregister(pipeline_, source_element_);
  audio_pipeline_unregister(pipeline_, output_->GetAudioElement());

  audio_event_iface_destroy(event_interface_);

  audio_pipeline_deinit(pipeline_);
  audio_element_deinit(source_element_);
}

void AudioPlayback::Play(const std::string& filename) {
  output_->SetSoftMute(true);

  if (playback_state_ != STOPPED) {
    audio_pipeline_stop(pipeline_);
    audio_pipeline_wait_for_stop(pipeline_);
    audio_pipeline_terminate(pipeline_);
  }

  playback_state_ = PLAYING;
  Decoder decoder = GetDecoderForFilename(filename);
  ReconfigurePipeline(decoder);
  audio_element_set_uri(source_element_, filename.c_str());
  audio_pipeline_reset_ringbuffer(pipeline_);
  audio_pipeline_reset_elements(pipeline_);
  audio_pipeline_run(pipeline_);

  output_->SetSoftMute(false);
}

void AudioPlayback::Toggle() {
  if (playback_state_ == PLAYING) {
    Pause();
  } else if (playback_state_ == PAUSED) {
    Resume();
  }
}

void AudioPlayback::Resume() {
  if (playback_state_ == PAUSED) {
    ESP_LOGI(kTag, "resuming");
    playback_state_ = PLAYING;
    audio_pipeline_resume(pipeline_);
    output_->SetSoftMute(false);
  }
}
void AudioPlayback::Pause() {
  if (GetPlaybackState() == PLAYING) {
    ESP_LOGI(kTag, "pausing");
    output_->SetSoftMute(true);
    playback_state_ = PAUSED;
    audio_pipeline_pause(pipeline_);
  }
}

auto AudioPlayback::GetPlaybackState() const -> PlaybackState {
  return playback_state_;
}

void AudioPlayback::ProcessEvents(uint16_t max_time_ms) {
  if (playback_state_ == STOPPED) {
    return;
  }

  while (1) {
    audio_event_iface_msg_t event;
    esp_err_t err = audio_event_iface_listen(event_interface_, &event,
                                             pdMS_TO_TICKS(max_time_ms));
    if (err != ESP_OK) {
      // Error should only be timeouts, so use a 'failure' as an indication that
      // we're out of events to process.
      break;
    }

    if (event.source_type == AUDIO_ELEMENT_TYPE_ELEMENT &&
        event.source == (void*)decoder_ &&
        event.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
      audio_element_info_t music_info;
      audio_element_getinfo(decoder_, &music_info);
      ESP_LOGI(kTag, "sample_rate=%d, bits=%d, ch=%d", music_info.sample_rates,
               music_info.bits, music_info.channels);
    }

    if (event.source_type == AUDIO_ELEMENT_TYPE_ELEMENT &&
        event.source == (void*)source_element_ &&
        event.cmd == AEL_MSG_CMD_REPORT_STATUS) {
      audio_element_status_t status = toStatus(event.data);
      if (status == AEL_STATUS_STATE_FINISHED) {
        // TODO: Could we change the uri here? hmm.
        ESP_LOGI(kTag, "finished reading input.");
      }
    }

    if (event.source_type == AUDIO_ELEMENT_TYPE_ELEMENT &&
        event.source == (void*)output_->GetAudioElement() &&
        event.cmd == AEL_MSG_CMD_REPORT_STATUS) {
      audio_element_status_t status = toStatus(event.data);
      if (status == AEL_STATUS_STATE_FINISHED) {
        if (next_filename_ != "") {
          ESP_LOGI(kTag, "finished writing output. enqueing next.");
          Decoder decoder = GetDecoderForFilename(next_filename_);
          if (decoder == decoder_type_) {
            output_->SetSoftMute(true);
            audio_element_set_uri(source_element_, next_filename_.c_str());
            audio_pipeline_reset_ringbuffer(pipeline_);
            audio_pipeline_reset_elements(pipeline_);
            audio_pipeline_change_state(pipeline_, AEL_STATE_INIT);
            audio_pipeline_run(pipeline_);
            output_->SetSoftMute(true);
          } else {
            Play(next_filename_);
          }
          next_filename_ = "";
        } else {
          ESP_LOGI(kTag, "finished writing output. stopping.");
          audio_pipeline_wait_for_stop(pipeline_);
          audio_pipeline_terminate(pipeline_);
          playback_state_ = STOPPED;
        }
        return;
      }
    }

    if (event.need_free_data) {
      // AFAICT this never happens in practice, but it doesn't hurt to follow
      // the api here anyway.
      free(event.data);
    }
  }
}

void AudioPlayback::SetNextFile(const std::string& filename) {
  next_filename_ = filename;
}

void AudioPlayback::SetVolume(uint8_t volume) {
  output_->SetVolume(volume);
}

auto AudioPlayback::GetVolume() const -> uint8_t {
  return output_->GetVolume();
}

auto AudioPlayback::GetDecoderForFilename(std::string filename) const
    -> Decoder {
  toLower(filename);
  if (endsWith(filename, "mp3")) {
    return MP3;
  }
  if (endsWith(filename, "amr") || endsWith(filename, "wamr")) {
    return AMR;
  }
  if (endsWith(filename, "opus")) {
    return OPUS;
  }
  if (endsWith(filename, "ogg")) {
    return OGG;
  }
  if (endsWith(filename, "flac")) {
    return FLAC;
  }
  if (endsWith(filename, "wav")) {
    return WAV;
  }
  if (endsWith(filename, "aac") || endsWith(filename, "m4a") ||
      endsWith(filename, "ts") || endsWith(filename, "mp4")) {
    return AAC;
  }
  return NONE;
}

auto AudioPlayback::CreateDecoder(Decoder decoder) const
    -> audio_element_handle_t {
  if (decoder == MP3) {
    mp3_decoder_cfg_t config = DEFAULT_MP3_DECODER_CONFIG();
    return mp3_decoder_init(&config);
  }
  if (decoder == AMR) {
    amr_decoder_cfg_t config = DEFAULT_AMR_DECODER_CONFIG();
    return amr_decoder_init(&config);
  }
  if (decoder == OPUS) {
    opus_decoder_cfg_t config = DEFAULT_OPUS_DECODER_CONFIG();
    return decoder_opus_init(&config);
  }
  if (decoder == OGG) {
    ogg_decoder_cfg_t config = DEFAULT_OGG_DECODER_CONFIG();
    return ogg_decoder_init(&config);
  }
  if (decoder == FLAC) {
    flac_decoder_cfg_t config = DEFAULT_FLAC_DECODER_CONFIG();
    return flac_decoder_init(&config);
  }
  if (decoder == WAV) {
    wav_decoder_cfg_t config = DEFAULT_WAV_DECODER_CONFIG();
    return wav_decoder_init(&config);
  }
  if (decoder == AAC) {
    aac_decoder_cfg_t config = DEFAULT_AAC_DECODER_CONFIG();
    return aac_decoder_init(&config);
  }
  return nullptr;
}

void AudioPlayback::ReconfigurePipeline(Decoder decoder) {
  if (decoder_type_ == decoder) {
    return;
  }

  if (decoder_type_ != NONE) {
    audio_pipeline_unlink(pipeline_);
    audio_element_msg_remove_listener(decoder_, event_interface_);
    audio_pipeline_unregister(pipeline_, decoder_);
    audio_element_deinit(decoder_);
  }

  if (decoder != NONE) {
    decoder_ = CreateDecoder(decoder);
    decoder_type_ = decoder;
    audio_pipeline_register(pipeline_, decoder_, kDecoder);
    audio_element_msg_set_listener(decoder_, event_interface_);
    static const char* link_tag[3] = {kSource, kDecoder, kSink};
    audio_pipeline_link(pipeline_, &link_tag[0], 3);
  }
}

}  // namespace drivers
