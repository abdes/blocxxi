//    Copyright The asap Project Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <utility>     // for std::move
#include <functional>  // for std::function

#include <common/logging.h>

namespace asap {

class AbstractApplication;


class RunnerBase : public asap::logging::Loggable<asap::logging::Id::NDAGENT> {
 public:
  using shutdown_function_type = std::function<void()>;

  explicit RunnerBase(shutdown_function_type func)
      : shutdown_function_(std::move(func)) {}

  virtual ~RunnerBase() = default;

  virtual void Run(AbstractApplication &app) = 0;

 protected:
  shutdown_function_type shutdown_function_;
};

}  // namespace asap
