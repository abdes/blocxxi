//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Blocxxi/Storage/api_export.h>

#include <memory>
#include <map>
#include <optional>
#include <vector>

#include <Blocxxi/Chain/kernel.h>

namespace blocxxi::storage {

[[nodiscard]] BLOCXXI_STORAGE_API auto MakeInMemoryBlockStore()
  -> std::shared_ptr<chain::BlockStore>;
[[nodiscard]] BLOCXXI_STORAGE_API auto MakeInMemorySnapshotStore()
  -> std::shared_ptr<chain::SnapshotStore>;

} // namespace blocxxi::storage
