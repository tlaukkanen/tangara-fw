#include "pipeline.hpp"
#include <memory>
#include "stream_info.hpp"

namespace audio {

Pipeline::Pipeline(IAudioElement* output) : root_(output), subtrees_() {}
Pipeline::~Pipeline() {}

auto Pipeline::AddInput(IAudioElement* input) -> Pipeline* {
  subtrees_.push_back(std::make_unique<Pipeline>(input));
  return subtrees_.back().get();
}

auto Pipeline::OutputElement() const -> IAudioElement* {
  return root_;
}

auto Pipeline::NumInputs() const -> std::size_t {
  return subtrees_.size();
}

auto Pipeline::InStreams(
    std::vector<MappableRegion<kPipelineBufferSize>>* regions,
    std::vector<RawStream>* out) -> void {
  for (int i = 0; i < subtrees_.size(); i++) {
    RawStream s = subtrees_[i]->OutStream(&regions->at(i));
    out->push_back(s);
  }
}

auto Pipeline::OutStream(MappableRegion<kPipelineBufferSize>* region)
    -> RawStream {
  return {&output_info_, region->Map(output_buffer_)};
}

auto Pipeline::GetIterationOrder() -> std::vector<Pipeline*> {
  std::vector<Pipeline*> to_search{this};
  std::vector<Pipeline*> found;

  while (!to_search.empty()) {
    Pipeline* current = to_search.back();
    to_search.pop_back();
    found.push_back(current);

    for (const auto& i : current->subtrees_) {
      to_search.push_back(i.get());
    }
  }

  return found;
}

}  // namespace audio
