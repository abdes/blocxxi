//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <any>
#include <exception>
#include <stdexcept>

#include <Nova/Testing/GTest.h>

#include <Nova/Base/StateMachine.h>

using testing::ByRef;
using testing::Eq;
using testing::IsFalse;
using testing::IsTrue;
using testing::Ref;
using testing::Return;
using testing::Throw;

using nova::fsm::ByDefault;
using nova::fsm::Continue;
using nova::fsm::DoNothing;
using nova::fsm::is_one_of_v;
using nova::fsm::Maybe;
using nova::fsm::On;
using nova::fsm::OneOf;
using nova::fsm::ReissueEvent;
using nova::fsm::ReportError;
using nova::fsm::StateMachine;
using nova::fsm::Status;
using nova::fsm::Terminate;
using nova::fsm::TerminateWithError;
using nova::fsm::TransitionTo;
using nova::fsm::Will;

namespace {

//! Tests that Machine::Handle relays the event to the state and executes the
//! returned action.
NOLINT_TEST(
  StateMachine, MachineHandleEventRelaysToStateAndExecutesReturnedAction)
{
  struct TestEvent { };
  struct DefaultedEvent { };
  struct FirstState;
  using Machine = StateMachine<FirstState>;

  // This abstraction is only here to manage the circular dependencies created
  // by mocking the TestAction class.
  struct Action { // NOLINT(cppcoreguidelines-special-member-functions)
    virtual ~Action() = default;
    // NOLINT
    virtual auto Execute(Machine& machine, FirstState& state,
      const TestEvent& event) -> Status = 0;
  };

  struct TestAction {
    auto Execute(Machine& machine, FirstState& state,
      const TestEvent& event) const -> Status
    {
      return mock->Execute(machine, state, event);
    }
    std::shared_ptr<Action> mock;
  };

  // ReSharper disable CppMemberFunctionMayBeConst
  // NOLINTBEGIN(*)
  struct MockState {
    MOCK_METHOD(TestAction, Handle, (const TestEvent&), ());
  };
  // NOLINTEND(*)
  // ReSharper enable CppMemberFunctionMayBeConst

  struct FirstState : ByDefault<DoNothing> {
    using ByDefault::Handle;
    explicit FirstState(std::shared_ptr<MockState> _mock)
      : mock { std::move(_mock) }
    {
    }

    [[nodiscard]] auto Handle(const TestEvent& event) const -> TestAction
    {
      return mock->Handle(event);
    }
    std::shared_ptr<MockState> mock;
  };

  // ReSharper disable CppMemberFunctionMayBeConst
  // NOLINTBEGIN(*)
  struct MockAction final : Action {
    MOCK_METHOD(
      Status, Execute, (Machine&, FirstState&, const TestEvent&), (override));
  };
  // NOLINTEND(*)
  // ReSharper enable CppMemberFunctionMayBeConst

  const auto mock_state = std::make_shared<MockState>();
  const auto mock_action = std::make_shared<MockAction>();

  Machine machine { FirstState { mock_state } };

  TestEvent test_event {};
  TestAction test_action { mock_action };
  auto& state = machine.TransitionTo<FirstState>();
  EXPECT_CALL(*mock_state, Handle)
    .Times(1)
    .WillOnce(Return(ByRef(test_action)));
  EXPECT_CALL(*mock_action, Execute(Ref(machine), Ref(state), Ref(test_event)))
    .Times(1);
  machine.Handle(test_event);

  // An event falling under a ByDefault rule will not trigger a call to the
  // state Handle method
  constexpr DefaultedEvent event {};
  EXPECT_CALL(*mock_state, Handle).Times(0);
  machine.Handle(event);
}

NOLINT_TEST(StateMachine, MachineHandleEventCatchesUnhandledExceptions)
//! Tests that Machine::Handle catches unhandled exceptions from state or
//! action.
{
  struct TestEvent { };
  struct FirstState;
  using Machine = StateMachine<FirstState>;

  // This abstraction is only here to manage the circular dependencies created
  // by mocking the TestAction class.
  struct Action { // NOLINT(cppcoreguidelines-special-member-functions)
    virtual ~Action() = default;
    virtual auto Execute(Machine& machine, FirstState& state,
      const TestEvent& event) -> Status = 0;
  };

  struct TestAction {
    auto Execute(Machine& machine, FirstState& state,
      const TestEvent& event) const -> Status
    {
      return mock->Execute(machine, state, event);
    }
    std::shared_ptr<Action> mock;
  };

  // ReSharper disable CppMemberFunctionMayBeConst
  // NOLINTBEGIN(*)
  struct MockState {
    MOCK_METHOD(TestAction, Handle, (const TestEvent&), ());
  };
  // NOLINTEND(*)
  // ReSharper enable CppMemberFunctionMayBeConst

  struct FirstState : ByDefault<DoNothing> {
    using ByDefault::Handle;
    explicit FirstState(std::shared_ptr<MockState> _mock)
      : mock { std::move(_mock) }
    {
    }

    [[nodiscard]] auto Handle(const TestEvent& event) const -> TestAction
    {
      return mock->Handle(event);
    }
    std::shared_ptr<MockState> mock;
  };

  // ReSharper disable CppMemberFunctionMayBeConst
  // NOLINTBEGIN(*)
  struct MockAction final : Action {
    MOCK_METHOD(
      Status, Execute, (Machine&, FirstState&, const TestEvent&), (override));
  };
  // NOLINTEND(*)
  // ReSharper enable CppMemberFunctionMayBeConst

  auto mock_state = std::make_shared<MockState>();
  auto mock_action = std::make_shared<MockAction>();

  Machine machine { FirstState { mock_state } };

  TestEvent test_event {};
  std::logic_error error("unhandled exception");
  EXPECT_CALL(*mock_state, Handle).Times(1).WillOnce(Throw(ByRef(error)));
  auto status = machine.Handle(test_event);
  ASSERT_NO_THROW([[maybe_unused]] auto val = std::get<2>(status));

  TestAction test_action { mock_action };
  EXPECT_CALL(*mock_state, Handle)
    .Times(1)
    .WillOnce(Return(ByRef(test_action)));
  auto& state = machine.TransitionTo<FirstState>();
  EXPECT_CALL(*mock_action, Execute(Ref(machine), Ref(state), Ref(test_event)))
    .Times(1)
    .WillOnce(Throw(ByRef(error)));
  status = machine.Handle(test_event);
  ASSERT_NO_THROW([[maybe_unused]] auto val = std::get<2>(status));
}

NOLINT_TEST(StateMachine, DoNothingExample)
//! Tests that DoNothing action results in no side effects.
{
  //! [DoNothing action example]
  struct DoNothingEvent { };
  struct TestState {
    static auto Handle(const DoNothingEvent& /*event*/) -> DoNothing
    {
      // Returning the `DoNothing` action will result in no side effects
      // from the state machine calling the `Execute` method of the action.
      return DoNothing {};
    }
  };

  StateMachine machine { TestState {} };
  machine.Handle(DoNothingEvent {});
  //! [DoNothing action example]

  ASSERT_THAT(machine.IsIn<TestState>(), IsTrue());
}

NOLINT_TEST(StateMachine, ByDefaultExample)
//! Tests ByDefault fallback for events without dedicated Handle overloads.
{
  //! [ByDefault example]
  struct NotForMeEvent { };
  struct SpecialEvent { };
  struct SecondState : Will<ByDefault<DoNothing>> { };

  // If an event does not have a dedicated `Handle` overload, use the
  // `ByDefault` action (i.e. `DoNothing` in this case).
  struct FirstState : ByDefault<DoNothing> {
    // Bring in the `Handle` overloads from the `ByDefault` base type.
    using ByDefault::Handle;

    // A dedicated overload of `Handle` for the special event ensures
    // that it is not using the `ByDefault` action.
    static auto Handle(const SpecialEvent& /*event*/)
      -> TransitionTo<SecondState>
    {
      return TransitionTo<SecondState> {};
    }
  };

  StateMachine machine { FirstState {}, SecondState {} };

  machine.Handle(NotForMeEvent {}); // DoNothing
  ASSERT_THAT(machine.IsIn<FirstState>(), IsTrue());

  machine.Handle(SpecialEvent {}); // TransitionTo AnotherState
  ASSERT_THAT(machine.IsIn<SecondState>(), IsTrue());
  //! [ByDefault example]
}

NOLINT_TEST(StateMachine, OnExample)
//! Tests On<> event handler for state transitions.
{
  //! [On example]
  struct SpecialEvent { };
  struct SecondState : Will<ByDefault<DoNothing>> { };

  struct FirstState : Will<On<SpecialEvent, TransitionTo<SecondState>>> { };

  StateMachine machine { FirstState {}, SecondState {} };

  machine.Handle(SpecialEvent {}); // TransitionTo AnotherState
  ASSERT_THAT(machine.IsIn<SecondState>(), IsTrue());
  //! [On example]
}

NOLINT_TEST(StateMachine, WillExample)
//! Tests Will<> composition for multiple event handlers and actions.
{
  //! [Will example]
  struct EventOne { };
  struct EventTwo { };
  struct EventThree { };

  struct FirstState;
  struct SecondState;
  using Machine = StateMachine<FirstState, SecondState>;

  struct FirstState
    : Will<ByDefault<DoNothing>, On<EventOne, TransitionTo<SecondState>>> { };

  struct SpecialAction {
    static auto Execute(Machine& /*machine*/, SecondState& /*state*/,
      const EventTwo& /*event*/) -> Status
    {
      return Continue {};
    }
  };

  struct SecondState
    : Will<ByDefault<DoNothing>, On<EventThree, TransitionTo<FirstState>>> {
    using Will::Handle;

    // A dedicated overload of `Handle` for the special event ensures
    // that it is not using the `ByDefault` action.
    static auto Handle(const EventTwo& /*event*/) -> SpecialAction
    {
      return SpecialAction {};
    }
  };

  Machine machine { FirstState {}, SecondState {} };

  EXPECT_THAT(machine.IsIn<FirstState>(), IsTrue());
  machine.Handle(EventOne {});
  EXPECT_THAT(machine.IsIn<SecondState>(), IsTrue());
  machine.Handle(EventTwo {}); // SpecialAction `Execute` called
  EXPECT_THAT(machine.IsIn<SecondState>(), IsTrue());
  machine.Handle(EventThree {});
  EXPECT_THAT(machine.IsIn<FirstState>(), IsTrue());
  //! [Will example]
}

NOLINT_TEST(StateMachine, OneOfExample)
//! Tests OneOf<> action for alternate handler paths.
{
  //! [OneOf example]
  struct SpecialEvent { };

  struct FirstState;
  struct SecondState;
  using Machine = StateMachine<FirstState, SecondState>;

  struct SecondState : Will<ByDefault<DoNothing>> { };

  struct SpecialAction {
    static auto Execute(Machine& /*machine*/, FirstState& /*state*/,
      const SpecialEvent& /*event*/) -> Status
    {
      return Continue {};
    }
  };

  struct FirstState {
    explicit FirstState(const bool _transition)
      : transition { _transition }
    {
    }

    // This handler has two alternate paths. We use the `OneOf`
    // helper to still be able to return a single action type
    [[nodiscard]] auto Handle(const SpecialEvent& /*event*/) const
      -> OneOf<TransitionTo<SecondState>, SpecialAction>
    {
      if (transition) {
        return TransitionTo<SecondState> {};
      }
      return SpecialAction {};
    }
    bool transition;
  };

  Machine machine1 { FirstState { true }, SecondState {} };
  try {
    machine1.Handle(SpecialEvent {});
  } catch (...) {
    (void)0;
  }
  ASSERT_THAT(machine1.IsIn<SecondState>(), IsTrue());

  Machine machine2 { FirstState { false }, SecondState {} };
  machine2.Handle(SpecialEvent {});
  ASSERT_THAT(machine2.IsIn<FirstState>(), IsTrue());
  //! [OneOf example]
}

NOLINT_TEST(StateMachine, MaybeExample)
//! Tests Maybe<> action for optional transitions.
{
  //! [Maybe example]
  struct SpecialEvent {
    bool transition { false };
  };

  struct FirstState;
  struct SecondState;
  using Machine = StateMachine<FirstState, SecondState>;

  struct SecondState : Will<ByDefault<DoNothing>> { };

  struct FirstState {
    // This handler either transitions to `AnotherState` or does
    // nothing at all
    [[nodiscard]] static auto Handle(const SpecialEvent& event)
      -> Maybe<TransitionTo<SecondState>>
    {
      if (event.transition) {
        return TransitionTo<SecondState> {};
      }
      return DoNothing {};
    }
  };

  Machine machine1 { FirstState {}, SecondState {} };
  machine1.Handle(SpecialEvent { true });
  ASSERT_THAT(machine1.IsIn<SecondState>(), IsTrue());

  Machine machine2 { FirstState {}, SecondState {} };
  machine2.Handle(SpecialEvent { false });
  ASSERT_THAT(machine2.IsIn<FirstState>(), IsTrue());
  //! [Maybe example]
}

NOLINT_TEST(StateMachine, TransitionToExample)
//! Tests TransitionTo<> action and OnEnter/OnLeave hooks.
{
  //! [TransitionTo example]
  struct TransitionEvent { };

  // ReSharper disable CppMemberFunctionMayBeConst
  // NOLINTBEGIN(*)
  struct ExampleMockState {
    MOCK_METHOD(Status, OnEnter, (const TransitionEvent&), ());
    MOCK_METHOD(Status, OnLeave, (const TransitionEvent&), ());
  };
  // NOLINTEND(*)
  // ReSharper enable CppMemberFunctionMayBeConst

  struct FirstState;
  struct SecondState;
  using Machine = StateMachine<FirstState, SecondState>;

  struct FirstState : Will<On<TransitionEvent, TransitionTo<SecondState>>> {
    explicit FirstState(std::shared_ptr<ExampleMockState> _mock)
      : mock { std::move(_mock) }
    {
    }
    // We only implement the OnLeave method, but that's ok.
    // The action will only call whatever is implemented.
    [[nodiscard]] auto OnLeave(const TransitionEvent& event) const -> Status
    {
      return mock->OnLeave(event);
    }

  private:
    std::shared_ptr<ExampleMockState> mock;
  };

  struct SecondState : Will<ByDefault<DoNothing>> {
    explicit SecondState(std::shared_ptr<ExampleMockState> _mock)
      : mock { std::move(_mock) }
    {
    }
    // We only implement the OnEnter method, but that's ok.
    // The action will only call whatever is implemented.
    [[nodiscard]] auto OnEnter(const TransitionEvent& event) const -> Status
    {
      return mock->OnEnter(event);
    }

  private:
    std::shared_ptr<ExampleMockState> mock;
  };

  const auto mock_initial_state = std::make_shared<ExampleMockState>();
  const auto mock_another_state = std::make_shared<ExampleMockState>();
  Machine machine { FirstState { mock_initial_state },
    SecondState { mock_another_state } };

  TransitionEvent event {};

  EXPECT_CALL(*mock_initial_state, OnLeave(Ref(event)))
    .Times(1)
    .WillOnce(Return(Continue {}));
  EXPECT_CALL(*mock_another_state, OnEnter(Ref(event)))
    .Times(1)
    .WillOnce(Return(Status { Continue {} }));
  machine.Handle(event);
  ASSERT_THAT(machine.IsIn<SecondState>(), IsTrue());
  //! [TransitionTo example]
}

//! Tests TransitionTo<> action with data passed to OnEnter.
NOLINT_TEST(StateMachine, TransitionToWithDataExample)
{
  //! [TransitionTo with data example]
  struct TransitionEvent { };

  // ReSharper disable CppMemberFunctionMayBeConst
  // NOLINTBEGIN(*)
  struct ExampleMockState {
    MOCK_METHOD(Status, OnEnter, (const TransitionEvent&, int), ());
    MOCK_METHOD(Status, OnLeave, (const TransitionEvent&), ());
  };
  // NOLINTEND(*)
  // ReSharper enable CppMemberFunctionMayBeConst

  struct FirstState;
  struct SecondState;
  using Machine = StateMachine<FirstState, SecondState>;

  struct FirstState {
    explicit FirstState(std::shared_ptr<ExampleMockState> _mock)
      : mock { std::move(_mock) }
    {
    }
    // We only implement the OnLeave method, but that's ok.
    // The action will only call whatever is implemented.
    [[nodiscard]] auto OnLeave(const TransitionEvent& event) const -> Status
    {
      return mock->OnLeave(event);
    }

    // This special handler allows for passing data via the
    // transition object to the next state which can be consumed
    // if that state implements `onEnter(event, data)`
    [[nodiscard]] static auto Handle(const TransitionEvent& /*event*/)
      -> TransitionTo<SecondState>
    {
      return TransitionTo<SecondState> { 1 };
    }

  private:
    std::shared_ptr<ExampleMockState> mock;
  };

  struct SecondState : Will<ByDefault<DoNothing>> {
    explicit SecondState(std::shared_ptr<ExampleMockState> _mock)
      : mock { std::move(_mock) }
    {
    }
    // This implementation expects data to be passed from the
    // previous state.
    [[nodiscard]] auto OnEnter(
      const TransitionEvent& event, const std::any& data) const -> Status
    {
      EXPECT_THAT(std::any_cast<int>(data), Eq(1));
      // Mocking methods with std::any arguments will fail to compile with
      // clang and there is no fix for it from gmock. Just specifically cast
      // the std::any before forwarding to the mock.
      return mock->OnEnter(event, std::any_cast<int>(data));
    }

  private:
    std::shared_ptr<ExampleMockState> mock;
  };

  const auto mock_initial_state = std::make_shared<ExampleMockState>();
  const auto mock_another_state = std::make_shared<ExampleMockState>();
  Machine machine { FirstState { mock_initial_state },
    SecondState { mock_another_state } };

  TransitionEvent event {};

  EXPECT_CALL(*mock_initial_state, OnLeave(Ref(event)))
    .Times(1)
    .WillOnce(Return(Continue {}));
  EXPECT_CALL(*mock_another_state, OnEnter(Ref(event), ::testing::_))
    .Times(1)
    .WillOnce(Return(Status { Continue {} }));
  machine.Handle(event);
  ASSERT_THAT(machine.IsIn<SecondState>(), IsTrue());
  //! [TransitionTo with data example]
}

struct StateMachineTransitionToErrors : testing::Test {
  struct TransitionEvent { };

