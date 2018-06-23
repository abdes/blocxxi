//    Copyright The Blocxxi Project Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <array>

#include <imgui.h>

#include <common/logging.h>
#include <p2p/kademlia/routing.h>
#include <ui/style/theme.h>
#include <ui/fonts/material_design_icons.h>

namespace blocxxi {
namespace debug {
namespace ui {

void ShowRoutingInfo(char const* title,
                     p2p::kademlia::RoutingTable const& router,
                     bool* open = nullptr) {
  ImGui::SetNextWindowPos(ImVec2(300, 0), ImGuiCond_Once);

  ImGui::Begin(title, open);

  Font font(Font::FAMILY_PROPORTIONAL);
  font.Bold().Italic().LargerSize();
  ImGui::PushFont(font.ImGuiFont());
  ImVec4 vec{0.3f, 0.6f, 0.8f, 1.0f};
  ImGui::TextColored(vec, "Routing Information");
  ImGui::PopFont();

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {5, 5});
  auto button_color = ImGui::GetStyleColorVec4(ImGuiCol_Button);
  button_color.w = 0.0f;
  ImGui::PushStyleColor(ImGuiCol_Button, button_color);
  if (ImGui::Button(ICON_MDI_CONTENT_COPY)) {
    ImGui::SetClipboardText(router.ThisNode().ToString().c_str());
  }
  ImGui::PopStyleColor();

  ImGui::PushTextWrapPos(0.0f);
  ImGui::SameLine();
  ImGui::TextUnformatted(router.ThisNode().ToString().c_str());
  ImGui::PopTextWrapPos();
  ImGui::PopStyleVar();

  font.LargeSize().Regular().Normal();
  ImGui::PushFont(font.ImGuiFont());

  ImGui::TextUnformatted("Buckets");
  ImGui::SameLine();
  ImGui::TextUnformatted(std::to_string(router.BucketsCount()).c_str());

  ImGui::PushItemWidth(-30);
  ImGui::TextUnformatted("Nodes");
  ImGui::SameLine();
  ImGui::TextUnformatted(std::to_string(router.NodesCount()).c_str());

  ImGui::PopFont();

  ImGui::End();
}

}  // namespace ui
}  // namespace debug
}  // namespace blocxxi
