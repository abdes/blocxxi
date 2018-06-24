//    Copyright The Blocxxi Project Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <array>
#include <sstream>

#include <imgui.h>
#include <imgui_internal.h>

#include <common/logging.h>
#include <p2p/kademlia/routing.h>
#include <ui/fonts/material_design_icons.h>
#include <ui/style/theme.h>

namespace blocxxi {
namespace debug {
namespace ui {

void ShowRoutingInfo(char const* title,
                     p2p::kademlia::RoutingTable const& router,
                     bool* open = nullptr) {
  ImGui::SetNextWindowPos(ImVec2(600, 0), ImGuiCond_Once);

  ImGui::Begin(title, open);

  ImGui::BeginChild("left pane", ImVec2(300, 0), true);
  {
    {
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {5, 20});
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
    }
    {
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {5.0f, 20.0f});
      ImGui::Separator();
      ImGui::PopStyleVar();

      Font font(Font::FAMILY_PROPORTIONAL);
      font.LargeSize().Regular().Normal();
      ImGui::PushFont(font.ImGuiFont());

	  static auto selected_bucket = -1;
	  static auto selected_node = -1;

      if (ImGui::TreeNode(
              ("Buckets (" + std::to_string(router.BucketsCount()) + ")")
                  .c_str())) {
        auto index = 0;
        for (auto const& bucket : router) {
          std::ostringstream ostr;
          ostr << "- Bucket " << index << " : depth " << bucket.Depth()
               << " size " << bucket.Size().first;
          font.MediumSize().Regular().Normal();
          ImGui::PushFont(font.ImGuiFont());
          if (ImGui::Selectable(ostr.str().c_str(), selected_bucket == index)) {
            selected_bucket = index;
			selected_node = -1;
          }
          ImGui::PopFont();
          ++index;
        }
        ImGui::TreePop();
      }

      if (ImGui::TreeNode(
              ("Nodes (" + std::to_string(router.NodesCount()) + ")")
                  .c_str())) {
        auto index = 0;
        for (auto const& bucket : router) {
          for (auto const& node : bucket) {
            std::ostringstream ostr;
            ostr << "- " << node.Id().ToBitStringShort(20);
            font.MediumSize().Regular().Normal();
            ImGui::PushFont(font.ImGuiFont());
            if (ImGui::Selectable(ostr.str().c_str(), selected_node == index)) {
              selected_node = index;
			  selected_bucket = -1;
			}
            ImGui::PopFont();
            ++index;
          }
        }
        ImGui::TreePop();
      }
      ImGui::PopFont();
    }
  }
  ImGui::EndChild();
  ImGui::SameLine();

  ImGui::BeginChild("right pane", ImGui::GetContentRegionAvail(), true);
  {
    ImGui::TextWrapped(
        "This will contain the details...");
  }
  ImGui::EndChild();

  ImGui::End();
}

}  // namespace ui
}  // namespace debug
}  // namespace blocxxi
