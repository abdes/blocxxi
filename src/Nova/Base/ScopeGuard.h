//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <utility>

#include <Nova/Base/Compilers.h>
#include <Nova/Base/Macros.h>

namespace nova {

template <typename Fn>
concept NoExceptCallable = requires(Fn fn) {
  { fn() } noexcept;
};

//! A scope guard that runs a function when it goes out of scope.
template <NoExceptCallable Fn> class ScopeGuard {
public:
  explicit ScopeGuard(Fn fn)
    : fn_(std::move(fn))
  {
  }

  ~ScopeGuard() noexcept { fn_(); }

  NOVA_MAKE_NON_COPYABLE(ScopeGuard)
  NOVA_MAKE_NON_MOVABLE(ScopeGuard)

private:
  NOVA_NO_UNIQUE_ADDRESS Fn fn_;
};

} // namespace nova
