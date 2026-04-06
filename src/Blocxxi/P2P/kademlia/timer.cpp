//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/P2P/kademlia/timer.h>

#include <Blocxxi/P2P/kademlia/detail/error_impl.h>

namespace blocxxi::p2p::kademlia {

void Timer::ScheduleNextTick(TimePointType const& expiration_time)
{
  // This will cancel any pending task.
  timer_.expires_at(expiration_time);

  timer_.async_wait([this](std::error_code const& failure) {
    // If the deadline timer has been cancelled, just stop right there.
    if (failure == asio::error::operation_aborted) {
      return;
    }

    if (failure) {
      throw std::system_error { detail::make_error_code(TIMER_MALFUNCTION) };
    }

    // Call the user callbacks.
    // The callbacks to execute are the first n callbacks expiring at a time
    // before now().
    const auto begin = timeouts_.begin();
    const auto end = timeouts_.upper_bound(ClockType::now());
    for (auto entry = begin; entry != end; ++entry) {
      LOG_F(2, "invoke callback for timer expiring at {}",
        entry->first.time_since_epoch().count());
      entry->second();
    }

    // And remove the timeout.
    auto count = std::distance(begin, end);
    timeouts_.erase(begin, end);

    LOG_F(
      1, "remove {} expired timer(s), remaining {}", count, timeouts_.size());

    // If there is a remaining timeout, schedule it.
    if (!timeouts_.empty()) {
      ScheduleNextTick(timeouts_.begin()->first);
    } else {
      LOG_F(1, "no more timers to schedule");
    }
  });
}

} // namespace blocxxi::p2p::kademlia
