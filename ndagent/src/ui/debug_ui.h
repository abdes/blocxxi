//    Copyright The Blocxxi Project Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <ui/style/theme.h>
#include <ui/log/sink.h>

namespace blocxxi {
namespace debug {
namespace ui {

extern void ShowLogSettings(char const* title, bool* open = nullptr);
extern void ShowRoutingInfo(char const* title, p2p::kademlia::RoutingTable const &router, bool* open = nullptr);

}  // namespace ui
}  // namespace debug
}  // namespace blocxxi
