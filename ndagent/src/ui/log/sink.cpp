//    Copyright The Blocxxi Project Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <ui/log/sink.h>

namespace blocxxi {
namespace debug {
namespace ui {

void ImGuiLogSink::Clear() {
  std::lock_guard<std::mutex> lock(_mutex);
  records_.clear();
}

void ImGuiLogSink::Draw(const char* title, bool* open) {
  ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
  ImGui::Begin(title, open);
  if (ImGui::Button("Clear")) Clear();
  ImGui::SameLine();
  display_filter_.Draw("Filter", -100.0f);
  ImGui::Separator();
  ImGui::BeginChild("scrolling", ImVec2(0, 0), false,
                    ImGuiWindowFlags_HorizontalScrollbar);

  Font font("Inconsolata");
  font.Regular().MediumSize();
  ImGui::PushFont(font.ImGuiFont());
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
  for (auto const& record : records_) {
    if (display_filter_.IsActive()) {
      if (display_filter_.PassFilter(record.c_str(),
                                     record.c_str() + record.size())) {
        ImGui::TextUnformatted(record.c_str());
      }
    } else {
      ImGui::TextUnformatted(record.c_str());
    }
  }
  ImGui::PopStyleVar();
  ImGui::PopFont();

  if (scroll_to_bottom_) ImGui::SetScrollHere(1.0f);
  scroll_to_bottom_ = false;
  ImGui::EndChild();
  ImGui::End();
}

void ImGuiLogSink::_sink_it(const spdlog::details::log_msg& msg) {
  records_.push_back(msg.formatted.str());
  scroll_to_bottom_ = true;
}

void ImGuiLogSink::_flush() {
  // Your code here
}

}  // namespace ui
}  // namespace debug
}  // namespace blocxxi
