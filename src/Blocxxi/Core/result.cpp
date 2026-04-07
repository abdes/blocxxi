//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/Core/result.h>

namespace blocxxi::core {

auto ToString(StatusCode code) -> std::string
{
  switch (code) {
  case StatusCode::Ok:
    return "ok";
  case StatusCode::InvalidArgument:
    return "invalid_argument";
  case StatusCode::Duplicate:
    return "duplicate";
  case StatusCode::NotFound:
    return "not_found";
  case StatusCode::StorageError:
    return "storage_error";
  case StatusCode::Rejected:
    return "rejected";
  case StatusCode::Unsupported:
    return "unsupported";
  case StatusCode::IOError:
    return "io_error";
  }

  return "unknown";
}

} // namespace blocxxi::core
