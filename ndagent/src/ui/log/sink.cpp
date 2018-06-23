//    Copyright The Blocxxi Project Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <array>
#include <strstream>  // For log record formatting

#include <date/date.h>

#include <ui/fonts/material_design_icons.h>
#include <ui/log/sink.h>

#include <common/logging.h>

namespace blocxxi {
namespace debug {
namespace ui {

const ImVec4 ImGuiLogSink::COLOR_WARN{0.9f, 0.7f, 0.0f, 1.0f};
const ImVec4 ImGuiLogSink::COLOR_ERROR{1.0f, 0.0f, 0.0f, 1.0f};

void ImGuiLogSink::Clear() {
  std::unique_lock<std::shared_timed_mutex> lock(records_mutex_);
  records_.clear();
}

void ImGuiLogSink::ShowLogLevelsPopup() {
  static constexpr int IDS_COUNT = 9;
  static std::array<blocxxi::logging::Id, IDS_COUNT> logger_ids{
      {blocxxi::logging::Id::MISC, blocxxi::logging::Id::TESTING,
       blocxxi::logging::Id::COMMON, blocxxi::logging::Id::CODEC,
       blocxxi::logging::Id::CRYPTO, blocxxi::logging::Id::NAT,
       blocxxi::logging::Id::P2P, blocxxi::logging::Id::P2P_KADEMLIA,
       blocxxi::logging::Id::NDAGENT}};

  ImGui::MenuItem("Logging Levels", NULL, false, false);

  for (auto id : logger_ids) {
    auto& the_logger = blocxxi::logging::Registry::GetLogger(id);
    static int levels[IDS_COUNT];
    auto id_value =
        static_cast<typename std::underlying_type<blocxxi::logging::Id>::type>(
            id);
    levels[id_value] = the_logger.level();
    auto format = std::string("%u (")
                      .append(spdlog::level::to_str(
                          spdlog::level::level_enum(levels[id_value])))
                      .append(")");
    if (ImGui::SliderInt(the_logger.name().c_str(), &levels[id_value], 0, 6,
                         format.c_str())) {
      the_logger.set_level(spdlog::level::level_enum(levels[id_value]));
    }
  }
}

void ImGuiLogSink::ShowLogFormatPopup() {
  ImGui::MenuItem("Logging Format", NULL, false, false);
  ImGui::Checkbox("Time", &show_time_);
  ImGui::SameLine();
  ImGui::Checkbox("Thread", &show_thread_);
  ImGui::SameLine();
  ImGui::Checkbox("Level", &show_level_);
  ImGui::SameLine();
  ImGui::Checkbox("Logger", &show_logger_);
  ImGui::SameLine();
  ImGui::Checkbox("Source", &show_source_);
}

void ImGuiLogSink::Draw(const char* title, bool* open) {
  ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
  ImGui::Begin(title, open);
  if (ImGui::Button("Log something...")) {
    BXLOG_MISC(trace, "TRACE");
    BXLOG_MISC(debug, "DEBUG");
    BXLOG_MISC(info, "INFO");
    BXLOG_MISC(warn, "WARN");
    BXLOG_MISC(error, "ERROR");
    BXLOG_MISC(critical, "CRITICAL");
  }
  if (ImGui::Button(ICON_MDI_SETTINGS " Levels")) {
    ImGui::OpenPopup("LogLevelsPopup");
  }
  if (ImGui::BeginPopup("LogLevelsPopup")) {
    ShowLogLevelsPopup();
    ImGui::EndPopup();
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_MDI_VIEW_COLUMN " Format")) {
    ImGui::OpenPopup("LogFormatPopup");
  }
  if (ImGui::BeginPopup("LogFormatPopup")) {
    ShowLogFormatPopup();
    ImGui::EndPopup();
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_MDI_NOTIFICATION_CLEAR_ALL " Clear")) Clear();

