//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <common/config.h>

#if defined __GNUC__ || defined __clang__
#define BLOCXXI_FORMAT(fmt, ellipsis) \
  __attribute__((__format__(__printf__, fmt, ellipsis)))
#else
#define BLOCXXI_FORMAT(fmt, ellipsis)
#endif


#if BLOCXXI_USE_ASSERTS


#include <string>
namespace blocxxi {
void print_backtrace(char* out, int len, int max_depth = 0,
                     void* ctx = nullptr);
}  // namespace blocxxi
#endif

// This is to disable the warning of conditional expressions
// being constant in msvc
// clang-format off
#ifdef _MSC_VER
#define BLOCXXI_WHILE_0  \
	__pragma( warning(push) ) \
	__pragma( warning(disable:4127) ) \
	while (false) \
	__pragma( warning(pop) )
#else
#define BLOCXXI_WHILE_0 while (false)
#endif
// clang-format on

namespace blocxxi {
// declarations of the two functions

// internal
void assert_print(char const* fmt, ...) BLOCXXI_FORMAT(1, 2);

// internal
void assert_fail(const char* expr, int line, char const* file,
                 char const* function, char const* val, int kind = 0);

}  // namespace blocxxi

#if BLOCXXI_USE_ASSERTS

#ifdef BLOCXXI_PRODUCTION_ASSERTS
extern char const* blocxxi_assert_log;
#endif

#if BLOCXXI_USE_IOSTREAM
#include <sstream>
#endif

#ifndef BLOCXXI_USE_SYSTEM_ASSERTS

#define BLOCXXI_ASSERT_PRECOND(x)                                             \
  do {                                                                        \
    if (x) {                                                                  \
    } else                                                                    \
      blocxxi::assert_fail(#x, __LINE__, __FILE__, BLOCXXI_FUNCTION, nullptr, \
                           1);                                                \
  }                                                                           \
  BLOCXXI_WHILE_0

#define BLOCXXI_ASSERT(x)                                                     \
  do {                                                                        \
    if (x) {                                                                  \
    } else                                                                    \
      blocxxi::assert_fail(#x, __LINE__, __FILE__, BLOCXXI_FUNCTION, nullptr, \
                           0);                                                \
  }                                                                           \
  BLOCXXI_WHILE_0

#define BLOCXXI_ASSERT_VAL(x, y)                                     \
  do {                                                               \
    if (x) {                                                         \
    } else {                                                         \
      std::stringstream __s__;                                       \
      __s__ << #y ": " << y;                                         \
      blocxxi::assert_fail(#x, __LINE__, __FILE__, BLOCXXI_FUNCTION, \
                           __s__.str().c_str(), 0);                  \
    }                                                                \
  }                                                                  \
  BLOCXXI_WHILE_0

#define BLOCXXI_ASSERT_FAIL_VAL(y)                                  \
  do {                                                              \
    std::stringstream __s__;                                        \
    __s__ << #y ": " << y;                                          \
    blocxxi::assert_fail("<unconditional>", __LINE__, __FILE__,     \
                         BLOCXXI_FUNCTION, __s__.str().c_str(), 0); \
  }                                                                 \
  BLOCXXI_WHILE_0


#define BLOCXXI_ASSERT_FAIL()                                 \
  blocxxi::assert_fail("<unconditional>", __LINE__, __FILE__, \
                       BLOCXXI_FUNCTION, nullptr, 0)

#else
#include <cassert>
#define BLOCXXI_ASSERT_PRECOND(x) assert(x)
#define BLOCXXI_ASSERT(x) assert(x)
#define BLOCXXI_ASSERT_VAL(x, y) assert(x)
#define BLOCXXI_ASSERT_FAIL_VAL(x) assert(false)
#define BLOCXXI_ASSERT_FAIL() assert(false)
#endif

#else  // BLOCXXI_USE_ASSERTS

#define BLOCXXI_ASSERT_PRECOND(a) \
  do {                            \
  }                               \
  BLOCXXI_WHILE_0
#define BLOCXXI_ASSERT(a) \
  do {                    \
  }                       \
  BLOCXXI_WHILE_0
#define BLOCXXI_ASSERT_VAL(a, b) \
  do {                           \
  }                              \
  BLOCXXI_WHILE_0
#define BLOCXXI_ASSERT_FAIL_VAL(a) \
  do {                             \
  }                                \
  BLOCXXI_WHILE_0
#define BLOCXXI_ASSERT_FAIL() \
  do {                        \
  }                           \
  BLOCXXI_WHILE_0

#endif  // BLOCXXI_USE_ASSERTS
