//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Blocxxi/Node/api_export.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include <Blocxxi/Core/result.h>

namespace blocxxi::node {

class Node;

struct ServicePolicy {
  std::chrono::milliseconds interval { 1000 };
  std::chrono::milliseconds retry_backoff { 250 };
  std::size_t max_retries { 3 };
};

struct ServiceState {
  std::string name {};
  bool started { false };
  std::size_t run_count { 0 };
  std::size_t failure_count { 0 };
  std::int64_t last_success_utc { 0 };
  std::string last_error {};
  std::string checkpoint {};
  std::int64_t next_run_utc { 0 };
};

class Service {
public:
  virtual ~Service() = default;

  [[nodiscard]] virtual auto Name() const -> std::string = 0;
  [[nodiscard]] virtual auto Policy() const -> ServicePolicy = 0;

  virtual auto Start(Node& node) -> core::Status
  {
    (void)node;
    return core::Status::Success();
  }

  virtual auto Poll(Node& node) -> core::Status = 0;

  virtual auto Stop(Node& node) -> core::Status
  {
    (void)node;
    return core::Status::Success();
  }

  [[nodiscard]] virtual auto Checkpoint() const -> std::string { return {}; }

  virtual auto RestoreCheckpoint(std::string_view checkpoint) -> core::Status
  {
    (void)checkpoint;
    return core::Status::Success();
  }
};

using ServicePointer = std::shared_ptr<Service>;

} // namespace blocxxi::node
