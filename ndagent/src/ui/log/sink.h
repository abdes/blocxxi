//    Copyright The Blocxxi Project Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <vector>

#include <spdlog/sinks/sink.h>
#include <spdlog/spdlog.h>

#include <ui/style/theme.h>
#include <imgui.h>

namespace blocxxi {
namespace debug {
namespace ui {

class ImGuiLogSink : public spdlog::sinks::base_sink<std::mutex> {
 public:
  void Clear();

  void Draw(const char* title, bool* p_open = nullptr);

 protected:
  void _sink_it(const spdlog::details::log_msg& msg) override;

  void _flush() override;

 private:
  std::vector<std::string> records_;
  ImGuiTextFilter display_filter_;

  bool scroll_to_bottom_;
};

}  // namespace ui
}  // namespace debug
}  // namespace blocxxi
