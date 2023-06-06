/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "pipeline.hpp"
#include <algorithm>
#include <memory>
#include "stream_info.hpp"

namespace audio {

Pipeline::Pipeline(IAudioElement* output) : root_(output), subtrees_() {
  assert(output != nullptr);
}

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

auto Pipeline::InStreams(std::vector<RawStream>* out) -> void {
  for (int i = 0; i < subtrees_.size(); i++) {
    out->push_back(subtrees_[i]->OutStream());
  }
}

auto Pipeline::OutStream() -> RawStream {
  return {&output_info_, output_buffer_};
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

  std::reverse(found.begin(), found.end());
  return found;
}

}  // namespace audio
