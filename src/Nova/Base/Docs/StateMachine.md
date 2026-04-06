# State Machine Library Documentation

## Overview

The State Machine library (`nova::fsm`) provides a modern, type-safe, and highly
flexible framework for implementing finite state machines (FSMs) in C++. It
leverages C++17/20 features such as `std::variant`, variadic templates, and
compile-time type deduction to enable declarative, robust, and testable state
machine designs.

Key features include:

- **Type-safe states and events**: States and events are represented by
  user-defined types, not enums or IDs.
- **Declarative transitions**: Use helper base classes (`Will`, `On`,
  `ByDefault`) to express transition logic clearly.
- **Action-based event handling**: State event handlers return action objects
  that describe what the machine should do next.
- **Comprehensive status reporting**: Each event handling returns a `Status`
  variant, indicating continuation, termination, or error.
- **Exception safety**: All exceptions from handlers are caught and reported as
  error statuses.
- **Testability**: The design supports easy mocking and inspection for unit
  testing.

## Core Concepts

### States

Each state is a distinct C++ type (usually a struct or class). States can have
data members and methods. The current state is tracked as a `std::variant` of
all possible state types.

### Events

Events are also C++ types (often empty structs or classes, but can carry data).
Events are passed to the state machine's `Handle()` method.

### Actions

Event handlers in states return action objects that describe what the state
machine should do next. Common actions include:

- `DoNothing` — No transition or side effect.
- `TransitionTo<State>` — Transition to another state (optionally with data).
- `ReportError` — Signal an error and terminate the machine.
- `OneOf<A1, A2, ...>` — Return one of several possible actions.
- `Maybe<A>` — Optionally return an action or do nothing.

All actions must provide an `Execute()` method with a standard signature.

### Status

After handling an event, the state machine returns a `Status` variant:

- `Continue` — Continue processing events.
- `Terminate` — Stop processing; machine is done.
- `TerminateWithError` — Stop processing due to error (with message).
- `ReissueEvent` — Reprocess the last event (after a transition).

## Declaring a State Machine

Declare a state machine by listing all possible states as template parameters:

```cpp
using MyMachine = nova::fsm::StateMachine<ClosedState, OpenState, LockedState>;
```

The first state is the initial state by default. You can explicitly transition
to another state if needed.

## Implementing States and Transitions

States implement event handlers as `Handle(const Event&)` methods, returning an
action. Use the provided base classes to compose transition logic:

- `Will<A1, A2, ...>` — Compose multiple event handlers and defaults.
- `On<Event, Action>` — Handle a specific event with a specific action.
- `ByDefault<Action>` — Fallback handler for unhandled events.

You can implement `OnEnter` and `OnLeave` methods for side effects during
transitions.

### Example: Basic Door State Machine

```cpp
struct OpenEvent { };
struct CloseEvent { };

struct ClosedState : nova::fsm::Will<
    nova::fsm::On<OpenEvent, nova::fsm::TransitionTo<OpenState>>,
    nova::fsm::ByDefault<nova::fsm::DoNothing>
> { };

struct OpenState : nova::fsm::Will<
    nova::fsm::On<CloseEvent, nova::fsm::TransitionTo<ClosedState>>,
    nova::fsm::ByDefault<nova::fsm::DoNothing>
> { };

using DoorMachine = nova::fsm::StateMachine<ClosedState, OpenState>;

DoorMachine machine{ClosedState{}};
machine.Handle(OpenEvent{});   // transitions to OpenState
machine.Handle(CloseEvent{});  // transitions to ClosedState
```

### Example: State with Data and Custom Handlers

```cpp
struct LockEvent { int code; };
struct UnlockEvent { int code; };

struct LockedState {
    int lock_code;
    auto Handle(const UnlockEvent& event) const {
        if (event.code == lock_code)
            return nova::fsm::TransitionTo<ClosedState>{};
        return nova::fsm::ReportError{"wrong key"};
    }
};
```

### Example: OnEnter/OnLeave Hooks

```cpp
struct ClosedState {
    template<typename Event>
    auto OnEnter(const Event&) const {
        std::cout << "Door is closed" << std::endl;
        return nova::fsm::Continue{};
    }
};
```

## Actions API

See the following action types (all in `nova::fsm`):

- `TransitionTo<State>`: Transition to another state, optionally with data.
- `DoNothing`: No transition or side effect.
- `ReportError`: Terminate with error message.
- `OneOf<A1, A2, ...>`: Return one of several possible actions.
- `Maybe<A>`: Optionally return an action or do nothing.

All actions must implement:

```cpp
auto Execute(Machine&, State&, const Event&) -> Status;
```

## State Helpers API

- `On<Event, Action>`: Handles a specific event with a specific action.
- `ByDefault<Action>`: Handles all other events with a default action.
- `Will<A1, A2, ...>`: Composes multiple handlers and defaults.

## Status API

- `Continue`: Continue event processing.
- `Terminate`: Stop processing, no error.
- `TerminateWithError`: Stop processing, with error message.
- `ReissueEvent`: Reprocess the last event after a transition.

## Exception Safety

All exceptions thrown by event handlers or actions are caught by the state
machine and reported as `TerminateWithError` in the returned status. Prefer
returning `ReportError` for predictable error handling.

## Testing and Mocking

The library is designed for testability. You can mock states and actions using
Google Mock, and inspect transitions and statuses. See the unit tests for
advanced patterns.

## Advanced Usage

- **Data passing**: Use `TransitionTo<State>(data)` to pass data to the next
  state. The new state's `OnEnter` can accept the data as an argument (via
  `std::any`).
- **Multiple actions**: Use `OneOf` or `Maybe` to return different actions based
  on runtime conditions.
- **Custom status**: Extend or wrap the status variant for custom event loop
  integration.

## References

- [Original blog
  inspiration](https://sii.pl/blog/implementing-a-state-machine-in-c17/)
- See the `StateMachine.h` header and unit tests for more advanced examples.
