//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <utility>

namespace nova {

//! FinalAction ensures something gets run at the end of a scope.
template <class F> class FinalAction {
public:
  explicit FinalAction(const F& ff) noexcept
    : f_ { ff }
  {
  }
  explicit FinalAction(F&& ff) noexcept
    : f_ { std::move(ff) }
  {
  }

  ~FinalAction() noexcept
  {
    // Invoke only if fully constructed or not moved from
    if (invoke_) {
      f_();
    }
  }

  FinalAction(FinalAction&& other) noexcept
    : f_(std::move(other.f_))
    , invoke_(std::exchange(other.invoke_, false))
  {
  }

  FinalAction(const FinalAction&) = delete;
  void operator=(const FinalAction&) = delete;
  void operator=(FinalAction&&) = delete;

private:
  F f_;
  bool invoke_ = true;
};

//! Convenience function to generate a `FinalAction`, which gets executed at the
//! end of a scope.
/*!
 @param f The callable to execute at the end of the scope. Can be a lambda, a
 function, a function pointer, or similar.

 `Finally` is less verbose and harder to get wrong than `try/catch`. It is a
 systematic and reasonably clean alternative to the old `goto exit;` technique
 for dealing with cleanup where resource management is not systematic.

 __Example__
 ```cpp
    void F(int n)
    {
        void* p = malloc(n);
        auto _ = Finally([p] { free(p); });
        // ...
    }
 ```
*/
template <class F> [[nodiscard]] auto Finally(F&& f) noexcept
{
  return FinalAction<std::decay_t<F>> { std::forward<F>(f) };
}

} // namespace nova
