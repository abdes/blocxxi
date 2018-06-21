//    Copyright The Blocxxi Project Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

namespace blocxxi {
namespace ndagent {
namespace ui {

class ThemeImpl {
 public:
  virtual void SetupStyleColors() = 0;
  virtual void RegularFant() = 0;

  virtual ~ThemeImpl(){};
};

}  // namespace ui
}  // namespace ndagent
}  // namespace blocxxi