//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#ifdef _MSC_VER
#  define NOVA_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#  define NOVA_NOINLINE __attribute__((noinline))
#else
#  define NOVA_NOINLINE
#endif
