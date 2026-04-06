//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//! State machine implementation using C++17 variant and variadic templates.
/*!
 @file
 This implementation is highly inspired by [Implementing a State Machine in
 C++17] (https://sii.pl/blog/implementing-a-state-machine-in-c17/).
*/

#pragma once

#include <any>
#include <exception>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include <Nova/Base/Compilers.h>

//! Namespace for the State Machine library.
namespace nova::fsm {

//! The event production loop should continue execution normally.
struct Continue { };

//! The state machine completed its execution with no error and
//! transitioned into a final state that does not wish to process any more
//! events.
/*!
 This status requires the event production loop to complete its execution.
 No more events should be fed into the state machine.

 @note The behavior if events keep coming after this point is state machine
       implementation dependent.
*/
struct Terminate { };

//! The event production loop should immediately terminate due to an
//! unrecoverable error in a state.
/*!
 When the error that was reported is derived from StateMachineError (as
 recommended), an explanatory message from the error exception will be placed in
 the error_message field of the returned status. Otherwise, the message will
 simply be a generic message with no specific error information.

 To report errors from event handlers, it is preferred to use the ReportError
 action rather than directly throwing an exception. This will keep the interface
 of the event handlers consistent (always returning an action) and make the
 testability of the states much easier.

 @note Continuing to send events after this point would probably create even
       more errors and is an undefined behavior.
*/
struct TerminateWithError {
  std::string error_message;
};

//! The event production loop should reissue the last event.
struct ReissueEvent { };

//! The execution status of the state machine, communicated back to the event
//! production loop after each event is handled by the state machine.
using Status
  = std::variant<Continue, Terminate, TerminateWithError, ReissueEvent>;

//! Represents a Finite State Machine (FSM).
/*!
 The FSM has a set of possible states and a set of rules that govern transitions
 between the current state and some other state upon receiving an event.

 A state can be an arbitrary object, which type is used to distinguish it from
 other states. Therefore, there is no need to maintain a list of known states
 (in an enumeration for example). We also don't want to enforce any relationship
 between those types to keep them completely independent. The state machine is a
 variadic template that gets to know its states through the parameter pack and
 stores them in a tuple.

 The state machine keeps track of the current state as a `std::variant` (which
 can accommodate the diversity of state types) and assumes that the first state
 provided to it is the *initial state*. It is possible however to explicitly
 transition the state machine into a specific state using its `TransitionTo()`
 method.

 Transitions from the current state to other states are triggered by events.
 This requires that all the state types have a `Handle` method that accepts an
 event of the type of event being handled. To avoid creating unnecessary
 dependencies between the state types and the state machine type, a state's
 `Handle` method will return an object of a specific type (an action) that will
 describe what action should the machine take.

 @snippet StateMachine_doc_test.cpp State Handle method

 In principle, a state machine should always transition to some state after
 receiving an event, even if it’s the same state as the current one. When we
 want to skip performing a transition action, we can simply use the helper
 action type 'DoNothing'.

 Events are produced outside the state machine and fed into it by an external
 event loop which calls the state machine's `Handle()` method. The latter
 dispatches the event to the current state, which returns a specific action that
 gets executed to eventually transition to a new state. Event handlers, the
 action execution, and anything inside could throw exceptions to report errors
 that are *all* caught and translated into an execution status returned to the
 event production loop.

 The execution status can take one of the following types documented in the
 class: Continue, Terminate, TerminateWithError and ReissueEvent. Refer to the
 documentation of these types and the StateMachine::Status.

 ### Full Example

 @snippet StateMachine_doc_test.cpp Full State Machine Example
*/
template <typename... States> class StateMachine {
public:
  /*!
   * @brief Construct a new State Machine object with the given states, starting
   * at the first state in the list.
   *
   * @see TransitionTo() if you need to start at a specific state other than the
   * first one in the list.
   */
  explicit StateMachine(States... states)
    : states_(std::move(states)...)
  {
  }

  //! Dispatch the event to the current state and execute the returned action.
  /*!
   The state machine is animated by an external event loop that produces events,
   passed to this method. Each event is dispatched to the current state for
   handling, which returns an action object that get executed. The result of
   executing the action is translated into an execution status for the state
   machine that can be to continue processing more events, terminate with no
   error or terminate with error.

   A special case is when an event results in a state transition but the new
   state prefers to handle the event in one of its handlers rather than in the
   `OnEnter` method. That particular situation corresponds to the execution
   status `ReissueEvent` which requires the event production loop to reissue
   that same event again.
  */
  template <typename Event> auto Handle(const Event& event) -> Status
  {
    auto pass_event_to_state = [this, &event](auto statePtr) noexcept -> auto {
      // Throwing exception from event handlers is not recommended but can still
      // be done. We'll try to translate this to a TerminateWithError status
      // anyway.
      try {
        auto action = statePtr->Handle(event);
        return action.Execute(*this, *statePtr, event);
      } catch (const std::exception& err) {
        return Status { TerminateWithError { err.what() } };
      } catch (...) {
        return Status { TerminateWithError { "not std::exception" } };
      }
    };
    return std::visit(pass_event_to_state, current_state_);
  }

  //! Explicitly transition the State Machine to a specific state without
  //! executing a transition action.
  /*!
   This is provided for the sole purpose of setting the State Machine to a
   specific starting state from the external event production loop. Transitions
   from event handlers should exclusively use the TransitionTo action.

   @tparam State the state to which the State Machine should transition.
   @return State& the new state.
  */
  template <typename State> auto TransitionTo() -> State&
  {
    auto& state = std::get<State>(states_);
    current_state_ = &state;
    return state;
  }

  //! Check if the State Machine is in a particular state.
  /*!
   @tparam State the state to be checked for.
   @return *true* if the State Machine's current state is the state being
           checked for; *false* otherwise.
  */
  template <typename State> [[nodiscard]] auto IsIn() const -> bool
  {
    const auto& state = std::get<State>(states_);
    try {
      return std::get<State*>(current_state_) == &state;
    } catch (const std::bad_variant_access& /*error*/) {
      return false;
    }
  }

private:
  std::tuple<States...> states_;
  std::variant<States*...> current_state_ { &std::get<0>(states_) };
};

//! An action type that represents a state transition from the current state to
//! the target state specified as a template parameter.
/*!
 This action supports rich states which may contain data and may have some
 complex logic that utilizes that data to decide on the next state transition.
 When being executed by the state machine, it is provided with the current state
 (through the state machine instance), the newly selected state and the event
 that triggered the transition.

 Furthermore, it calls the OnLeave method on the previous state and OnEnter on
 the destination state, provided they are in the states interface. (Uses SFINAE
 to detect if such a method exists; see
 https://en.cppreference.com/w/cpp/language/sfinae)

 The requirement on the OnLeave method is to return a State object that can only
 be one of Continue, Terminate or TerminateWithError. The OnEnter method on the
 other hand can return any of the possible values of Status.

 ### Example

 @snippet StateMachine_test.cpp TransitionTo example

 A state can implement a Handle method that returns a TransitionTo object
 constructed with a piece of data (std::any object) that will be passed to the
 next state when its onEnter method is called. The next state can opt to ignore
 that data by implementing onEnter(Event evt) only or can opt to use the data
 by implementing onEnter(Event evt, std::any data).

 ### Example of data passing between states

 @snippet StateMachine_test.cpp TransitionTo with data example
*/
template <typename TargetState> struct TransitionTo {
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  TransitionTo(std::any _data = {})
    : data { std::move(_data) }
  {
  }

  template <typename Machine, typename State, typename Event>
  auto Execute(Machine& machine, State& prevState, const Event& event) -> Status
  {
    // Call OnLeave and if it fails or requests termination, do not transition
    // to the next state.
    if (Status status = Leave(prevState, event);
      std::holds_alternative<Terminate>(status)
      || std::holds_alternative<TerminateWithError>(status)) {
      return status;
    }
    auto& new_state = machine.template TransitionTo<TargetState>();
    if (data.has_value()) {
      return Enter(new_state, event, data);
    }
    return Enter(new_state, event);
  }

  [[nodiscard]] auto Data() const noexcept -> const std::any& { return data; }

  template <typename ActionType> [[nodiscard]] auto IsA() const noexcept -> bool
  {
    return std::is_same_v<ActionType, TransitionTo>;
  }

  template <typename ActionType> auto GetAs() -> ActionType&
  {
    static_assert(std::is_same_v<ActionType, TransitionTo>,
      "do not use GetAs() with the wrong type");
    return static_cast<ActionType&>(*this);
  }

  template <typename ActionType>
  [[nodiscard]] auto GetAs() const -> const ActionType&
  {
    static_assert(std::is_same_v<ActionType, TransitionTo>,
      "do not use GetAs() with the wrong type");
    return static_cast<const ActionType&>(*this);
  }

private:
  // This overload is always in the set of overloads. Ellipsis parameter has the
  // lowest ranking for overload resolution.
  template <typename... Args> static auto Leave(Args... /*unused*/) -> Status
  {
    return Continue {};
  }

  // SFINAE: This overload is for states that implement `onLeave` with an event
  // parameter.
  template <typename State, typename Event>
  [[maybe_unused]] static auto Leave(State& state, const Event& event)
    -> decltype(state.OnLeave(event))
  {
    return state.OnLeave(event);
  }

  // This overload is always in the set of overloads. Ellipsis parameter has the
  // lowest ranking for overload resolution.
  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  template <typename... Args> static auto Enter(Args... /*unused*/) -> Status
  {
    return Continue {};
  }

  // SFINAE: This overload is for states that implement `onEnter` with an event
  // parameter. Data from the previous event, if any, will be discarded.
  template <typename State, typename Event>
  [[maybe_unused]] static auto Enter(State& state, const Event& event)
    -> decltype(state.OnEnter(event))
  {
    return state.OnEnter(event);
  }

  // SFINAE: This overload is for states that implement `onEnter` with an event
  // parameter and a data parameter, indicating that it is expecting data from
  // the previous state.
  template <typename State, typename Event>
  static auto Enter(State& state, const Event& event, const std::any& _data)
    -> decltype(state.OnEnter(event, _data))
  {
    return state.OnEnter(event, _data);
  }

  std::any data;
};

//! Helper action representing the situation where an event is ignored and
//! the current state stays as is.
/*!
 ### Example

 @snippet StateMachine_test.cpp DoNothing action example

 A typical use of this action is in combination with ByDefault to make a state
 that by default does nothing with any event it receives and therefore only
 focuses on implementing those event handlers that make sense for that
 particular state.
*/
struct DoNothing {
  template <typename Machine, typename State, typename Event>
  static auto Execute(
    Machine& /*unused*/, State& /*unused*/, const Event& /*unused*/) -> Status
  {
    return Continue {};
  }

