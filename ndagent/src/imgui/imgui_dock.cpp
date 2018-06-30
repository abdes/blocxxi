// Based on docking implementation in LumixEngine.
// https://github.com/nem0/LumixEngine

// The MIT License (MIT)
//
// Copyright (c) 2013-2017 Mikulas Florek
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <imgui.h>
#include <imgui_internal.h>

#include <imgui/imgui_dock.h>

namespace ImGui {

ImVec2 operator+(ImVec2 lhs, ImVec2 rhs) {
  return {lhs.x + rhs.x, lhs.y + rhs.y};
}
ImVec2 operator-(ImVec2 lhs, ImVec2 rhs) {
  return {lhs.x - rhs.x, lhs.y - rhs.y};
}
ImVec2 operator*(ImVec2 lhs, float rhs) { return {lhs.x * rhs, lhs.y * rhs}; }

struct DockContext {
  enum Slot_ {
    Slot_Left,
    Slot_Right,
    Slot_Top,
    Slot_Bottom,
    Slot_Tab,

    Slot_Float,
    Slot_None
  };

  enum EndAction_ {
    EndAction_None,
    EndAction_Panel,
    EndAction_End,
    EndAction_EndChild
  };

  enum Status_ { Status_Docked, Status_Float, Status_Dragged };

  struct Dock {
    Dock() = default;

    ~Dock() { MemFree(label); }

    ImVec2 getMinSize() const {
      if (!children[0]) return {16, 16 + GetTextLineHeightWithSpacing()};

      ImVec2 s0 = children[0]->getMinSize();
      ImVec2 s1 = children[1]->getMinSize();
      return isHorizontal() ? ImVec2(s0.x + s1.x, ImMax(s0.y, s1.y))
                            : ImVec2(ImMax(s0.x, s1.x), s0.y + s1.y);
    }

    bool isHorizontal() const {
      return children[0]->pos.x < children[1]->pos.x;
    }

    void setParent(Dock *dock) {
      parent = dock;
      for (Dock *tmp = prev_tab; tmp; tmp = tmp->prev_tab) tmp->parent = dock;
      for (Dock *tmp = next_tab; tmp; tmp = tmp->next_tab) tmp->parent = dock;
    }

    Dock &getSibling() {
      IM_ASSERT(parent);
      if (parent->children[0] == &getFirstTab()) return *parent->children[1];
      return *parent->children[0];
    }

    Dock &getFirstTab() {
      Dock *tmp = this;
      while (tmp->prev_tab) tmp = tmp->prev_tab;
      return *tmp;
    }

    void setActive() {
      active = true;
      for (Dock *tmp = prev_tab; tmp; tmp = tmp->prev_tab) tmp->active = false;
      for (Dock *tmp = next_tab; tmp; tmp = tmp->next_tab) tmp->active = false;
    }

    bool hasChildren() const { return children[0] != nullptr; }

    void setChildrenPosSize(const ImVec2 &_pos, const ImVec2 &_size) {
      ImVec2 s = children[0]->size;
      if (isHorizontal()) {
        s.y = _size.y;
        s.x = (float)int(_size.x * children[0]->size.x /
                         (children[0]->size.x + children[1]->size.x));
        if (s.x < children[0]->getMinSize().x) {
          s.x = children[0]->getMinSize().x;
        } else if (_size.x - s.x < children[1]->getMinSize().x) {
          s.x = _size.x - children[1]->getMinSize().x;
        }
        children[0]->setPosSize(_pos, s);

        s.x = _size.x - children[0]->size.x;
        ImVec2 p = _pos;
        p.x += children[0]->size.x;
        children[1]->setPosSize(p, s);
      } else {
        s.x = _size.x;
        s.y = (float)int(_size.y * children[0]->size.y /
                         (children[0]->size.y + children[1]->size.y));
        if (s.y < children[0]->getMinSize().y) {
          s.y = children[0]->getMinSize().y;
        } else if (_size.y - s.y < children[1]->getMinSize().y) {
          s.y = _size.y - children[1]->getMinSize().y;
        }
        children[0]->setPosSize(_pos, s);

        s.y = _size.y - children[0]->size.y;
        ImVec2 p = _pos;
        p.y += children[0]->size.y;
        children[1]->setPosSize(p, s);
      }
    }

    void setPosSize(const ImVec2 &_pos, const ImVec2 &_size) {
      size = _size;
      pos = _pos;
      for (Dock *tmp = prev_tab; tmp; tmp = tmp->prev_tab) {
        tmp->size = _size;
        tmp->pos = _pos;
      }
      for (Dock *tmp = next_tab; tmp; tmp = tmp->next_tab) {
        tmp->size = _size;
        tmp->pos = _pos;
      }

      if (!hasChildren()) return;
      setChildrenPosSize(_pos, _size);
    }