  // ReSharper disable CppMemberFunctionMayBeConst
  // NOLINTBEGIN(*)
  struct ExampleMockState {
    MOCK_METHOD(Status, OnEnter, (const TransitionEvent&), ());
    MOCK_METHOD(Status, OnLeave, (const TransitionEvent&), ());
  };
  // NOLINTEND(*)
  // ReSharper enable CppMemberFunctionMayBeConst

  struct FirstState;
  struct SecondState;
  using Machine = StateMachine<FirstState, SecondState>;

  struct FirstState : Will<On<TransitionEvent, TransitionTo<SecondState>>> {
    explicit FirstState(std::shared_ptr<ExampleMockState> _mock)
      : mock { std::move(_mock) }
    {
    }
    // We only implement the OnLeave method, but that's ok.
    // The action will only call whatever is implemented.
    [[maybe_unused]] [[nodiscard]] auto OnLeave(
      const TransitionEvent& event) const -> Status
    {
      return mock->OnLeave(event);
    }

  private:
    std::shared_ptr<ExampleMockState> mock;
  };

  struct SecondState : Will<ByDefault<DoNothing>> {
    explicit SecondState(std::shared_ptr<ExampleMockState> _mock)
      : mock { std::move(_mock) }
    {
    }
    // We only implement the OnEnter method, but that's ok.
    // The action will only call whatever is implemented.
    [[maybe_unused]] [[nodiscard]] auto OnEnter(
      const TransitionEvent& event) const -> Status
    {
      return mock->OnEnter(event);
    }

