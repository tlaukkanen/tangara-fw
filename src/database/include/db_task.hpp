/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <functional>
#include <future>
#include <memory>

namespace database {

auto StartDbTask() -> bool;
auto QuitDbTask() -> void;

auto SendToDbTask(std::function<void(void)> fn) -> void;

template <typename T>
auto RunOnDbTask(std::function<T(void)> fn) -> std::future<T> {
  std::shared_ptr<std::promise<T>> promise =
      std::make_shared<std::promise<T>>();
  SendToDbTask([=]() { promise->set_value(std::invoke(fn)); });
  return promise->get_future();
}

template <>
auto RunOnDbTask(std::function<void(void)> fn) -> std::future<void>;

}  // namespace database