    char *label{nullptr};
    ImU32 id{0};
    Dock *next_tab{nullptr};
    Dock *prev_tab{nullptr};
    Dock *children[2]{nullptr};
    Dock *parent{nullptr};
    bool active{true};
    ImVec2 pos{0,0};
    ImVec2 size{-1, -1};
    Status_ status{Status_Float};
    char location[16]{0};
    bool opened{false};
    bool first{true};
    int last_frame{0};
  };

  ImVector<Dock *> m_docks;
  ImVec2 m_drag_offset;
  Dock *m_current{nullptr};
  int m_last_frame{0};
  EndAction_ m_end_action{EndAction_None};
  bool m_is_begin_open{false};

  ~DockContext() = default;

  Dock &getDock(const char *label, bool opened, const ImVec2 &default_size) {
    ImU32 id = ImHash(label, 0);
    for (auto dock_ptr : m_docks) {
      if (dock_ptr->id == id) return *dock_ptr;
    }

    auto *new_dock = (Dock *)MemAlloc(sizeof(Dock));
    IM_PLACEMENT_NEW(new_dock) Dock();
    m_docks.push_back(new_dock);
    new_dock->label = ImStrdup(label);
    IM_ASSERT(new_dock->label);
    new_dock->id = id;
    new_dock->setActive();
    new_dock->status = Status_Float;
    new_dock->pos = ImVec2(0, 0);
    new_dock->size.x =
        default_size.x < 0 ? GetIO().DisplaySize.x : default_size.x;
    new_dock->size.y =
        default_size.y < 0 ? GetIO().DisplaySize.y : default_size.y;
    new_dock->opened = opened;
    new_dock->first = true;
    new_dock->location[0] = 0;
    return *new_dock;
  }

  void putInBackground() {
    ImGuiWindow *win = GetCurrentWindow();
    ImGuiContext &g = *GImGui;
    if (g.Windows[0] == win) return;

    for (int i = 0; i < g.Windows.Size; i++) {
      if (g.Windows[i] == win) {
        for (int j = i - 1; j >= 0; --j) {
          g.Windows[j + 1] = g.Windows[j];
        }
        g.Windows[0] = win;
        break;
      }
    }
  }

  void splits() {
    if (GetFrameCount() == m_last_frame) return;
    m_last_frame = GetFrameCount();

    putInBackground();

    ImU32 color = GetColorU32(ImGuiCol_Button);
    ImU32 color_hovered = GetColorU32(ImGuiCol_ButtonHovered);
    ImDrawList *draw_list = GetWindowDrawList();
    ImGuiIO &io = GetIO();
    for (int i = 0; i < m_docks.size(); ++i) {
      Dock &dock = *m_docks[i];
      if (!dock.hasChildren()) continue;

      PushID(i);
      if (!IsMouseDown(0)) dock.status = Status_Docked;

      //ImVec2 size = dock.children[0]->size;
      ImVec2 dsize(0, 0);
      SetCursorScreenPos(dock.children[1]->pos);
      ImVec2 min_size0 = dock.children[0]->getMinSize();
      ImVec2 min_size1 = dock.children[1]->getMinSize();
      if (dock.isHorizontal()) {
        InvisibleButton("split", ImVec2(3, dock.size.y));
        if (dock.status == Status_Dragged) dsize.x = io.MouseDelta.x;
        dsize.x = -ImMin(-dsize.x, dock.children[0]->size.x - min_size0.x);
        dsize.x = ImMin(dsize.x, dock.children[1]->size.x - min_size1.x);
      } else {
        InvisibleButton("split", ImVec2(dock.size.x, 3));
        if (dock.status == Status_Dragged) dsize.y = io.MouseDelta.y;
        dsize.y = -ImMin(-dsize.y, dock.children[0]->size.y - min_size0.y);
        dsize.y = ImMin(dsize.y, dock.children[1]->size.y - min_size1.y);
      }
      ImVec2 new_size0 = dock.children[0]->size + dsize;
      ImVec2 new_size1 = dock.children[1]->size - dsize;
      ImVec2 new_pos1 = dock.children[1]->pos + dsize;
      dock.children[0]->setPosSize(dock.children[0]->pos, new_size0);
      dock.children[1]->setPosSize(new_pos1, new_size1);

      if (IsItemHovered() && IsMouseClicked(0)) {
        dock.status = Status_Dragged;
      }

      draw_list->AddRectFilled(GetItemRectMin(), GetItemRectMax(),
                               IsItemHovered() ? color_hovered : color);
      PopID();
    }
  }