  private:
    std::shared_ptr<ExampleMockState> mock;
  };

  std::shared_ptr<ExampleMockState> mock_initial_state
    = std::make_shared<ExampleMockState>();
  std::shared_ptr<ExampleMockState> mock_another_state
    = std::make_shared<ExampleMockState>();
  Machine machine { FirstState { mock_initial_state },
    SecondState { mock_another_state } };
};

TEST_F(StateMachineTransitionToErrors, OnLeaveReturnsTerminate)
//! Tests that OnLeave returning Terminate prevents state transition.
{
  TransitionEvent event {};

  EXPECT_CALL(*mock_initial_state, OnLeave(Ref(event)))
    .Times(1)
    .WillOnce(Return(Terminate {}));
  EXPECT_CALL(*mock_another_state, OnEnter(Ref(event))).Times(0);
  const auto status = machine.Handle(event);
  ASSERT_THAT(machine.IsIn<SecondState>(), IsFalse());
  EXPECT_THAT(std::holds_alternative<Terminate>(status), IsTrue());
}

TEST_F(StateMachineTransitionToErrors, OnLeaveReturnsTerminateWithError)
//! Tests that OnLeave returning TerminateWithError prevents state transition
//! and stores error.
{
  TransitionEvent event {};

  EXPECT_CALL(*mock_initial_state, OnLeave(Ref(event)))
    .Times(1)
    .WillOnce(Return(TerminateWithError { "error" }));
  EXPECT_CALL(*mock_another_state, OnEnter(Ref(event))).Times(0);
  const auto status = machine.Handle(event);
  ASSERT_THAT(machine.IsIn<SecondState>(), IsFalse());
  EXPECT_THAT(std::holds_alternative<TerminateWithError>(status), IsTrue());
  EXPECT_THAT(std::get<TerminateWithError>(status).error_message, Eq("error"));
}

TEST_F(StateMachineTransitionToErrors, OnLeaveThrowsStateMachineError)
//! Tests that OnLeave throwing an error results in TerminateWithError.
{
  TransitionEvent event {};

  EXPECT_CALL(*mock_initial_state, OnLeave(Ref(event)))
    .Times(1)
    .WillOnce(Throw(std::runtime_error("error")));
  EXPECT_CALL(*mock_another_state, OnEnter(Ref(event))).Times(0);
  const auto status = machine.Handle(event);
  ASSERT_THAT(machine.IsIn<SecondState>(), IsFalse());
  EXPECT_THAT(std::holds_alternative<TerminateWithError>(status), IsTrue());
  EXPECT_THAT(std::get<TerminateWithError>(status).error_message, Eq("error"));
}

TEST_F(StateMachineTransitionToErrors, OnLeaveThrowsOtherError)
//! Tests that OnLeave throwing a non-state-machine error results in
//! TerminateWithError.
{
  TransitionEvent event {};

  EXPECT_CALL(*mock_initial_state, OnLeave(Ref(event)))
    .Times(1)
    .WillOnce(Throw(std::runtime_error("error")));
  EXPECT_CALL(*mock_another_state, OnEnter(Ref(event))).Times(0);
  const auto status = machine.Handle(event);
  ASSERT_THAT(machine.IsIn<SecondState>(), IsFalse());
  EXPECT_THAT(std::holds_alternative<TerminateWithError>(status), IsTrue());
  EXPECT_THAT(std::get<TerminateWithError>(status).error_message, Eq("error"));
}

TEST_F(StateMachineTransitionToErrors, OnEnterReturnsTerminate)
//! Tests that OnEnter returning Terminate ends the state machine in the new
//! state.
{
  TransitionEvent event {};

  EXPECT_CALL(*mock_initial_state, OnLeave(Ref(event)))
    .Times(1)
    .WillOnce(Return(Continue {}));
  EXPECT_CALL(*mock_another_state, OnEnter(Ref(event)))
    .Times(1)
    .WillOnce(Return(Terminate {}));
  const auto status = machine.Handle(event);
  ASSERT_THAT(machine.IsIn<SecondState>(), IsTrue());
  EXPECT_THAT(std::holds_alternative<Terminate>(status), IsTrue());
}

TEST_F(StateMachineTransitionToErrors, OnEnterReturnsTerminateWithError)
//! Tests that OnEnter returning TerminateWithError ends the state machine in
//! the new state and stores error.
{
  TransitionEvent event {};

  EXPECT_CALL(*mock_initial_state, OnLeave(Ref(event)))
    .Times(1)
    .WillOnce(Return(Continue {}));
  EXPECT_CALL(*mock_another_state, OnEnter(Ref(event)))
    .Times(1)
    .WillOnce(Return(TerminateWithError { "error" }));
  const auto status = machine.Handle(event);
  ASSERT_THAT(machine.IsIn<SecondState>(), IsTrue());
  EXPECT_THAT(std::holds_alternative<TerminateWithError>(status), IsTrue());
  EXPECT_THAT(std::get<TerminateWithError>(status).error_message, Eq("error"));
}

TEST_F(StateMachineTransitionToErrors, OnEnterReturnsReissueEvent)
//! Tests that OnEnter returning ReissueEvent requests event reprocessing.
{
  TransitionEvent event {};

  EXPECT_CALL(*mock_initial_state, OnLeave(Ref(event)))
    .Times(1)
    .WillOnce(Return(Continue {}));
  EXPECT_CALL(*mock_another_state, OnEnter(Ref(event)))
    .Times(1)
    .WillOnce(Return(ReissueEvent {}));
  const auto status = machine.Handle(event);
  ASSERT_THAT(machine.IsIn<SecondState>(), IsTrue());
  EXPECT_THAT(std::holds_alternative<ReissueEvent>(status), IsTrue());
}

TEST_F(StateMachineTransitionToErrors, OnEnterThrowsStateMachineError)
//! Tests that OnEnter throwing an error results in TerminateWithError in the
//! new state.
{
  TransitionEvent event {};

  EXPECT_CALL(*mock_initial_state, OnLeave(Ref(event)))
    .Times(1)
    .WillOnce(Return(Continue {}));
  EXPECT_CALL(*mock_another_state, OnEnter(Ref(event)))
    .Times(1)
    .WillOnce(Throw(std::runtime_error("error")));
  const auto status = machine.Handle(event);
  ASSERT_THAT(machine.IsIn<SecondState>(), IsTrue());
  EXPECT_THAT(std::holds_alternative<TerminateWithError>(status), IsTrue());
  EXPECT_THAT(std::get<TerminateWithError>(status).error_message, Eq("error"));
}

TEST_F(StateMachineTransitionToErrors, OnEnterThrowsOtherError)
//! Tests that OnEnter throwing a non-state-machine error results in
//! TerminateWithError in the new state.
{
  TransitionEvent event {};

  EXPECT_CALL(*mock_initial_state, OnLeave(Ref(event)))
    .Times(1)
    .WillOnce(Return(Continue {}));
  EXPECT_CALL(*mock_another_state, OnEnter(Ref(event)))
    .Times(1)
    .WillOnce(Throw(std::runtime_error("error")));
  const auto status = machine.Handle(event);
  ASSERT_THAT(machine.IsIn<SecondState>(), IsTrue());
  EXPECT_THAT(std::holds_alternative<TerminateWithError>(status), IsTrue());
  EXPECT_THAT(std::get<TerminateWithError>(status).error_message, Eq("error"));
}

NOLINT_TEST(StateMachine, TransitionToIsATest)
//! Tests IsA/GetAs for TransitionTo action type.
{
  struct State { };
  constexpr int data = 1;
  auto action = TransitionTo<State> { data };
  ASSERT_THAT(action.IsA<TransitionTo<State>>(), IsTrue());
  ASSERT_THAT(action.IsA<DoNothing>(), IsFalse());

  const auto specific_ok = action.GetAs<TransitionTo<State>>();
  EXPECT_THAT(std::any_cast<int>(specific_ok.Data()), Eq(data));
  const auto& const_specific_ok = const_cast<const TransitionTo<State>&>(action)
                                    .GetAs<TransitionTo<State>>();
  EXPECT_THAT(std::any_cast<int>(const_specific_ok.Data()), Eq(data));
}

NOLINT_TEST(StateMachine, DoNothingIsATest)
//! Tests IsA/GetAs for DoNothing action type.
{
  struct State { };
  auto action = DoNothing {};
  EXPECT_THAT(action.IsA<DoNothing>(), IsTrue());

  [[maybe_unused]] auto specific_ok = action.GetAs<DoNothing>();
  [[maybe_unused]] const auto& const_specific_ok
    = const_cast<const DoNothing&>(action).GetAs<DoNothing>();
}

NOLINT_TEST(StateMachine, ReportErrorIsATest)
//! Tests IsA/GetAs for ReportError action type and error storage.
{
  struct State { };
  constexpr auto error = "an error";
  auto action = ReportError { error };
  EXPECT_THAT(action.IsA<ReportError>(), IsTrue());

  [[maybe_unused]] auto specific_ok = action.GetAs<ReportError>();
  [[maybe_unused]] const auto& const_specific_ok
    = const_cast<const ReportError&>(action).GetAs<ReportError>();
  const auto stored_error
    = std::any_cast<std::string>(const_specific_ok.Data());
  EXPECT_THAT(stored_error, Eq(error));
}

NOLINT_TEST(StateMachine, OneOfIsATest)
//! Tests IsA/GetAs for OneOf action type and error on wrong type access.
{
  struct State { };
  auto action = OneOf<DoNothing, TransitionTo<State>> { DoNothing {} };

  // compile-time type check
  if constexpr (!is_one_of_v<OneOf<DoNothing, TransitionTo<State>>>) {
    FAIL();
  }

  EXPECT_THAT(action.IsA<DoNothing>(), IsTrue());
  EXPECT_THAT(action.IsA<TransitionTo<State>>(), IsFalse());

  [[maybe_unused]] auto specific_ok = action.GetAs<DoNothing>();
  [[maybe_unused]] const auto& const_specific_ok
    = const_cast<const OneOf<DoNothing, TransitionTo<State>>&>(action)
        .GetAs<DoNothing>();
  EXPECT_THROW(action.GetAs<TransitionTo<State>>(), std::bad_variant_access);
}

struct State { };
struct OtherState { };

template <typename Action> struct DynamicActionTestValue {
  Action action;
  bool is_transition_to_state;
};

template <typename Action> class DynamicActionTest : public testing::Test {
protected:
  static std::vector<DynamicActionTestValue<Action>> test_actions_;
};
TYPED_TEST_SUITE_P(DynamicActionTest);

TYPED_TEST_P(DynamicActionTest, CheckAction)
{
  for (const auto& value : DynamicActionTest<TypeParam>::test_actions_) {
    // Test the action
    auto check = value.action.template IsA<TransitionTo<State>>();
    EXPECT_THAT(check, Eq(value.is_transition_to_state));
  }
}

REGISTER_TYPED_TEST_SUITE_P(DynamicActionTest, CheckAction);

using OneOf_TT_State = OneOf<TransitionTo<State>, DoNothing>;
using OneOf_TT_OtherState = OneOf<TransitionTo<OtherState>, DoNothing>;
using ActionTypes = testing::Types<
  // clang-format off
    OneOf_TT_State,
    OneOf_TT_OtherState,
    TransitionTo<State>,
    DoNothing
>; // clang-format on

INSTANTIATE_TYPED_TEST_SUITE_P(My, DynamicActionTest, ActionTypes, );
// Silence the warning zero-variadic-macro-arguments by adding a comma and an
// empty argument for the macros
// https://github.com/google/googletest/issues/2271#issuecomment-665742471
TYPED_TEST_SUITE(DynamicActionTest, ActionTypes, );

template <>
std::vector<DynamicActionTestValue<OneOf_TT_State>>
  DynamicActionTest<OneOf_TT_State>::test_actions_ {
    {
      .action = TransitionTo<State> {},
      .is_transition_to_state = true,
    },
    {
      .action = DoNothing {},
      .is_transition_to_state = false,
    },
  };

template <>
std::vector<DynamicActionTestValue<OneOf_TT_OtherState>>
  DynamicActionTest<OneOf_TT_OtherState>::test_actions_ {
    {
      .action = TransitionTo<OtherState> {},
      .is_transition_to_state = false,
    },
    {
      .action = DoNothing {},
      .is_transition_to_state = false,
    },
  };

template <>
std::vector<DynamicActionTestValue<TransitionTo<State>>>
  DynamicActionTest<TransitionTo<State>>::test_actions_ {
    {
      .action = TransitionTo<State> {},
      .is_transition_to_state = true,
    },
  };

template <>
std::vector<DynamicActionTestValue<DoNothing>>
  DynamicActionTest<DoNothing>::test_actions_ {
    {
      .action = DoNothing {},
      .is_transition_to_state = false,
    },
  };

} // namespace
