//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <chrono>     // for std:: time related types
#include <functional> // for std::function (callbacks)
#include <map>        // for multimap storing timers<->callbacks

#include <p2p/kademlia/boost_asio.h>

#include <logging/logging.h>

#include <p2p/blocxxi_p2p_api.h>

namespace blocxxi::p2p::kademlia {

/// Timer management class using a single asynchronous timer to run a list of
/// timeouts associated with callback handlers.
class BLOCXXI_P2P_API Timer final : asap::logging::Loggable<Timer> {
public:
  /// The logger id used for logging within this class.
  static constexpr const char *LOGGER_NAME = "p2p-kademlia";

  /// Clock type for timer duration, we are using a steady_clock, most suitable
  /// for measuring intervals.
  using ClockType = std::chrono::steady_clock;

  /// Timer duration type, can be expressed using any of std::chrono helpers
  /// (nanoseconds, microseconds, milliseconds, seconds, minutes and hours).
  using DurationType = ClockType::duration;

  /// @name Constructors etc.
  //@{
  /*!
   * @brief Create an instance of the timer management class using the provided
   * boost::asio::io_context object.
   *
   * The io_context should be run by the user of the Timer class.
   *
   * @param io_context the boost::asio::io_context to be used for the management
   * of timeouts.
   */
  explicit Timer(boost::asio::io_context &io_context) : timer_{io_context} {
    ASLOG(debug, "Creating Timer DONE");
  }

  /// Destructor cancels any pending asynchronous wait opration for the timers.
  ~Timer() {
    ASLOG(debug, "Destroy Timer");
    timer_.cancel();
  }

  /// Deleted
  Timer(Timer const &) = delete;
  /// Deleted
  auto operator=(Timer const &) -> Timer & = delete;

  /// Move constructor cancels other deadline timer, moves all timeouts and
  /// starts a new deadline timer waiting for them.
  Timer(Timer &&other) noexcept
      : timer_(std::move(other.timer_)), timeouts_(std::move(other.timeouts_)) {
    other.timer_.cancel();
    if (!timeouts_.empty()) {
      ScheduleNextTick(timeouts_.begin()->first);
    }
  }

  /// Move assignments cancels both deadline timers, moves all timeouts and
  /// starts a new deadline timer waiting for them.
  auto operator=(Timer &&other) noexcept -> Timer & {
    timer_.cancel();
    other.timer_.cancel();
    timeouts_ = std::move(other.timeouts_);
    if (!timeouts_.empty()) {
      ScheduleNextTick(timeouts_.begin()->first);
    }
    return *this;
  }
  //@}

  /*!
   * @brief Start a timer that should expire after the given timeout duration
   * from the current time.
   *
   * @tparam CallbackType the signature of the callback function to be invoked
   * when the timer expires.
   * @param timeout the duration after which the timer expires (reference is
   * current time).
   * @param on_timer_expired callabck to be invoked when the timer expires.
   */
  template <typename CallbackType>
  void ExpiresFromNow(
      DurationType const &timeout, CallbackType const &on_timer_expired);

private:
  /// Represents a point in time. It is implemented as if it stores a value of
  /// type Duration indicating the time interval from the start of the Clock's
  /// epoch.
  using TimePointType = ClockType::time_point;

  /// Function callback type invoked at expiration of a timer.
  using CallbackType = std::function<void(void)>;

  /// A collection type for the association of timers expiry point in time to
  /// callbacks.
  using TimeoutsCollectionType = std::multimap<TimePointType, CallbackType>;

  /*! The boost::asio::basic_waitable_timer template accepts an optional
   * WaitTraits
   * template parameter. The underlying time_t clock has one-second granularity,
   * so these traits may be customised to reduce the latency between the clock
   * ticking over and a wait operation's completion. When the timeout is near
   * (less than one second away) we poll the clock more frequently to detect the
   * time change closer to when it occurs. The user can select the appropriate
   * trade off between accuracy and the increased CPU cost of polling. In
   * extreme cases, a zero duration may be returned to make the timers as
   * accurate as possible, albeit with 100% CPU usage.
   */
  struct ClockTypeWaitTraits {
    // Determine how long until the clock should be next polled to determine
    // whether the duration has elapsed.
    static auto to_wait_duration(const ClockType::duration &duration)
        -> ClockType::duration {
      if (duration > boost::asio::chrono::seconds(1)) {
        return duration - boost::asio::chrono::seconds(1);
      }
      if (duration > boost::asio::chrono::seconds(0)) {
        return boost::asio::chrono::milliseconds(10);
      }
      return boost::asio::chrono::seconds(0);
    }

    // Determine how long until the clock should be next polled to determine
    // whether the absolute time has been reached.
    static auto to_wait_duration(const ClockType::time_point &until)
        -> ClockType::duration {
      return to_wait_duration(until - ClockType::now());
    }
  };

  /// A timer type providing the ability to perform a blocking or asynchronous
  /// wait for a timer to expire.
  // using DeadlineTimerType = boost::asio::basic_waitable_timer<ClockType>;
  using DeadlineTimerType =
      boost::asio::basic_waitable_timer<ClockType, ClockTypeWaitTraits>;

  void ScheduleNextTick(TimePointType const &expiration_time);

  /// The deadline timer used to wait for all registered timeouts.
  DeadlineTimerType timer_;
  /// The collection of registered timeouts. Key is the point in time, value is
  /// the callback.
  TimeoutsCollectionType timeouts_;
};

template <typename Callback>
void Timer::ExpiresFromNow(
    DurationType const &timeout, Callback const &on_timer_expired) {
  auto expiration_time = ClockType::now() + timeout;
  ASLOG(debug, "adding timer expiring at {}",
      expiration_time.time_since_epoch().count());

  // If the current expiration time will be the sooner to expires
  // then cancel any pending wait and schedule this one instead.
  if (timeouts_.empty() || expiration_time < timeouts_.begin()->first) {
    ScheduleNextTick(expiration_time);
  }

  timeouts_.emplace(expiration_time, on_timer_expired);
}

} // namespace blocxxi::p2p::kademlia