  void beginPanel() {
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    Dock *root = getRootDock();
    if (root) {
      SetNextWindowPos(root->pos);
      SetNextWindowSize(root->size);
    } else {
      SetNextWindowPos(ImVec2(0, 0));
      SetNextWindowSize(GetIO().DisplaySize);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    Begin("###DockPanel", nullptr, flags);
    splits();
  }

  void endPanel() {
    End();
    ImGui::PopStyleVar();
  }

  Dock *getDockAtMousePos() const {
    for (auto dock_ptr : m_docks) {
      if (dock_ptr->hasChildren()) continue;
      if (dock_ptr->status != Status_Docked) continue;
      if (IsMouseHoveringRect(dock_ptr->pos, dock_ptr->pos + dock_ptr->size,
                              false)) {
        return dock_ptr;
      }
    }

    return nullptr;
  }

  static ImRect getDockedRect(const ImRect &rect, Slot_ dock_slot) {
    ImVec2 size = rect.GetSize();
    switch (dock_slot) {
      default:
        return rect;
      case Slot_Top:
        return {rect.Min, rect.Min + ImVec2(size.x, size.y * 0.5f)};
      case Slot_Right:
        return {rect.Min + ImVec2(size.x * 0.5f, 0), rect.Max};
      case Slot_Bottom:
        return {rect.Min + ImVec2(0, size.y * 0.5f), rect.Max};
      case Slot_Left:
        return {rect.Min, rect.Min + ImVec2(size.x * 0.5f, rect.GetSize().y)};
    }
  }

  static ImRect getSlotRect(ImRect parent_rect, Slot_ dock_slot) {
    ImVec2 size = parent_rect.Max - parent_rect.Min;
    ImVec2 center = parent_rect.Min + size * 0.5f;
    switch (dock_slot) {
      default:
        return {center - ImVec2(20, 20), center + ImVec2(20, 20)};
      case Slot_Top:
        return {center + ImVec2(-20, -50), center + ImVec2(20, -30)};
      case Slot_Right:
        return {center + ImVec2(30, -20), center + ImVec2(50, 20)};
      case Slot_Bottom:
        return {center + ImVec2(-20, +30), center + ImVec2(20, 50)};
      case Slot_Left:
        return {center + ImVec2(-50, -20), center + ImVec2(-30, 20)};
    }
  }

  static ImRect getSlotRectOnBorder(ImRect parent_rect, Slot_ dock_slot) {
    ImVec2 size = parent_rect.Max - parent_rect.Min;
    ImVec2 center = parent_rect.Min + size * 0.5f;
    switch (dock_slot) {
      case Slot_Top:
        return {ImVec2(center.x - 20, parent_rect.Min.y + 10),
                ImVec2(center.x + 20, parent_rect.Min.y + 30)};
      case Slot_Left:
        return {ImVec2(parent_rect.Min.x + 10, center.y - 20),
                ImVec2(parent_rect.Min.x + 30, center.y + 20)};
      case Slot_Bottom:
        return {ImVec2(center.x - 20, parent_rect.Max.y - 30),
                ImVec2(center.x + 20, parent_rect.Max.y - 10)};
      case Slot_Right:
        return {ImVec2(parent_rect.Max.x - 30, center.y - 20),
                ImVec2(parent_rect.Max.x - 10, center.y + 20)};
      case Slot_Tab:
      case Slot_Float:
      case Slot_None:
        // TODO: add unexpected value log message
        IM_ASSERT(false && "unexpected value of dock_slot");
        break;
    }
    return {};
  }

  Dock *getRootDock() {
    for (auto dock_ptr : m_docks) {
      if (!dock_ptr->parent &&
          (dock_ptr->status == Status_Docked || dock_ptr->children[0])) {
        return dock_ptr;
      }
    }
    return nullptr;
  }

  bool dockSlots(Dock &dock, Dock *dest_dock, const ImRect &rect,
                 bool on_border) {
    ImDrawList *canvas = GetWindowDrawList();
    ImU32 color = GetColorU32(ImGuiCol_Button);
    ImU32 color_hovered = GetColorU32(ImGuiCol_ButtonHovered);
    ImVec2 mouse_pos = GetIO().MousePos;
    for (int i = 0; i < (on_border ? 4 : 5); ++i) {
      ImRect r = on_border ? getSlotRectOnBorder(rect, (Slot_)i)
                           : getSlotRect(rect, (Slot_)i);
      bool hovered = r.Contains(mouse_pos);
      canvas->AddRectFilled(r.Min, r.Max, hovered ? color_hovered : color);
      if (!hovered) continue;

      if (!IsMouseDown(0)) {
        doDock(dock, dest_dock ? dest_dock : getRootDock(), (Slot_)i);
        return true;
      }
      ImRect docked_rect = getDockedRect(rect, (Slot_)i);
      canvas->AddRectFilled(docked_rect.Min, docked_rect.Max,
                            GetColorU32(ImGuiCol_Button));
    }
    return false;
  }

  void handleDrag(Dock &dock) {
    Dock *dest_dock = getDockAtMousePos();

    Begin("##Overlay", nullptr,
          ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoTitleBar |
              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
              ImGuiWindowFlags_NoSavedSettings |
              ImGuiWindowFlags_AlwaysAutoResize);
    ImDrawList *canvas = GetWindowDrawList();

    canvas->PushClipRectFullScreen();

    ImU32 docked_color = GetColorU32(ImGuiCol_FrameBg);
    docked_color = (docked_color & 0x00ffFFFFu) | 0x80000000u;
    dock.pos = GetIO().MousePos - m_drag_offset;
    if (dest_dock) {
      if (dockSlots(dock, dest_dock,
                    ImRect(dest_dock->pos, dest_dock->pos + dest_dock->size),
                    false)) {
        canvas->PopClipRect();
        End();
        return;
      }
    }
    if (dockSlots(dock, nullptr, ImRect(ImVec2(0, 0), GetIO().DisplaySize),
                  true)) {
      canvas->PopClipRect();
      End();
      return;
    }
    canvas->AddRectFilled(dock.pos, dock.pos + dock.size, docked_color);
    canvas->PopClipRect();

    if (!IsMouseDown(0)) {
      dock.status = Status_Float;
      dock.location[0] = 0;
      dock.setActive();
    }

    End();
  }

  void fillLocation(Dock &dock) {
    if (dock.status == Status_Float) return;
    char *c = dock.location;
    Dock *tmp = &dock;
    while (tmp->parent) {
      *c = getLocationCode(tmp);
      tmp = tmp->parent;
      ++c;
    }
    *c = 0;
  }

  void doUndock(Dock &dock) {
    if (dock.prev_tab)
      dock.prev_tab->setActive();
    else if (dock.next_tab)
      dock.next_tab->setActive();
    else
      dock.active = false;
    Dock *container = dock.parent;

    if (container) {
      Dock &sibling = dock.getSibling();
      if (container->children[0] == &dock) {
        container->children[0] = dock.next_tab;
      } else if (container->children[1] == &dock) {
        container->children[1] = dock.next_tab;
      }

      bool remove_container =
          !container->children[0] || !container->children[1];
      if (remove_container) {
        if (container->parent) {
          Dock *&child = container->parent->children[0] == container
                             ? container->parent->children[0]
                             : container->parent->children[1];
          child = &sibling;
          child->setPosSize(container->pos, container->size);
          child->setParent(container->parent);
        } else {
          if (container->children[0]) {
            container->children[0]->setParent(nullptr);
            container->children[0]->setPosSize(container->pos, container->size);
          }
          if (container->children[1]) {
            container->children[1]->setParent(nullptr);
            container->children[1]->setPosSize(container->pos, container->size);
          }
        }
        for (int i = 0; i < m_docks.size(); ++i) {
          if (m_docks[i] == container) {
            m_docks.erase(m_docks.begin() + i);
            break;
          }
        }
        container->~Dock();
        MemFree(container);
      }
    }
    if (dock.prev_tab) dock.prev_tab->next_tab = dock.next_tab;
    if (dock.next_tab) dock.next_tab->prev_tab = dock.prev_tab;
    dock.parent = nullptr;
    dock.prev_tab = dock.next_tab = nullptr;
  }

  void drawTabbarListButton(Dock &dock) {
    if (!dock.next_tab) return;

    ImDrawList *draw_list = GetWindowDrawList();
    if (InvisibleButton("list", ImVec2(16, 16))) {
      OpenPopup("tab_list_popup");
    }
    if (BeginPopup("tab_list_popup")) {
      Dock *tmp = &dock;
      while (tmp) {
        bool dummy = false;
        if (Selectable(tmp->label, &dummy)) {
          tmp->setActive();
        }
        tmp = tmp->next_tab;
      }
      EndPopup();
    }

    bool hovered = IsItemHovered();
    ImVec2 min = GetItemRectMin();
    ImVec2 max = GetItemRectMax();
    ImVec2 center = (min + max) * 0.5f;
    ImU32 text_color = GetColorU32(ImGuiCol_Text);
    ImU32 color_active = GetColorU32(ImGuiCol_FrameBgActive);
    draw_list->AddRectFilled(ImVec2(center.x - 4, min.y + 3),
                             ImVec2(center.x + 4, min.y + 5),
                             hovered ? color_active : text_color);
    draw_list->AddTriangleFilled(
        ImVec2(center.x - 4, min.y + 7), ImVec2(center.x + 4, min.y + 7),
        ImVec2(center.x, min.y + 12), hovered ? color_active : text_color);
  }

  bool tabbar(Dock &dock, bool close_button) {
    float tabbar_height = 2 * GetTextLineHeightWithSpacing();
    ImVec2 size(dock.size.x, tabbar_height);
    bool tab_closed = false;

    SetCursorScreenPos(dock.pos);
    char tmp[20];
    ImFormatString(tmp, IM_ARRAYSIZE(tmp), "tabs%d", (int)dock.id);

    if (BeginChild(tmp, size, true)) {
      Dock *dock_tab = &dock;

      ImDrawList *draw_list = GetWindowDrawList();
      ImU32 color = GetColorU32(ImGuiCol_FrameBg);
      ImU32 color_active = GetColorU32(ImGuiCol_FrameBgActive);
      ImU32 color_hovered = GetColorU32(ImGuiCol_FrameBgHovered);
      ImU32 button_hovered = GetColorU32(ImGuiCol_ButtonHovered);
      ImU32 text_color = GetColorU32(ImGuiCol_Text);
      float line_height = GetTextLineHeightWithSpacing();
      float tab_base{0.0f};

      drawTabbarListButton(dock);

      while (dock_tab) {
        SameLine(0, 15);

        const char *text_end = FindRenderedTextEnd(dock_tab->label);
        ImVec2 label_size(CalcTextSize(dock_tab->label, text_end).x,
                          line_height);
        if (InvisibleButton(dock_tab->label, label_size)) {
          dock_tab->setActive();
        }

        if (IsItemActive() && IsMouseDragging()) {
          m_drag_offset = GetMousePos() - dock_tab->pos;
          doUndock(*dock_tab);
          dock_tab->status = Status_Dragged;
        }

        if (dock_tab->active && close_button)
          label_size.x += 16 + GetStyle().ItemSpacing.x;

        bool hovered = IsItemHovered();
        ImVec2 pos = GetItemRectMin();
        tab_base = pos.y;
        draw_list->PathClear();
        draw_list->PathLineTo(pos + ImVec2(-15, label_size.y));
        draw_list->PathBezierCurveTo(pos + ImVec2(-10, label_size.y),
                                     pos + ImVec2(-5, 0), pos + ImVec2(0, 0),
                                     10);
        draw_list->PathLineTo(pos + ImVec2(label_size.x, 0));
        draw_list->PathBezierCurveTo(
            pos + ImVec2(label_size.x + 5, 0),
            pos + ImVec2(label_size.x + 10, label_size.y),
            pos + ImVec2(label_size.x + 15, label_size.y), 10);
        draw_list->PathFillConvex(
            hovered ? color_hovered
                    : (dock_tab->active ? color_active : color));
        draw_list->AddText(pos, text_color, dock_tab->label, text_end);

        if (dock_tab->active && close_button) {
          SameLine();
          tab_closed = InvisibleButton("close", ImVec2(16, 16));
          ImVec2 center = (GetItemRectMin() + GetItemRectMax()) * 0.5f;
          if (IsItemHovered()) {
            draw_list->AddRectFilled(center + ImVec2(-6.0f, -6.0f),
                                     center + ImVec2(7.0f, 7.0f),
                                     button_hovered);
          }
          draw_list->AddLine(center + ImVec2(-3.5f, -3.5f),
                             center + ImVec2(3.5f, 3.5f), text_color);
          draw_list->AddLine(center + ImVec2(3.5f, -3.5f),
                             center + ImVec2(-3.5f, 3.5f), text_color);
        }

        dock_tab = dock_tab->next_tab;
      }
      ImVec2 cp(dock.pos.x, tab_base + line_height);
      draw_list->AddLine(cp, cp + ImVec2(dock.size.x, 0), color);
    }
    EndChild();
    return tab_closed;
  }

  static void setDockPosSize(Dock &dest, Dock &dock, Slot_ dock_slot,
                             Dock &container) {
    IM_ASSERT(!dock.prev_tab && !dock.next_tab && !dock.children[0] &&
              !dock.children[1]);

    dest.pos = container.pos;
    dest.size = container.size;
    dock.pos = container.pos;
    dock.size = container.size;

    switch (dock_slot) {
      case Slot_Bottom:
        dest.size.y *= 0.5f;
        dock.size.y *= 0.5f;
        dock.pos.y += dest.size.y;
        break;
      case Slot_Right:
        dest.size.x *= 0.5f;
        dock.size.x *= 0.5f;
        dock.pos.x += dest.size.x;
        break;
      case Slot_Left:
        dest.size.x *= 0.5f;
        dock.size.x *= 0.5f;
        dest.pos.x += dock.size.x;
        break;
      case Slot_Top:
        dest.size.y *= 0.5f;
        dock.size.y *= 0.5f;
        dest.pos.y += dock.size.y;
        break;
      case Slot_Tab:
      case Slot_Float:
      case Slot_None:
        IM_ASSERT(false && "unexpected value of dock_slot");
        break;
    }
    dest.setPosSize(dest.pos, dest.size);

    if (container.children[1]->pos.x < container.children[0]->pos.x ||
        container.children[1]->pos.y < container.children[0]->pos.y) {
      Dock *tmp = container.children[0];
      container.children[0] = container.children[1];
      container.children[1] = tmp;
    }
  }

  void doDock(Dock &dock, Dock *dest, Slot_ dock_slot) {
    IM_ASSERT(!dock.parent);
    if (!dest) {
      dock.status = Status_Docked;
      dock.setPosSize(ImVec2(0, 0), GetIO().DisplaySize);
    } else if (dock_slot == Slot_Tab) {
      Dock *tmp = dest;
      while (tmp->next_tab) {
        tmp = tmp->next_tab;
      }

      tmp->next_tab = &dock;
      dock.prev_tab = tmp;
      dock.size = tmp->size;
      dock.pos = tmp->pos;
      dock.parent = dest->parent;
      dock.status = Status_Docked;
    } else if (dock_slot == Slot_None) {
      dock.status = Status_Float;
    } else {
      auto *container = (Dock *)MemAlloc(sizeof(Dock));
      IM_PLACEMENT_NEW(container) Dock();
      m_docks.push_back(container);
      container->children[0] = &dest->getFirstTab();
      container->children[1] = &dock;
      container->next_tab = nullptr;
      container->prev_tab = nullptr;
      container->parent = dest->parent;
      container->size = dest->size;
      container->pos = dest->pos;
      container->status = Status_Docked;
      container->label = ImStrdup("");

      if (!dest->parent) {
      } else if (&dest->getFirstTab() == dest->parent->children[0]) {
        dest->parent->children[0] = container;
      } else {
        dest->parent->children[1] = container;
      }

      dest->setParent(container);
      dock.parent = container;
      dock.status = Status_Docked;

      setDockPosSize(*dest, dock, dock_slot, *container);
    }
    dock.setActive();
  }

  void rootDock(const ImVec2 &pos, const ImVec2 &size) {
    Dock *root = getRootDock();
    if (!root) return;

    ImVec2 min_size = root->getMinSize();
    ImVec2 requested_size = size;
    root->setPosSize(pos, ImMax(min_size, requested_size));

    static bool is_first_call = true;
    if (!is_first_call) {
      for (auto it = m_docks.begin(); it != m_docks.end();) {
        auto &dock = *it;
        if (!dock->hasChildren() && dock != root &&
            (ImGui::GetFrameCount() - dock->last_frame) > 1) {
          doUndock(*dock);
          dock->~Dock();
          MemFree(dock);
          it = m_docks.erase(it);
        } else
          ++it;
      }
    }
    is_first_call = false;
  }

  void setDockActive() {
    IM_ASSERT(m_current);
    if (m_current) m_current->setActive();
  }

  static Slot_ getSlotFromLocationCode(char code) {
    switch (code) {
      case '1':
        return Slot_Left;
      case '2':
        return Slot_Top;
      case '3':
        return Slot_Bottom;
      default:
        return Slot_Right;
    }
  }

  static char getLocationCode(Dock *dock) {
    if (!dock) return '0';

    if (dock->parent->isHorizontal()) {
      if (dock->pos.x < dock->parent->children[0]->pos.x) return '1';
      if (dock->pos.x < dock->parent->children[1]->pos.x) return '1';
      return '0';
    } else {
      if (dock->pos.y < dock->parent->children[0]->pos.y) return '2';
      if (dock->pos.y < dock->parent->children[1]->pos.y) return '2';
      return '3';
    }
  }

  void tryDockToStoredLocation(Dock &dock) {
    if (dock.status == Status_Docked) return;
    if (dock.location[0] == 0) return;

    Dock *tmp = getRootDock();
    if (!tmp) return;

    Dock *prev = nullptr;
    char *c = dock.location + strlen(dock.location) - 1;
    while (c >= dock.location && tmp) {
      prev = tmp;
      tmp = *c == getLocationCode(tmp->children[0]) ? tmp->children[0]
                                                    : tmp->children[1];
      if (tmp) --c;
    }
    if (tmp && tmp->children[0]) tmp = tmp->parent;
    doDock(dock, tmp ? tmp : prev,
           tmp && !tmp->children[0] ? Slot_Tab : getSlotFromLocationCode(*c));
  }

  void cleanDocks() {
  restart:
    for (int i = 0, c = m_docks.size(); i < c; ++i) {
      Dock &dock = *m_docks[i];
      if (dock.last_frame == 0 && dock.status != Status_Float &&
          !dock.children[0]) {
        fillLocation(*m_docks[i]);
        doUndock(*m_docks[i]);
        m_docks[i]->status = Status_Float;
        goto restart;
      }
    }
  }

  bool begin(const char *label, bool *opened, ImGuiWindowFlags extra_flags,
             const ImVec2 &default_size) {
    IM_ASSERT(!m_is_begin_open);
    m_is_begin_open = true;
    Dock &dock = getDock(label, !opened || *opened, default_size);
    if (dock.last_frame != 0 && m_last_frame != ImGui::GetFrameCount()) {
      cleanDocks();
    }
    dock.last_frame = ImGui::GetFrameCount();
    if (!dock.opened && (!opened || *opened)) tryDockToStoredLocation(dock);
    if (strcmp(dock.label, label) != 0) {
      MemFree(dock.label);
      dock.label = ImStrdup(label);
    }

    m_end_action = EndAction_None;

    if (dock.first && opened) *opened = dock.opened;
    dock.first = false;
    if (opened && !*opened) {
      if (dock.status != Status_Float) {
        fillLocation(dock);
        doUndock(dock);
        dock.status = Status_Float;
      }
      dock.opened = false;
      return false;
    }
    dock.opened = true;

    m_end_action = EndAction_Panel;
    beginPanel();

    m_current = &dock;
    if (dock.status == Status_Dragged) handleDrag(dock);

    bool is_float = dock.status == Status_Float;

    if (is_float) {
      SetNextWindowPos(dock.pos);
      SetNextWindowSize(dock.size, ImGuiCond_FirstUseEver);
      bool ret =
          Begin(label, opened, ImGuiWindowFlags_NoCollapse | extra_flags);
      m_end_action = EndAction_End;
      dock.pos = GetWindowPos();
      dock.size = GetWindowSize();

      ImGuiContext &g = *GImGui;

      if (g.ActiveId == GetCurrentWindow()->MoveId && g.IO.MouseDown[0]) {
        m_drag_offset = GetMousePos() - dock.pos;
        doUndock(dock);
        dock.status = Status_Dragged;
      }
      return ret;
    }

    if (!dock.active && dock.status != Status_Dragged) return false;

    m_end_action = EndAction_EndChild;

    PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0, 0, 0, 0));
    float tabbar_height = GetTextLineHeightWithSpacing();
    if (tabbar(dock.getFirstTab(), opened != nullptr)) {
      fillLocation(dock);
      *opened = false;
    }
    ImVec2 pos = dock.pos;
    ImVec2 size = dock.size;
    pos.y += tabbar_height + GetStyle().WindowPadding.y;
    size.y -= tabbar_height + GetStyle().WindowPadding.y;

