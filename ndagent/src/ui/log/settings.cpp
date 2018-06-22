//    Copyright The Blocxxi Project Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <array>

#include <common/logging.h>

#include <imgui.h>

namespace blocxxi {
namespace debug {
namespace ui {

void ShowLogSettings(char const* title, bool* open = nullptr) {
  ImGui::SetNextWindowPos(ImVec2(300, 0), ImGuiCond_Once);

  ImGui::Begin(title, open);

  static constexpr int IDS_COUNT = 9;
  static std::array<blocxxi::logging::Id, IDS_COUNT> logger_ids{
      {blocxxi::logging::Id::MISC, blocxxi::logging::Id::TESTING,
       blocxxi::logging::Id::COMMON, blocxxi::logging::Id::CODEC,
       blocxxi::logging::Id::CRYPTO, blocxxi::logging::Id::NAT,
       blocxxi::logging::Id::P2P, blocxxi::logging::Id::P2P_KADEMLIA,
       blocxxi::logging::Id::NDAGENT}};
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
  ImGui::End();
}

}  // namespace ui
}  // namespace debug
}  // namespace blocxxi
