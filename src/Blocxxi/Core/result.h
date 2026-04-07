//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Blocxxi/Core/api_export.h>

#include <string>

namespace blocxxi::core {

enum class StatusCode {
  Ok = 0,
  InvalidArgument,
  Duplicate,
  NotFound,
  StorageError,
  Rejected,
  Unsupported,
  IOError,
};

struct Status {
  StatusCode code { StatusCode::Ok };
  std::string message {};

  [[nodiscard]] auto ok() const -> bool { return code == StatusCode::Ok; }

  static auto Success(std::string message = {}) -> Status
  {
    return Status { StatusCode::Ok, std::move(message) };
  }

  static auto Failure(StatusCode code, std::string message) -> Status
  {
    return Status { code, std::move(message) };
  }
};

BLOCXXI_CORE_NDAPI auto ToString(StatusCode code) -> std::string;

} // namespace blocxxi::core
