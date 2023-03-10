#include "pipeline.hpp"
#include "stream_info.hpp"

namespace audio {

Pipeline::Pipeline(IAudioElement* output) : root_(output), subtrees_() {}
Pipeline::~Pipeline() {}

auto Pipeline::AddInput(IAudioElement* input) -> Pipeline* {
  subtrees_.emplace_back(input);
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
    std::vector<MutableStream>* out) -> void {
  for (int i = 0; i < subtrees_.size(); i++) {
    MutableStream s = subtrees_[i]->OutStream(&regions->at(i));
    out->push_back(s);
  }
}

auto Pipeline::OutStream(MappableRegion<kPipelineBufferSize>* region)
    -> MutableStream {
  return {&output_info_, region->Map(output_buffer_)};
}

auto Pipeline::GetIterationOrder() -> std::vector<Pipeline*> {
  std::vector<Pipeline*> to_search{this};
  std::vector<Pipeline*> found;

  while (!to_search.empty()) {
    Pipeline* current = to_search.back();
    to_search.pop_back();
    found.push_back(current);

    to_search.insert(to_search.end(), current->subtrees_.begin(),
                     current->subtrees_.end());
  }

  return found;
}

}  // namespace audio
