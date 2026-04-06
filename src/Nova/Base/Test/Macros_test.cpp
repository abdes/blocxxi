//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Nova/Base/Macros.h>

#include <string>
#include <type_traits>

#include <Nova/Testing/GTest.h>

NOLINT_TEST(CommonMacros, NonCopyable)
{
  // NOLINTNEXTLINE
  class NonCopyable {
  public:
    NOVA_MAKE_NON_COPYABLE(NonCopyable)
  };

  static_assert(!std::is_copy_constructible_v<NonCopyable>);
  static_assert(!std::is_assignable_v<NonCopyable, NonCopyable>);
}

NOLINT_TEST(CommonMacros, NonMoveable)
{
  // NOLINTNEXTLINE
  class NonMoveable {
  public:
    NOVA_MAKE_NON_MOVABLE(NonMoveable)
  };

  static_assert(!std::is_move_constructible_v<NonMoveable>);
  static_assert(!std::is_move_assignable_v<NonMoveable>);
}

NOLINT_TEST(CommonMacros, DefaultCopyable)
{
  // NOLINTNEXTLINE
  class DefaultCopyable {
  public:
    NOVA_DEFAULT_COPYABLE(DefaultCopyable)
  };

  static_assert(std::is_copy_constructible_v<DefaultCopyable>);
  static_assert(std::is_assignable_v<DefaultCopyable, DefaultCopyable>);
}

NOLINT_TEST(CommonMacros, DefaultMoveable)
{
  // NOLINTNEXTLINE
  class DefaultMoveable {
  public:
    DefaultMoveable() = default;
    NOVA_DEFAULT_MOVABLE(DefaultMoveable)

  private:
    std::string member_ { "Hello World!" };
  };

  const DefaultMoveable movable;
  (void)movable;

  static_assert(std::is_move_constructible_v<DefaultMoveable>);
  static_assert(std::is_move_assignable_v<DefaultMoveable>);
}
