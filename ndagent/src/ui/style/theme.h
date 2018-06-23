//    Copyright The Blocxxi Project Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <map>
#include <string>

struct ImFont;

namespace blocxxi {
namespace debug {
namespace ui {

class Font final {
 public:
  static std::string const FAMILY_MONOSPACE;
  static std::string const FAMILY_PROPORTIONAL;

  enum class Size {
    SMALL = 10,   // 10px
    MEDIUM = 13,  // 13px
    LARGE = 16,   // 16px
    LARGER = 24,  // 20px
  };
  static float SizeFloat(Font::Size size);
  static char const *SizeString(Font::Size size);

  enum class Style { NORMAL, ITALIC };
  static char const *StyleString(Font::Style style);

  enum class Weight { LIGHT, REGULAR, BOLD };
  static char const *WeightString(Font::Weight weight);

  Font(std::string family);
  Font(Font const &other);
  Font &operator=(Font const &rhs);
  Font(Font &&moved);
  Font &operator=(Font &&moved);
  void swap(Font &other);
  ~Font();

  std::string const &Name() const { return name_; }
  ImFont *ImGuiFont() { return font_; }

  Font &SmallSize();
  Font &MediumSize();
  Font &LargeSize();
  Font &LargerSize();

  Font &Italic();

  Font &Light();
  Font &Regular();
  Font &Bold();

 private:
  void BuildName();
  void InitFont();

  ImFont *font_{nullptr};
  std::string family_;
  Font::Size size_{Font::Size::MEDIUM};
  Font::Style style_{Font::Style::NORMAL};
  Font::Weight weight_{Font::Weight::REGULAR};
  mutable std::string name_{};
};

class Theme {
 public:
  static void Init();

  static ImFont *GetFont(std::string const &name);

  static ImFont *GetIconsFont() { return icons_font_normal_; }

 private:
  Theme() {}

  static void LoadDefaultFonts();
  static void LoadColors();

  static void AddFont(std::string const &name, ImFont *font);

  static std::map<std::string, ImFont *> fonts_;
  static ImFont *icons_font_normal_;
};

}  // namespace ui
}  // namespace debug
}  // namespace blocxxi
