//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <common/platform.h>

#if defined __GNUC__
#define ASAP_FUNCTION __PRETTY_FUNCTION__
#else
#define ASAP_FUNCTION __FUNCTION__
#endif


#ifndef ASAP_USE_ASSERTS
#define ASAP_USE_ASSERTS 1
#endif  // ASAP_USE_ASSERTS


#ifndef ASAP_USE_EXECINFO
#define ASAP_USE_EXECINFO 0
#endif
