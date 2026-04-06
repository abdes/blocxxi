//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Nova/Base/Compilers.h>
#include <Nova/Base/Logging.h>

NOVA_DIAGNOSTIC_PUSH
#if NOVA_GNUC_VERSION
#  pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#  pragma GCC diagnostic ignored "-Woverloaded-virtual"
#  pragma GCC diagnostic ignored "-Wsign-conversion"
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wredundant-decls"
#endif
#if NOVA_CLANG_VERSION
#  pragma clang diagnostic ignored "-Wcovered-switch-default"
#  pragma clang diagnostic ignored "-Wdeprecated"
#  pragma clang diagnostic ignored "-Wdocumentation"
#  pragma clang diagnostic ignored "-Wexit-time-destructors"
#  pragma clang diagnostic ignored "-Wglobal-constructors"
#  pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#  pragma clang diagnostic ignored "-Wmissing-noreturn"
#  pragma clang diagnostic ignored "-Wshadow"
#  pragma clang diagnostic ignored "-Wshorten-64-to-32"
#  pragma clang diagnostic ignored "-Wsuggest-destructor-override"
#  pragma clang diagnostic ignored "-Wsuggest-override"
#  pragma clang diagnostic ignored "-Wswitch-enum"
#  pragma clang diagnostic ignored "-Wundef"
#  pragma clang diagnostic ignored "-Wweak-vtables"
#  pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif
#if NOVA_CLANG_VERSION && NOVA_HAS_WARNING("-Wreserved-macro-identifier")
#  pragma clang diagnostic ignored "-Wreserved-macro-identifier"
#endif
#if NOVA_GNUC_VERSION
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
#include <asio.hpp>
NOVA_DIAGNOSTIC_POP
