//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Nova/Base/NoStd.h>

namespace nova {

template <typename T> struct Point {
  T x;
  T y;

  friend auto to_string(Point const& self)
  {
    std::string out = "x: ";
    out.append(nostd::to_string(self.x));
    out.append(", y: ");
    out.append(nostd::to_string(self.y));
    return out;
  }
};

template <typename T> struct Extent {
  T width;
  T height;

  friend auto to_string(Extent const& self)
  {
    std::string out = "w: ";
    out.append(nostd::to_string(self.width));
    out.append(", h: ");
    out.append(nostd::to_string(self.height));
    return out;
  }
};

template <typename T> struct Bounds {
  // Upper left corner
  Point<T> origin;
  // Width and height
  Extent<T> extent;

  friend auto to_string(Bounds const& self)
  {
    std::string out = "x: ";
    out.append(nostd::to_string(self.origin.x));
    out.append(", y: ");
    out.append(nostd::to_string(self.origin.y));
    out.append(", w: ");
    out.append(nostd::to_string(self.extent.width));
    out.append(", h: ");
    out.append(nostd::to_string(self.extent.height));
    return out;
  }
};

template <typename T> struct Motion {
  T dx;
  T dy;

  friend auto to_string(Motion const& self)
  {
    std::string out = "dx: ";
    out.append(nostd::to_string(self.dx));
    out.append(", dy: ");
    out.append(nostd::to_string(self.dy));
    return out;
  }
};

using PixelPosition = Point<int>;
using SubPixelPosition = Point<float>;
using PixelExtent = Extent<int>;
using SubPixelExtent = Extent<float>;
using PixelBounds = Bounds<int>;
using SubPixelBounds = Bounds<float>;
using PixelMotion = Motion<int>;
using SubPixelMotion = Motion<float>;

struct Axis1D {
  float x;

  friend auto to_string(Axis1D const& self)
  {
    std::string out = "x: ";
    out.append(nostd::to_string(self.x));
    return out;
  }
};
struct Axis2D {
  float x;
  float y;

  friend auto to_string(Axis2D const& self)
  {
    std::string out = "x: ";
    out.append(nostd::to_string(self.x));
    out.append(", y: ");
    out.append(nostd::to_string(self.y));
    return out;
  }
};

} // namespace nova