    SetCursorScreenPos(pos);
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | extra_flags;
    char tmp[256];
    strncpy(tmp, label, 247);
    tmp[247] = '\0';
    strncat(tmp,
           "_docked", 8);  // to avoid https://github.com/ocornut/imgui/issues/713
    bool ret = BeginChild(tmp, size, true, flags);
    PopStyleColor();
    PopStyleColor();
    return ret;
  }

  void end() {
    if (m_end_action == EndAction_End) {
      End();
    } else if (m_end_action == EndAction_EndChild) {
      PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
      PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0, 0, 0, 0));
      EndChild();
      PopStyleColor();
      PopStyleColor();
    }
    m_current = nullptr;
    if (m_end_action > EndAction_None) endPanel();
    m_is_begin_open = false;
  }

  int getDockIndex(Dock *dock) {
    if (!dock) return -1;

    for (int i = 0; i < m_docks.size(); ++i) {
      if (dock == m_docks[i]) return i;
    }

    IM_ASSERT(false);
    return -1;
  }

  Dock *getDockByIndex(int idx) { return idx < 0 ? nullptr : m_docks[idx]; }

  void save() {
    FILE *fp = fopen("imgui_dock.layout", "w");
    fprintf(fp, "docks %d\n\n", m_docks.size());
    for (int i = 0; i < m_docks.size(); ++i) {
      Dock &dock = *m_docks[i];

      fprintf(fp, "index    %d\n", i);
      fprintf(
          fp, "label    %s\n",
          dock.parent ? (dock.label[0] == '\0' ? "DOCK" : dock.label) : "ROOT"),
          fprintf(fp, "x        %d\n", (int)dock.pos.x);
      fprintf(fp, "y        %d\n", (int)dock.pos.y);
      fprintf(fp, "size_x   %d\n", (int)dock.size.x);
      fprintf(fp, "size_y   %d\n", (int)dock.size.y);
      fprintf(fp, "status   %d\n", (int)dock.status);
      fprintf(fp, "active   %d\n", dock.active ? 1 : 0);
      fprintf(fp, "opened   %d\n", dock.opened ? 1 : 0);
      fillLocation(dock);
      fprintf(fp, "location %s\n",
              strlen(dock.location) ? dock.location : "-1");
      fprintf(fp, "child0   %d\n", getDockIndex(dock.children[0]));
      fprintf(fp, "child1   %d\n", getDockIndex(dock.children[1]));
      fprintf(fp, "prev_tab %d\n", getDockIndex(dock.prev_tab));
      fprintf(fp, "next_tab %d\n", getDockIndex(dock.next_tab));
      fprintf(fp, "parent   %d\n\n", getDockIndex(dock.parent));
    }
    fclose(fp);
  }

  void load() {
    for (int i = 0; i < m_docks.size(); ++i) {
      m_docks[i]->~Dock();
      MemFree(m_docks[i]);
    }
    m_docks.clear();

    FILE *fp = fopen("imgui_dock.layout", "r");

    if (fp) {
      int ival;
      char str2[64];
      fscanf(fp, "docks %d", &ival);
      printf("%d docks\n", ival);

      for (int i = 0; i < ival; i++) {
        auto *new_dock = (Dock *)MemAlloc(sizeof(Dock));
        IM_PLACEMENT_NEW(new_dock) Dock();
        m_docks.push_back(new_dock);
      }

      for (int i = 0; i < ival; i++) {
        int id, id1, id2, id3, id4, id5;
        int st;
        int b1, b2;
        char lab[32];

        fscanf(fp, "%s %d", str2, &id);
        fscanf(fp, "%s %[^\n]s", str2, &lab[0]);
        fscanf(fp, "%s %f", str2, &m_docks[id]->pos.x);
        fscanf(fp, "%s %f", str2, &m_docks[id]->pos.y);
        fscanf(fp, "%s %f", str2, &m_docks[id]->size.x);
        fscanf(fp, "%s %f", str2, &m_docks[id]->size.y);
        fscanf(fp, "%s %d", str2, &st);
        fscanf(fp, "%s %d", str2, &b1);
        fscanf(fp, "%s %d", str2, &b2);
        fscanf(fp, "%s %s", str2, &m_docks[id]->location[0]);
        fscanf(fp, "%s %d", str2, &id1);
        fscanf(fp, "%s %d", str2, &id2);
        fscanf(fp, "%s %d", str2, &id3);
        fscanf(fp, "%s %d", str2, &id4);
        fscanf(fp, "%s %d", str2, &id5);

#ifdef _WIN32
        m_docks[id]->label = _strdup(lab);
#else
		m_docks[id]->label = strdup(lab);
#endif
		m_docks[id]->id = ImHash(m_docks[id]->label, 0);

        m_docks[id]->children[0] = getDockByIndex(id1);
        m_docks[id]->children[1] = getDockByIndex(id2);
        m_docks[id]->prev_tab = getDockByIndex(id3);
        m_docks[id]->next_tab = getDockByIndex(id4);
        m_docks[id]->parent = getDockByIndex(id5);
        m_docks[id]->status = (Status_)st;
        m_docks[id]->active = b1;
        m_docks[id]->opened = b2;

        tryDockToStoredLocation(*m_docks[id]);
      }

      fclose(fp);
    }
    printf("done\n");
    fflush(stdout);
  }
};

