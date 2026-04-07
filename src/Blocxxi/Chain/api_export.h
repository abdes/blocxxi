//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#if defined(_WIN32) || defined(_WIN64)
#  ifdef BLOCXXI_CHAIN_STATIC
#    define BLOCXXI_CHAIN_API
#  else
#    ifdef BLOCXXI_CHAIN_EXPORTS
#      define BLOCXXI_CHAIN_API __declspec(dllexport)
#    else
#      define BLOCXXI_CHAIN_API __declspec(dllimport)
#    endif
#  endif
#elif defined(__APPLE__) || defined(__linux__)
#  ifdef BLOCXXI_CHAIN_EXPORTS
#    define BLOCXXI_CHAIN_API __attribute__((visibility("default")))
#  else
#    define BLOCXXI_CHAIN_API
#  endif
#else
#  define BLOCXXI_CHAIN_API
#endif

#define BLOCXXI_CHAIN_NDAPI [[nodiscard]] BLOCXXI_CHAIN_API
