//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#if defined(_WIN32) || defined(_WIN64)
#  ifdef NOVA_BASE_STATIC
#    define NOVA_BASE_API
#  else
#    ifdef NOVA_BASE_EXPORTS
#      define NOVA_BASE_API __declspec(dllexport)
#    else
#      define NOVA_BASE_API __declspec(dllimport)
#    endif
#  endif
#elif defined(__APPLE__) || defined(__linux__)
#  ifdef NOVA_BASE_EXPORTS
#    define NOVA_BASE_API __attribute__((visibility("default")))
#  else
#    define NOVA_BASE_API
#  endif
#else
#  define NOVA_BASE_API
#endif

#define NOVA_BASE_NDAPI [[nodiscard]] NOVA_BASE_API
