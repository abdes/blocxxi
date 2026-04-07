//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#if defined(_WIN32) || defined(_WIN64)
#  ifdef BLOCXXI_NODE_STATIC
#    define BLOCXXI_NODE_API
#  else
#    ifdef BLOCXXI_NODE_EXPORTS
#      define BLOCXXI_NODE_API __declspec(dllexport)
#    else
#      define BLOCXXI_NODE_API __declspec(dllimport)
#    endif
#  endif
#elif defined(__APPLE__) || defined(__linux__)
#  ifdef BLOCXXI_NODE_EXPORTS
#    define BLOCXXI_NODE_API __attribute__((visibility("default")))
#  else
#    define BLOCXXI_NODE_API
#  endif
#else
#  define BLOCXXI_NODE_API
#endif

#define BLOCXXI_NODE_NDAPI [[nodiscard]] BLOCXXI_NODE_API