  ImGui::SameLine();
  bool need_pop_style_var = false;
  if (wrap_) {
	  // Highlight the button
	  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
	  ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_TextSelectedBg));
	  need_pop_style_var = true;
  }
  if (ImGui::Button(ICON_MDI_WRAP)) ToggleWrap();
  if (need_pop_style_var) {
	  ImGui::PopStyleColor();
	  ImGui::PopStyleVar();
  }

  ImGui::SameLine();
  display_filter_.Draw(ICON_MDI_FILTER " Filter", -100.0f);
  ImGui::Separator();
  ImGui::BeginChild("scrolling", ImVec2(0, 0), false,
                    ImGuiWindowFlags_HorizontalScrollbar);

  Font font("Inconsolata");
  font.MediumSize();
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
  {
	  std::shared_lock<std::shared_timed_mutex> lock(records_mutex_);

	  for (auto const& record : records_) {
		  if (!display_filter_.IsActive() ||
			  display_filter_.PassFilter(
				  record.text_.c_str(), record.text_.c_str() + record.text_.size())) {
			  // Style the text and show it
			  if (record.emphasis_) {
				  font.Bold();
			  }
			  else {
				  font.Regular();
			  }
			  ImGui::PushFont(font.ImGuiFont());
			  if (record.color_range_start_ > 0) {
				  std::string part = record.text_.substr(0, record.color_range_start_);
				  if (wrap_) ImGui::PushTextWrapPos(0.0f);
				  ImGui::TextUnformatted(part.c_str());
				  if (wrap_) ImGui::PopTextWrapPos();
				  ImGui::SameLine();
			  }
			  if (record.color_range_end_ > record.color_range_start_) {
				  std::string part = record.text_.substr(
					  record.color_range_start_,
					  record.color_range_end_ - record.color_range_start_);
				  if (wrap_) ImGui::PushTextWrapPos(0.0f);
				  ImGui::TextColored(record.color_, part.c_str());
				  if (wrap_) ImGui::PopTextWrapPos();
			  }
			  auto msg_len = record.text_.size();
			  if (record.color_range_end_ < msg_len) {
				  std::string part = record.text_.substr(
					  record.color_range_end_, msg_len - record.color_range_end_);
				  ImGui::SameLine();
				  if (wrap_) ImGui::PushTextWrapPos(0.0f);
				  ImGui::TextUnformatted(part.c_str());
				  if (wrap_) ImGui::PopTextWrapPos();
			  }
			  ImGui::PopFont();
		  }
	  }
  }
  ImGui::PopStyleVar();

  if (scroll_to_bottom_) ImGui::SetScrollHere(1.0f);
  scroll_to_bottom_ = false;
  ImGui::EndChild();
  ImGui::End();
}

void ImGuiLogSink::_sink_it(const spdlog::details::log_msg& msg) {
  auto ostr = std::ostringstream();
  std::size_t color_range_start = 0;
  std::size_t color_range_end = 0;
  ImVec4 const* color = nullptr;
  auto emphasis = false;

  if (show_time_) {
    ostr << "["
         << date::format("%D %T %Z", floor<std::chrono::milliseconds>(msg.time))
         << "] ";
  }
  if (show_thread_) {
    ostr << "[" << msg.thread_id << "] ";
  }
  if (show_level_) {
    color_range_start = ostr.tellp();
    ostr << "[" << spdlog::level::to_short_str(msg.level) << "] ";
    color_range_end = ostr.tellp();
  }
  if (show_logger_) {
    ostr << "[" << *msg.logger_name << "]";
  }

  // Strip the filename:line from the message if needed
  auto msg_str = msg.raw.str();
  auto skip_to = msg_str.begin();
  if (!show_source_) {
    if (*skip_to == '[') {
      while (skip_to != msg_str.end() && *skip_to != ']') ++skip_to;
      if (skip_to == msg_str.end()) {
        skip_to = msg_str.begin();
      } else {
        auto saved_skip_to = skip_to;
        while (skip_to != msg_str.begin()) {
          --skip_to;
          if (*skip_to == ':' || !std::isdigit(*skip_to)) break;
        }
        if (*skip_to == ':') skip_to = ++saved_skip_to;
      }
    }
    ostr << &(*skip_to);
  } else {
    ostr << " " << &(*skip_to);
  }

  // Select display color and colored text range based on level
  switch (msg.level) {
    case spdlog::level::trace:
      color = &ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
      // the entire message
      color_range_start = 0;
      color_range_end = ostr.tellp();
      break;

    case spdlog::level::debug:
      color = &ImGui::GetStyleColorVec4(ImGuiCol_Text);
      // The level part if show, otherwise no coloring
      break;

    case spdlog::level::info:
      color = &ImGui::GetStyleColorVec4(ImGuiCol_NavHighlight);
      // The level part if show, otherwise no coloring
      break;

    case spdlog::level::warn:
      color = &COLOR_WARN;
      // the entire message
      color_range_start = 0;
      color_range_end = ostr.tellp();
      break;

    case spdlog::level::err:
      color = &COLOR_ERROR;
      // the entire message
      color_range_start = 0;
      color_range_end = ostr.tellp();
      break;

    case spdlog::level::critical:
      color = &COLOR_ERROR;
      emphasis = true;
      // the entire message
      color_range_start = 0;
      color_range_end = ostr.tellp();
      break;

    default:
        // Nothing
        ;
  }
  // Adjust color range
  auto record = LogRecord{std::move(ostr.str()), color_range_start,
                          color_range_end, *color, emphasis};
  {
    std::unique_lock<std::shared_timed_mutex> lock(records_mutex_);
    records_.push_back(std::move(record));
  }
  scroll_to_bottom_ = true;
}

void ImGuiLogSink::_flush() {
  // Your code here
}

}  // namespace ui
}  // namespace debug
}  // namespace blocxxi
