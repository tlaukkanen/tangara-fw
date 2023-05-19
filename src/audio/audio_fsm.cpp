#include "audio_fsm.hpp"
#include "audio_decoder.hpp"
#include "audio_task.hpp"
#include "dac.hpp"
#include "fatfs_audio_input.hpp"
#include "i2s_audio_output.hpp"
#include "pipeline.hpp"

namespace audio {

drivers::GpioExpander* AudioState::sGpioExpander;
std::weak_ptr<drivers::AudioDac> AudioState::sDac;
std::weak_ptr<database::Database> AudioState::sDatabase;

std::unique_ptr<FatfsAudioInput> AudioState::sFileSource;
std::unique_ptr<I2SAudioOutput> AudioState::sI2SOutput;
std::vector<std::unique_ptr<IAudioElement>> AudioState::sPipeline;

auto AudioState::Init(drivers::GpioExpander* gpio_expander,
                      std::weak_ptr<drivers::AudioDac> dac,
                      std::weak_ptr<database::Database> database) -> void {
  sGpioExpander = gpio_expander;
  sDac = dac;
  sDatabase = database;
}

namespace states {

void Uninitialised::react(const system_fsm::BootComplete&) {
  transit<Standby>([&]() {
    sFileSource.reset(new FatfsAudioInput());
    sI2SOutput.reset(new I2SAudioOutput(sGpioExpander, sDac));

    // Perform initial pipeline configuration.
    // TODO(jacqueline): Factor this out once we have any kind of dynamic
    // reconfiguration.
    AudioDecoder* codec = new AudioDecoder();
    sPipeline.emplace_back(codec);

    Pipeline* pipeline = new Pipeline(sPipeline.front().get());
    pipeline->AddInput(sFileSource.get());

    task::StartPipeline(pipeline, sI2SOutput.get());
  });
}

}  // namespace states

}  // namespace audio

FSM_INITIAL_STATE(audio::AudioState, audio::states::Uninitialised)
