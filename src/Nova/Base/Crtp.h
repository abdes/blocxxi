//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

namespace nova {

template <typename T, template <typename> class crtp_type> struct Crtp {
  constexpr auto underlying() noexcept -> T& { return static_cast<T&>(*this); }
  constexpr auto underlying() const noexcept -> T const&
  {
    return static_cast<T const&>(*this);
  }
};

} // namespace nova
