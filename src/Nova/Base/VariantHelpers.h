//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

namespace nova {

//! Overloads pattern, used to create a lambda that can handle multiple
//! types within a `variant`, in a type-safe manner.
/*!
 It elegantly combines multiple lambdas into a single visitor object, and
 inherits function call operators from each lambda. Each type is handled with
 type-specific code, and a 'catch-all' lambda provides helpful runtime errors
 for unexpected types.

 <b>Example usage:</b>
 ```cpp
 using BarrierDesc = std::variant<
    BufferBarrierDesc,
    TextureBarrierDesc,
    MemoryBarrierDesc
 >;

 auto GetStateAfter() -> ResourceStates {
    return std::visit(
        overloads {
            [](const BufferBarrierDesc& desc) { return desc.after; },
            [](const TextureBarrierDesc& desc) { return desc.after; },
            [](const MemoryBarrierDesc& desc) { return desc.after; },
            // Provide a catch-all for unexpected types at runtime, or omit it
            // to have a compile time error if a new type is added but still not
            // implemented.
            [](const auto&) -> ResourceStates {
                ABORT_F("Unexpected barrier descriptor type");
            },
        },
        descriptor_);
 }
 ```
*/
template <class... Ts> struct Overloads : Ts... {
  using Ts::operator()...;
};

} // namespace nova
