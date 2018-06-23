//    Copyright The Blocxxi Project Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <vector>        // for the records vector
#include <shared_mutex>  // for locking the records vector

#include <spdlog/sinks/sink.h>
#include <spdlog/spdlog.h>

#include <imgui.h>
#include <ui/style/theme.h>

namespace blocxxi {
namespace debug {
namespace ui {

class ImGuiLogSink : public spdlog::sinks::base_sink<std::mutex> {
 public:
  void Clear();

  void ShowLogLevelsPopup();

  void ShowLogFormatPopup();

  void ToggleWrap() { wrap_ = !wrap_; }

  void ToggleScrollLock() { scroll_lock_ = !scroll_lock_; }

  void Draw(const char* title, bool* p_open = nullptr);

 protected:
  void _sink_it(const spdlog::details::log_msg& msg) override;

  void _flush() override;

 private:
  static const ImVec4 COLOR_WARN;
  static const ImVec4 COLOR_ERROR;

  struct LogRecord {
    std::string text_;
    std::size_t color_range_start_{0};
    std::size_t color_range_end_{0};
    const ImVec4& color_;
    bool emphasis_{false};
  };
  std::vector<LogRecord> records_;
  mutable std::shared_timed_mutex records_mutex_;
  ImGuiTextFilter display_filter_;

  bool scroll_to_bottom_;
  bool wrap_{ false };
  bool scroll_lock_{ false };

  /// @name Log Format flags
  //@{
  bool show_time_{true};
  bool show_thread_{true};
  bool show_level_{true};
  bool show_logger_{true};
  bool show_source_{false};
  //@}
};

}  // namespace ui
}  // namespace debug
}  // namespace blocxxi
