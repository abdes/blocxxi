//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#if defined(_WIN32) || defined(_WIN64)
#  ifdef BLOCXXI_NAT_STATIC
#    define BLOCXXI_NAT_API
#  else
#    ifdef BLOCXXI_NAT_EXPORTS
#      define BLOCXXI_NAT_API __declspec(dllexport)
#    else
#      define BLOCXXI_NAT_API __declspec(dllimport)
#    endif
#  endif
#elif defined(__APPLE__) || defined(__linux__)
#  ifdef BLOCXXI_NAT_EXPORTS
#    define BLOCXXI_NAT_API __attribute__((visibility("default")))
#  else
#    define BLOCXXI_NAT_API
#  endif
#else
#  define BLOCXXI_NAT_API
#endif

#define BLOCXXI_NAT_NDAPI [[nodiscard]] BLOCXXI_NAT_API