  [[nodiscard]] static auto Data() noexcept -> const std::any&
  {
    NOVA_DIAGNOSTIC_PUSH
#ifdef ASAP_CLANG_VERSION
    ASAP_PRAGMA(clang diagnostic ignored "-Wexit-time-destructors")
#endif
    // Nothing to worry about here for the exit time destructor.
    static const std::any data {};
    NOVA_DIAGNOSTIC_POP
    return data;
  }

  template <typename ActionType> [[nodiscard]] auto IsA() const noexcept -> bool
  {
    return std::is_same_v<ActionType, DoNothing>;
  }

  template <typename ActionType> auto GetAs() -> ActionType&
  {
    static_assert(std::is_same_v<ActionType, DoNothing>,
      "do not use GetAs() with the wrong type");
    return static_cast<ActionType&>(*this);
  }

  template <typename ActionType>
  [[nodiscard]] auto GetAs() const -> const ActionType&
  {
    static_assert(std::is_same_v<ActionType, DoNothing>,
      "do not use GetAs() with the wrong type");
    return static_cast<const ActionType&>(*this);
  }
};

//! The action to be used to report an error from an event handler.
/*!
 Throwing an exception from event handlers to report an error is not recommended
 as it will break the interface of the event handlers (expected to return an
 action to be executed) and make the unit testing more complicated.

 Instead, the event handler would use this action to report the error, providing
 the error explanatory message in the form of the action's data.
*/
struct ReportError {
  explicit ReportError(std::string what)
    : error(std::move(what))
  {
  }

