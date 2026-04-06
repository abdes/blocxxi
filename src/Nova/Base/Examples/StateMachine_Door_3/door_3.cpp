//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <cstdio>
#include <iostream>

#include <Nova/Base/StateMachine.h>

using nova::fsm::ByDefault;
using nova::fsm::Continue;
using nova::fsm::DoNothing;
using nova::fsm::Maybe;
using nova::fsm::On;
using nova::fsm::StateMachine;
using nova::fsm::Status;
using nova::fsm::TransitionTo;
using nova::fsm::Will;

namespace {
struct OpenEvent { };
struct CloseEvent { };

struct LockEvent {
  uint32_t new_key; // the lock code chosen by the user
};

struct UnlockEvent {
  uint32_t key; // the lock key entered when unlocking
};

struct ClosedState;
struct OpenState;
struct LockedState;

struct ClosedState
  : Will<ByDefault<DoNothing>, On<LockEvent, TransitionTo<LockedState>>,
      On<OpenEvent, TransitionTo<OpenState>>> {
  using Will::Handle;

  [[maybe_unused]] static auto OnEnter(const UnlockEvent& /*event*/) -> Status
  {
    std::cout << "   > door is closed - unlocked\n";
    return Continue {};
  }

  template <typename Event>
  static auto OnEnter(const Event& /*event*/) -> Status
  {
    std::cout << "   > door is closed\n";
    return Continue {};
  }

  [[nodiscard]] [[maybe_unused]] static auto Handle(const CloseEvent& /*event*/)
    -> DoNothing
  {
    std::cerr << "Error: the door is already closed!\n";
    return DoNothing {};
  }
};

struct OpenState
  : Will<ByDefault<DoNothing>, On<CloseEvent, TransitionTo<ClosedState>>> {
  using Will::Handle;

  template <typename Event>
  static auto OnEnter(const Event& /*event*/) -> Status
  {
    std::cout << "   > door is open\n";
    return Continue {};
  }

  [[nodiscard]] [[maybe_unused]] static auto Handle(const OpenEvent& /*event*/)
    -> DoNothing
  {
    std::cerr << "Error: the door is already open!\n";
    return DoNothing {};
  }
};

struct LockedState : ByDefault<DoNothing> {
  using ByDefault::Handle;

  explicit LockedState(const uint32_t _key)
    : key(_key)
  {
  }

  [[maybe_unused]] auto OnEnter(const LockEvent& event) -> Status
  {
    std::cout << "   > door is locked with new code(" << event.new_key << ")\n";
    key = event.new_key;
    return Continue {};
  }

  [[nodiscard]] [[maybe_unused]] auto Handle(const UnlockEvent& event) const
    -> Maybe<TransitionTo<ClosedState>>
  {
    if (event.key == key) {
      return TransitionTo<ClosedState> {};
    }
    std::cerr << "Error: wrong key (" << event.key
              << ") used to unlock door!\n";
    return DoNothing {};
  }

private:
  uint32_t key;
};
} // namespace

using Door = StateMachine<ClosedState, LockedState, OpenState>;

auto main() -> int
{
  try {
    Door door { ClosedState {}, LockedState(0), OpenState {} };

    constexpr int lock_code = 1234;
    constexpr int bad_code = 2;

    std::cout << "-- Starting\n";

    std::cout << "-- sending open event\n";
    door.Handle(OpenEvent {});

    std::cout << "-- sending close event\n";
    door.Handle(CloseEvent {});

    std::cout << "-- sending lock event (" << lock_code << ")\n";
    door.Handle(LockEvent { lock_code });

    std::cout << "-- sending unlock event (" << bad_code << ")\n";
    door.Handle(UnlockEvent { bad_code });

    std::cout << "-- sending unlock event (" << lock_code << ")\n";
    door.Handle(UnlockEvent { lock_code });
  } catch (const std::exception&) {
    puts("An exception was thrown.");
  } catch (...) {
    puts("An unknown exception was thrown.");
  }
}
