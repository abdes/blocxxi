//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <exception>

#include <Nova/Testing/GTest.h>

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

//! [Full State Machine Example]
/*
 * This example simulates a door with an electronic lock. When the door
 * is locked, the user chooses a lock code that needs to be re-entered
 * to unlock it.
 */

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
class LockedState;

struct ClosedState
  : Will<ByDefault<DoNothing>, On<LockEvent, TransitionTo<LockedState>>,
      On<OpenEvent, TransitionTo<OpenState>>> { };

struct OpenState
  : Will<ByDefault<DoNothing>, On<CloseEvent, TransitionTo<ClosedState>>> { };

class LockedState : ByDefault<DoNothing> {
public:
  using ByDefault::Handle;

  explicit LockedState(const uint32_t key)
    : key_(key)
  {
  }

  [[maybe_unused]] auto OnEnter(const LockEvent& event) -> Status
  {
    key_ = event.new_key;
    return Continue {};
  }

  //! [State Handle method]
  [[nodiscard]] [[maybe_unused]] auto Handle(const UnlockEvent& event) const
    -> Maybe<TransitionTo<ClosedState>>
  {
    if (event.key == key_) {
      return TransitionTo<ClosedState> {};
    }
    return DoNothing {};
  }
  //! [State Handle method]

private:
  uint32_t key_;
};

using Door = StateMachine<ClosedState, OpenState, LockedState>;

// NOLINTNEXTLINE
NOLINT_TEST(StateMachine, ExampleTest)
{
  Door door { ClosedState {}, OpenState {}, LockedState { 0 } };

  constexpr int lock_code = 1234;
  constexpr int bad_code = 2;

  door.Handle(LockEvent { lock_code });
  door.Handle(UnlockEvent { bad_code });
  door.Handle(UnlockEvent { lock_code });
}
//! [Full State Machine Example]

} // namespace
