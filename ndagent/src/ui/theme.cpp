//    Copyright The Blocxxi Project Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <array>
#include <map>
#include <mutex>  // for call_once()

#include <imgui.h>

#include <ui/theme.h>
#include <fonts/fonts.h>

namespace blocxxi {
namespace ndagent {
namespace ui {

std::map<std::string, ImFont *> Theme::fonts_;

namespace {

std::string BuildFontName(char const *family, Font::Weight weight,
                          Font::Style style, Font::Size size) {
  std::string name(family);
  name.append(" ").append(Font::WeightString(weight));
  if (style == Font::Style::ITALIC)
    name.append(" ").append(Font::StyleString(style));
  name.append(" ").append(Font::SizeString(size));
  return name;
}

ImFont *LoadRobotoFont(std::string const &name, Font::Weight weight,
                       Font::Style style, Font::Size size) {
  ImGuiIO &io = ImGui::GetIO();
  ImFontConfig fontConfig;
  fontConfig.MergeMode = false;
  strncpy(fontConfig.Name, name.c_str(), 40);
  ImFont *font = nullptr;
  switch (weight) {
    case Font::Weight::LIGHTER:
      switch (style) {
        case Font::Style::ITALIC:
          font = io.Fonts->AddFontFromMemoryCompressedTTF(
              blocxxi::ndagent::Fonts::ROBOTO_THINITALIC_COMPRESSED_DATA,
              blocxxi::ndagent::Fonts::ROBOTO_THINITALIC_COMPRESSED_SIZE,
              Font::SizeFloat(size), &fontConfig);
          break;
        case Font::Style::NORMAL:
          font = io.Fonts->AddFontFromMemoryCompressedTTF(
              blocxxi::ndagent::Fonts::ROBOTO_THIN_COMPRESSED_DATA,
              blocxxi::ndagent::Fonts::ROBOTO_THIN_COMPRESSED_SIZE,
              Font::SizeFloat(size), &fontConfig);
          break;
      }
      break;
    case Font::Weight::LIGHT:
      switch (style) {
        case Font::Style::ITALIC:
          font = io.Fonts->AddFontFromMemoryCompressedTTF(
              blocxxi::ndagent::Fonts::ROBOTO_LIGHTITALIC_COMPRESSED_DATA,
              blocxxi::ndagent::Fonts::ROBOTO_LIGHTITALIC_COMPRESSED_SIZE,
              Font::SizeFloat(size), &fontConfig);
          break;
        case Font::Style::NORMAL:
          font = io.Fonts->AddFontFromMemoryCompressedTTF(
              blocxxi::ndagent::Fonts::ROBOTO_LIGHT_COMPRESSED_DATA,
              blocxxi::ndagent::Fonts::ROBOTO_LIGHT_COMPRESSED_SIZE,
              Font::SizeFloat(size), &fontConfig);
          break;
      }
      break;
    case Font::Weight::REGULAR:
      switch (style) {
        case Font::Style::ITALIC:
          font = io.Fonts->AddFontFromMemoryCompressedTTF(
              blocxxi::ndagent::Fonts::ROBOTO_ITALIC_COMPRESSED_DATA,
              blocxxi::ndagent::Fonts::ROBOTO_ITALIC_COMPRESSED_SIZE,
              Font::SizeFloat(size), &fontConfig);
          break;
        case Font::Style::NORMAL:
          font = io.Fonts->AddFontFromMemoryCompressedTTF(
              blocxxi::ndagent::Fonts::ROBOTO_REGULAR_COMPRESSED_DATA,
              blocxxi::ndagent::Fonts::ROBOTO_REGULAR_COMPRESSED_SIZE,
              Font::SizeFloat(size), &fontConfig);
          break;
      }
      break;
    case Font::Weight::MEDIUM:
      switch (style) {
        case Font::Style::ITALIC:
          font = io.Fonts->AddFontFromMemoryCompressedTTF(
              blocxxi::ndagent::Fonts::ROBOTO_MEDIUMITALIC_COMPRESSED_DATA,
              blocxxi::ndagent::Fonts::ROBOTO_MEDIUMITALIC_COMPRESSED_SIZE,
              Font::SizeFloat(size), &fontConfig);
          break;
        case Font::Style::NORMAL:
          font = io.Fonts->AddFontFromMemoryCompressedTTF(
              blocxxi::ndagent::Fonts::ROBOTO_MEDIUM_COMPRESSED_DATA,
              blocxxi::ndagent::Fonts::ROBOTO_MEDIUM_COMPRESSED_SIZE,
              Font::SizeFloat(size), &fontConfig);
          break;
      }
      break;
    case Font::Weight::BOLD:
      switch (style) {
        case Font::Style::ITALIC:
          font = io.Fonts->AddFontFromMemoryCompressedTTF(
              blocxxi::ndagent::Fonts::ROBOTO_BOLDITALIC_COMPRESSED_DATA,
              blocxxi::ndagent::Fonts::ROBOTO_BOLDITALIC_COMPRESSED_SIZE,
              Font::SizeFloat(size), &fontConfig);
          break;
        case Font::Style::NORMAL:
          font = io.Fonts->AddFontFromMemoryCompressedTTF(
              blocxxi::ndagent::Fonts::ROBOTO_BOLD_COMPRESSED_DATA,
              blocxxi::ndagent::Fonts::ROBOTO_BOLD_COMPRESSED_SIZE,
              Font::SizeFloat(size), &fontConfig);
          break;
      }
      break;
    case Font::Weight::BOLDER:
      switch (style) {
        case Font::Style::ITALIC:
          font = io.Fonts->AddFontFromMemoryCompressedTTF(
              blocxxi::ndagent::Fonts::ROBOTO_BLACKITALIC_COMPRESSED_DATA,
              blocxxi::ndagent::Fonts::ROBOTO_BLACKITALIC_COMPRESSED_SIZE,
              Font::SizeFloat(size), &fontConfig);
          break;
        case Font::Style::NORMAL:
          font = io.Fonts->AddFontFromMemoryCompressedTTF(
              blocxxi::ndagent::Fonts::ROBOTO_BLACK_COMPRESSED_DATA,
              blocxxi::ndagent::Fonts::ROBOTO_BLACK_COMPRESSED_SIZE,
              Font::SizeFloat(size), &fontConfig);
          break;
      }
      break;
  }
  return font;
}

}  // namespace

Font::Font(std::string family) : family_(std::move(family)) { InitFont(); }

void Font::InitFont() {
  BuildName();
  font_ = Theme::GetFont(name_);
  name_.assign(font_->GetDebugName());
}

Font::Font(Font const &other)
    : font_(other.font_),
      size_(other.size_),
      style_(other.style_),
      weight_(other.weight_),
      name_(other.name_) {}

Font &Font::operator=(Font const &rhs) {
  font_ = rhs.font_;
  size_ = rhs.size_;
  style_ = rhs.style_;
  weight_ = rhs.weight_;
  name_ = rhs.name_;
  return *this;
}
Font::Font(Font &&moved)
    : font_(moved.font_),
      size_(moved.size_),
      style_(moved.style_),
      weight_(moved.weight_),
      name_(std::move(moved.name_)) {
  moved.font_ = nullptr;
}
Font &Font::operator=(Font &&moved) {
  auto tmp = std::move(moved);
  swap(tmp);
  return *this;
}
void Font::swap(Font &other) {
  std::swap(font_, other.font_);
  std::swap(size_, other.size_);
  std::swap(style_, other.style_);
  std::swap(weight_, other.weight_);
  name_.swap(other.name_);
}
Font::~Font() {}

void Font::BuildName() {
  name_ = BuildFontName(family_.c_str(), weight_, style_, size_);
}

Font &Font::SmallSize() {
  size_ = Size::SMALL;
  InitFont();
  return *this;
}
Font &Font::MediumSize() {
  size_ = Size::MEDIUM;
  InitFont();
  return *this;
}
Font &Font::LargeSize() {
  size_ = Size::LARGE;
  InitFont();
  return *this;
}
Font &Font::LargerSize() {
  size_ = Size::LARGER;
  InitFont();
  return *this;
}

Font &Font::Italic() {
  style_ = Style::ITALIC;
  InitFont();
  return *this;
}

Font &Font::Lighter() {
  weight_ = Weight::LIGHTER;
  InitFont();
  return *this;
}
Font &Font::Light() {
  weight_ = Weight::LIGHT;
  InitFont();
  return *this;
}
Font &Font::Regular() {
  weight_ = Weight::REGULAR;
  InitFont();
  return *this;
}
Font &Font::Medium() {
  weight_ = Weight::MEDIUM;
  InitFont();
  return *this;
}
Font &Font::Bold() {
  weight_ = Weight::BOLD;
  InitFont();
  return *this;
}
Font &Font::Bolder() {
  weight_ = Weight::BOLDER;
  InitFont();
  return *this;
}

float Font::SizeFloat(Font::Size size) {
  auto val = static_cast<typename std::underlying_type<Font::Size>::type>(size);
  return static_cast<float>(val);
}
char const *Font::SizeString(Font::Size size) {
  switch (size) {
    case Size::SMALL:return "10px";
    case Size::MEDIUM:return "13px";
    case Size::LARGE:return "16px";
    case Size::LARGER:return "24px";
  }
  // Only needed for compilers that complain about not all control paths
  // return a value.
  return "__NEVER__";
}

char const *Font::StyleString(Font::Style style) {
  switch (style) {
    case Style::NORMAL:return "Normal";
    case Style::ITALIC:return "Italic";
  }
  // Only needed for compilers that complain about not all control paths
  // return a value.
  return "__NEVER__";
}

char const *Font::WeightString(Font::Weight weight) {
  switch (weight) {
    case Weight::LIGHTER:return "Thin";
    case Weight::LIGHT:return "Light";
    case Weight::REGULAR:return "Regular";
    case Weight::MEDIUM:return "Medium";
    case Weight::BOLD:return "Bold";
    case Weight::BOLDER:return "Bolder";
  }
  // Only needed for compilers that complain about not all control paths
  // return a value.
  return "__NEVER__";
}

void Theme::Init() {
  //
  // Colors
  //
  LoadColors();

  //
  // Fonts
  //
  static std::once_flag init_flag;
  std::call_once(init_flag, []() {
    ImGui::GetIO().Fonts->AddFontDefault();
    LoadDefaultFonts();
    // TODO: Temporary default font - can be configured
    Font default_font("Roboto");
    default_font.Medium().Regular().MediumSize();
    ImGui::GetIO().FontDefault = default_font.ImGuiFont();
  });
}

void Theme::LoadDefaultFonts() {
  std::array<Font::Weight, 6> font_weights{
      {Font::Weight::LIGHTER, Font::Weight::LIGHT, Font::Weight::REGULAR,
       Font::Weight::MEDIUM, Font::Weight::BOLD, Font::Weight::BOLDER}};
  std::array<Font::Style, 2> font_styles{
      {Font::Style::NORMAL, Font::Style::ITALIC}};
  std::array<Font::Size, 4> font_sizes{{Font::Size::SMALL, Font::Size::MEDIUM,
                                        Font::Size::LARGE, Font::Size::LARGER}};
  for (auto weight : font_weights) {
    for (auto style : font_styles) {
      for (auto size : font_sizes) {
        auto name = BuildFontName("Roboto", weight, style, size);
        auto font = LoadRobotoFont(name, weight, style, size);
        if (font) AddFont(name, font);
      }
    }
  }
}

void Theme::LoadColors() {
  ///////////////////////////////////////////////////////////
  // Style setup for ImGui. Colors copied from Stylepicker //
  ///////////////////////////////////////////////////////////
  ImGuiStyle *style = &ImGui::GetStyle();

  style->WindowPadding = ImVec2(15, 15);
  style->WindowRounding = 5.0f;
  style->FramePadding = ImVec2(5, 5);
  style->FrameRounding = 4.0f;
  style->ItemSpacing = ImVec2(12, 8);
  style->ItemInnerSpacing = ImVec2(8, 6);
  style->IndentSpacing = 25.0f;
  style->ScrollbarSize = 15.0f;
  style->ScrollbarRounding = 9.0f;
  style->GrabMinSize = 5.0f;
  style->GrabRounding = 3.0f;

  style->Colors[ImGuiCol_Text] = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
  style->Colors[ImGuiCol_TextDisabled] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
  style->Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
  style->Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
  style->Colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
  style->Colors[ImGuiCol_Border] = ImVec4(0.80f, 0.80f, 0.83f, 0.88f);
  style->Colors[ImGuiCol_BorderShadow] = ImVec4(0.92f, 0.91f, 0.88f, 0.00f);
  style->Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
  style->Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
  style->Colors[ImGuiCol_FrameBgActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
  style->Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
  style->Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 0.98f, 0.95f, 0.75f);
  style->Colors[ImGuiCol_TitleBgActive] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
  style->Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
  style->Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
  style->Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
  style->Colors[ImGuiCol_ScrollbarGrabHovered] =
      ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
  style->Colors[ImGuiCol_ScrollbarGrabActive] =
      ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
  style->Colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
  style->Colors[ImGuiCol_SliderGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
  style->Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
  style->Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
  style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
  style->Colors[ImGuiCol_ButtonActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
  style->Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
  style->Colors[ImGuiCol_HeaderHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
  style->Colors[ImGuiCol_HeaderActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
  style->Colors[ImGuiCol_Column] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
  style->Colors[ImGuiCol_ColumnHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
  style->Colors[ImGuiCol_ColumnActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
  style->Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  style->Colors[ImGuiCol_ResizeGripHovered] =
      ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
  style->Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
  style->Colors[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
  style->Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
  style->Colors[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
  style->Colors[ImGuiCol_PlotHistogramHovered] =
      ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
  style->Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.25f, 1.00f, 0.00f, 0.43f);
  style->Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(1.00f, 0.98f, 0.95f, 0.73f);
}

ImFont *Theme::GetFont(std::string const &name) {
  auto font = fonts_.find(name);
  if (font == fonts_.end()) {
    return ImGui::GetFont();
  }
  return font->second;
}

void Theme::AddFont(std::string const &name, ImFont *font) {
  fonts_.insert({name, font});
}

}  // namespace ui
}  // namespace ndagent
}  // namespace blocxxi