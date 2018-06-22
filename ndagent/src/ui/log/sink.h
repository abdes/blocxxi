//    Copyright The Blocxxi Project Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <vector>

#include <spdlog/sinks/sink.h>
#include <spdlog/spdlog.h>

#include <ui/style/theme.h>


namespace blocxxi {
namespace debug {
namespace ui {

class ImGuiLogSink : public spdlog::sinks::base_sink<std::mutex> {
 public:
  void Clear() {
    std::lock_guard<std::mutex> lock(_mutex);
    records_.clear();
  }

  void Draw(const char* title, bool* p_open = NULL) {
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin(title, p_open);
    if (ImGui::Button("Clear")) Clear();
    ImGui::SameLine();
    display_filter_.Draw("Filter", -100.0f);
    ImGui::Separator();
    ImGui::BeginChild("scrolling", ImVec2(0, 0), false,
                      ImGuiWindowFlags_HorizontalScrollbar);

    Font font("Inconsolata");
    font.Regular().MediumSize();
    ImGui::PushFont(font.ImGuiFont());
	for (auto record : records_) {
      if (display_filter_.IsActive()) {
            if (display_filter_.PassFilter(record.c_str(),
                                           record.c_str() + record.size())) {
          ImGui::TextWrapped(record.c_str());
        }
      } else {
        ImGui::TextWrapped(record.c_str());
      }
    }
    ImGui::PopFont();

    if (scroll_to_bottom_) ImGui::SetScrollHere(1.0f);
    scroll_to_bottom_ = false;
    ImGui::EndChild();
    ImGui::End();
  }

 protected:
  void _sink_it(const spdlog::details::log_msg& msg) override {
    records_.push_back(msg.formatted.str());
    scroll_to_bottom_ = true;
  }

  void _flush() override {
    // Your code here
  }

 private:
  std::vector<std::string> records_;
  ImGuiTextFilter display_filter_;

  bool scroll_to_bottom_;
};

}  // namespace ui
}  // namespace debug
}  // namespace blocxxi