  template <typename Machine, typename State, typename Event>
  auto Execute(Machine& /*unused*/, State& /*unused*/, const Event& /*unused*/)
    -> Status
  {
    return TerminateWithError { std::any_cast<std::string>(error) };
  }

  [[nodiscard]] auto Data() const noexcept -> const std::any& { return error; }

  template <typename ActionType> [[nodiscard]] auto IsA() const noexcept -> bool
  {
    return std::is_same_v<ActionType, ReportError>;
  }

  template <typename ActionType> auto GetAs() -> ActionType&
  {
    static_assert(std::is_same_v<ActionType, ReportError>,
      "do not use GetAs() with the wrong type");
    return static_cast<ActionType&>(*this);
  }

  template <typename ActionType>
  [[nodiscard]] auto GetAs() const -> const ActionType&
  {
    static_assert(std::is_same_v<ActionType, ReportError>,
      "do not use GetAs() with the wrong type");
    return static_cast<const ActionType&>(*this);
  }

private:
  std::any error;
};

//! Concept to identify OneOf types with a specific alternative.
/*!
 Checks if type T is one of the types Ts.

 @tparam T The type to check.
 @tparam Ts The list of types to check against.
 @return true if T is one of Ts, false otherwise.
*/
template <typename T, typename... Ts>
constexpr auto supports_alternative() -> bool
{
  return std::disjunction_v<std::is_same<T, Ts>...>;
}

//! Base type for states modeling the situation where the result of an event can
//! be one of a set of possible actions.
/*!
 Since actions are returned from state Handle methods and can have only one
 return type, this class multiplexes the return types using the variadic
 template parameter pack.

 ### Example

 @snippet StateMachine_test.cpp OneOf example

 @tparam Actions The set of possible action types.
*/
template <typename... Actions> struct OneOf {
  template <typename T,
    std::enable_if<
      !std::is_base_of_v<OneOf, std::remove_reference_t<T>>>* = nullptr>
  // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions,bugprone-forwarding-reference-overload)
  OneOf(T&& arg)
    : option(std::forward<T>(arg))
  {
  }

  template <typename Machine, typename State, typename Event>
  auto Execute(Machine& machine, State& state, const Event& event) -> Status
  {
    return std::visit(
      [&machine, &state, &event](
        auto& action) -> auto { return action.Execute(machine, state, event); },
      option);
  }

  [[nodiscard]] auto Data() const noexcept -> const std::any&
  {
    return std::visit(
      [](auto& action) noexcept -> const std::any& { return action.Data(); },
      option);
  }

  template <typename ActionType> [[nodiscard]] auto IsA() const noexcept -> bool
  {
    // Here we use a visitor to check if one of the alternatives satisfies the
    // requested type instead of using
    // `std::holds_alternative<ActionType>(option)` because we do not want a
    // strict compile-time check that would make it very difficult to handle
    // actions generically when needed.
    //
    // For example, testing an action like this: `action.IsA<DoNothing>()`,
    // would require that any `OneOf` action being tested must include the
    // `DoNothing` in the alternatives, which is sometimes not the case. With
    // the visitor however, such condition is not required.
    return std::visit(
      [](auto& action) noexcept -> bool {
        return action.template IsA<ActionType>();
      },
      option);
  }

  template <typename ActionType> auto GetAs() -> ActionType&
  {
    static_assert(supports_alternative<ActionType, Actions...>(),
      "do not use GetAs() with the wrong type");
    return std::get<ActionType>(option);
  }

  template <typename ActionType>
  [[nodiscard]] [[nodiscard]] auto GetAs() const -> const ActionType&
  {
    static_assert(supports_alternative<ActionType, Actions...>(),
      "do not use GetAs() with the wrong type");
    return std::get<ActionType>(option);
  }

private:
  std::variant<Actions...> option;
};

//! Concept to identify if a particular type is a OneOf<...Actions> type.
template <typename /*T*/> struct is_one_of : std::false_type { };
template <typename... Actions>
struct is_one_of<OneOf<Actions...>> : std::true_type { };
template <typename T> constexpr bool is_one_of_v = is_one_of<T>::value;

//! Base type for states modeling the situation where an event may do something
//! or nothing at all.
/*!
 Provides a Maybe type that is a OneOf of Action and DoNothing.

 **Example**

 @snippet StateMachine_test.cpp OneOf example

 @tparam Action The main action type.
*/
template <typename Action> struct Maybe : OneOf<Action, DoNothing> {
  using OneOf<Action, DoNothing>::OneOf;
};

template <typename... Actions>
struct is_one_of<Maybe<Actions...>> : std::true_type { };

//! Base type for states to facilitate specification of a default action.
/*!
 States need to have handlers for all events that go through the state machine,
 even though they are really interested only in some specific events. For the
 rest of the events, either nothing is done or in some special cases a default
 action is returned. This base type provides a straightforward way to express
 the default action for a state.

 ### Example

 @snippet StateMachine_test.cpp ByDefault example

 @tparam Action The default action type.
*/
template <typename Action> struct ByDefault {
  template <typename Event>
  static auto Handle(const Event& /*unused*/) -> Action
  {
    return Action {};
  }
};

//! Base type for states to facilitate implementation of handlers that simply
//! return a default-constructed action of a certain type.
/*!
 Provides a handler for a specific event type that returns a default-constructed
 action.

 ### Example

 @snippet StateMachine_test.cpp On example

 @tparam Event The event type.
 @tparam Action The action type to return.
*/
template <typename Event, typename Action> struct On {
  static auto Handle(const Event& /*event*/) -> Action { return {}; }
};

//! Base type for states to automate pulling all handlers from all base classes.
/*!
 Allows a state to inherit handlers from multiple base handler types.

 ### Example

 @snippet StateMachine_test.cpp Will example

 @tparam Handlers The base handler types to inherit from.
*/
template <typename... Handlers> struct Will : Handlers... {
  using Handlers::Handle...;
};

} // namespace nova::fsm
