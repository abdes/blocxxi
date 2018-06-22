//    Copyright The Blocxxi Project Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <array>

#include <imgui.h>

#include <common/logging.h>
#include <p2p/kademlia/routing.h>
#include <ui/style/theme.h>

namespace blocxxi {
namespace debug {
namespace ui {

void ShowRoutingInfo(char const* title,
                     p2p::kademlia::RoutingTable const& router,
                     bool* open = nullptr) {
  ImGui::SetNextWindowPos(ImVec2(300, 0), ImGuiCond_Once);

  ImGui::Begin(title, open);

  Font font("Roboto");
  font.Light().Italic().LargerSize();
  ImGui::PushFont(font.ImGuiFont());
  ImVec4 vec{0.3f, 0.6f, 0.8f, 1.0f};
  ImGui::TextColored(vec, "Routing Information");
  ImGui::PopFont();
  ImGui::TextUnformatted(router.ThisNode().ToString().c_str());

  ImGui::End();
}

}  // namespace ui
}  // namespace debug
}  // namespace blocxxi