static DockContext g_dock;

void PrintDocks() {
  auto index = 0;
  for (auto *dock_ptr : g_dock.m_docks) {
    ImGui::Text(
        "i=%d label='%s' state=(%s %s) pos=(%.0f %.0f) size=(%.0f %.0f) "
        "children=(%s %s) tabs=(%s %s) parent=%s status=%d  location='%s'\n",
        index, dock_ptr->label, (dock_ptr->active) ? "active" : "inactive",
        (dock_ptr->opened) ? "open" : "closed", dock_ptr->pos.x,
        dock_ptr->pos.y, dock_ptr->size.x, dock_ptr->size.y,
        dock_ptr->children[0] ? dock_ptr->children[0]->label : "None",
        dock_ptr->children[1] ? dock_ptr->children[1]->label : "None",
        dock_ptr->prev_tab ? dock_ptr->prev_tab->label : "None",
        dock_ptr->next_tab ? dock_ptr->next_tab->label : "None",
        dock_ptr->parent ? dock_ptr->parent->label : "None", dock_ptr->status,
        dock_ptr->location);
    ++index;
  }
}

void ShutdownDock() {
  for (auto dock_ptr : g_dock.m_docks) {
    dock_ptr->~Dock();
    MemFree(dock_ptr);
  }
  g_dock.m_docks.clear();
}

void RootDock(const ImVec2 &pos, const ImVec2 &size) {
  g_dock.rootDock(pos, size);
}

void SetDockActive() { g_dock.setDockActive(); }

bool BeginDock(const char *label, bool *opened, ImGuiWindowFlags extra_flags,
               const ImVec2 &default_size) {
  return g_dock.begin(label, opened, extra_flags, default_size);
}

void EndDock() { g_dock.end(); }

void SaveDock() { g_dock.save(); }

void LoadDock() { g_dock.load(); }

}  // namespace ImGui