//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

/*!
 * @file
 *
 * @brief Compiler detection, diagnostics and feature helpers.
 */

#pragma once

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

// ----------------------------------------------------------------------------
// Compiler detection
// ----------------------------------------------------------------------------

#ifndef DNOVA_DOCUMENTATION_BUILD

#  ifdef NOVA_VERSION_ENCODE_INTERNAL_
#    undef NOVA_VERSION_ENCODE_INTERNAL_
#  endif
#  define NOVA_VERSION_ENCODE_INTERNAL_(major, minor, revision)                \
    (((major) * 1000000) + ((minor) * 1000) + (revision))

#  ifdef NOVA_CLANG_VERSION
#    undef NOVA_CLANG_VERSION
#  endif
#  ifdef __clang__
#    define NOVA_CLANG_VERSION                                                 \
      NOVA_VERSION_ENCODE_INTERNAL_(                                           \
        __clang_major__, __clang_minor__, __clang_patchlevel__)
#  endif

#else // DNOVA_DOCUMENTATION_BUILD
/*!
 * @brief Clang compiler detection macro.
 *
 * This macro is only defined if the compiler in use is a Clang compiler
 * (including Apple Clang).
 *
 * Example
 * ```
 * #if defined(NOVA_CLANG_VERSION)
 * // Do things for clang
 * #endif
 * ```
 *
 * @see NOVA_CLANG_VERSION_CHECK
 */
#  define NOVA_CLANG_VERSION 0
#endif // DNOVA_DOCUMENTATION_BUILD

#ifdef NOVA_CLANG_VERSION_CHECK
#  undef NOVA_CLANG_VERSION_CHECK
#endif
#ifdef NOVA_CLANG_VERSION
#  define NOVA_CLANG_VERSION_CHECK(major, minor, patch)                        \
    (NOVA_CLANG_VERSION >= NOVA_VERSION_ENCODE_INTERNAL_(major, minor, patch))
#else
#  define NOVA_CLANG_VERSION_CHECK(major, minor, patch) (0)
#endif
/*!
 * @def NOVA_CLANG_VERSION_CHECK
 *
 * @brief Clang compiler version check macro.
 *
 * This macro is always defined, and provides a convenient way to check for
 * features based on the version number.
 *
 * @note In most cases for clang, you should not test its version number, you
 * should use the feature checking macros
 * (https://clang.llvm.org/docs/LanguageExtensions.html#feature-checking-macros).
 *
 * Example
 * ```
 * #if NOVA_CLANG_VERSION_CHECK(14,0,0)
 * // Do a thing that only clang-14+ supports
 * #endif
 * ```
 *
 * @return true (1) if the current compiler corresponds to the macro name, and
 * the compiler version is greater than or equal to the provided values.
 *
 * @see NOVA_CLANG_VERSION
 */

#ifndef DNOVA_DOCUMENTATION_BUILD

#  ifdef NOVA_MSVC_VERSION
#    undef NOVA_MSVC_VERSION
#  endif
#  if defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 140000000) && !defined(__ICL)
#    define NOVA_MSVC_VERSION                                                  \
      NOVA_VERSION_ENCODE_INTERNAL_(_MSC_FULL_VER / 10000000,                  \
        (_MSC_FULL_VER % 10000000) / 100000, (_MSC_FULL_VER % 100000) / 100)
#  elif defined(_MSC_FULL_VER) && !defined(__ICL)
#    define NOVA_MSVC_VERSION                                                  \
      NOVA_VERSION_ENCODE_INTERNAL_(_MSC_FULL_VER / 1000000,                   \
        (_MSC_FULL_VER % 1000000) / 10000, (_MSC_FULL_VER % 10000) / 10)
#  elif defined(_MSC_VER) && !defined(__ICL)
#    define NOVA_MSVC_VERSION                                                  \
      NOVA_VERSION_ENCODE_INTERNAL_(_MSC_VER / 100, _MSC_VER % 100, 0)
#  endif

#else // DNOVA_DOCUMENTATION_BUILD
/*!
 * @brief MSVC compiler detection macro.
 *
 * This macro is only defined if the compiler in use is Microsoft Visual Studio
 * C++ compiler.
 *
 * Example
 * ```
 * #if defined(NOVA_MSVC_VERSION)
 * // Do things for MSVC
 * #endif
 * ```
 *
 * @see NOVA_MSVC_VERSION_CHECK
 */
#  define NOVA_MSVC_VERSION 0
#endif // DNOVA_DOCUMENTATION_BUILD

#ifdef NOVA_MSVC_VERSION_CHECK
#  undef NOVA_MSVC_VERSION_CHECK
#endif
#ifndef NOVA_MSVC_VERSION
#  define NOVA_MSVC_VERSION_CHECK(major, minor, patch) (0)
#elif defined(_MSC_VER) && (_MSC_VER >= 1400)
#  define NOVA_MSVC_VERSION_CHECK(major, minor, patch)                         \
    (_MSC_FULL_VER >= (((major) * 10000000) + ((minor) * 100000) + (patch)))
#elif defined(_MSC_VER) && (_MSC_VER >= 1200)
#  define NOVA_MSVC_VERSION_CHECK(major, minor, patch)                         \
    (_MSC_FULL_VER >= ((major * 1000000) + (minor * 10000) + (patch)))
#else
#  define NOVA_MSVC_VERSION_CHECK(major, minor, patch)                         \
    (_MSC_VER >= ((major * 100) + (minor)))
#endif
/*!
 * @def NOVA_MSVC_VERSION_CHECK
 * @brief MSVC compiler version check macro.
 *
 * This macro is always defined, and provides a convenient way to check for
 * features based on the version number.
 *
 * Example
 * ```
 * #if NOVA_MSVC_VERSION_CHECK(16,0,0)
 * // Do a thing that only MSVC 16+ supports
 * #endif
 * ```
 *
 * @return true (1) if the current compiler corresponds to the macro name, and
 * the compiler version is greater than or equal to the provided values.
 *
 * @see NOVA_MSVC_VERSION
 */

#ifndef DNOVA_DOCUMENTATION_BUILD

/*
 * Note that the GNUC and GCC macros are different. Many compilers masquerade as
 * GCC (by defining * __GNUC__, __GNUC_MINOR__, and __GNUC_PATCHLEVEL__), but
 * often do not implement all the features of the version of GCC they pretend to
 * support.
 *
 * To work around this, the NOVA_GCC_VERSION macro is only defined for GCC,
 * whereas NOVA_GNUC_VERSION will be defined whenever a compiler defines
 * __GCC__.
 */

#  ifdef NOVA_GNUC_VERSION
#    undef NOVA_GNUC_VERSION
#  endif
#  if defined(__GNUC__) && defined(__GNUC_PATCHLEVEL__)
#    define NOVA_GNUC_VERSION                                                  \
      NOVA_VERSION_ENCODE_INTERNAL_(                                           \
        __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
#  elif defined(__GNUC__)
#    define NOVA_GNUC_VERSION                                                  \
      NOVA_VERSION_ENCODE_INTERNAL_(__GNUC__, __GNUC_MINOR__, 0)
#  endif

#else // DNOVA_DOCUMENTATION_BUILD
/*!
 * @brief GNU like compiler detection macro.
 *
 * This macro is only defined if the compiler in use defines `__GNUC__`, which
 * may indicate that it is the real GCC compiler or a compiler masquerading GCC.
 *
 * Example
 * ```
 * #if defined(NOVA_GNUC_VERSION)
 * // Do things that work for GNU like compilers
 * #endif
 * ```
 *
 * @see NOVA_GNUC_VERSION_CHECK
 */
#  define NOVA_GNUC_VERSION 0
#endif // DNOVA_DOCUMENTATION_BUILD

#ifdef NOVA_GNUC_VERSION_CHECK
#  undef NOVA_GNUC_VERSION_CHECK
#endif
#ifdef NOVA_GNUC_VERSION
#  define NOVA_GNUC_VERSION_CHECK(major, minor, patch)                         \
    (NOVA_GNUC_VERSION >= NOVA_VERSION_ENCODE_INTERNAL_(major, minor, patch))
#else
#  define NOVA_GNUC_VERSION_CHECK(major, minor, patch) (0)
#endif
/*!
 * @def NOVA_GNUC_VERSION_CHECK
 * @brief GNU like compiler version check macro.
 *
 * This macro is always defined, and provides a convenient way to check for
 * features based on the version number.
 *
 * Example
 * ```
 * #if NOVA_GNUC_VERSION_CHECK(9,0,0)
 * // Do a thing that only GNU-like compiler 9+ supports
 * #endif
 * ```
 *
 * @return true (1) if the current compiler corresponds to the macro name, and
 * the compiler version is greater than or equal to the provided values.
 *
 * @see NOVA_GNUC_VERSION
 */

#ifndef DNOVA_DOCUMENTATION_BUILD

#  ifdef NOVA_GCC_VERSION
#    undef NOVA_GCC_VERSION
#  endif
#  if defined(NOVA_GNUC_VERSION) && !defined(__clang__)
#    define NOVA_GCC_VERSION NOVA_GNUC_VERSION
#  endif

#else // DNOVA_DOCUMENTATION_BUILD
/*!
 * @brief GCC compiler detection macro.
 *
 * This macro is only defined if the compiler in use is GNU C/C++ compiler. Any
 * other compilers that masquerade as `__GNUC__` but define another known
 * compiler symbol are excluded.
 *
 * Example
 * ```
 * #if defined(NOVA_GCC_VERSION)
 * // Do things for GCC/G++
 * #endif
 * ```
 *
 * @see NOVA_GCC_VERSION_CHECK
 */
#  define NOVA_GCC_VERSION 0
#endif // DNOVA_DOCUMENTATION_BUILD

#ifdef NOVA_GCC_VERSION_CHECK
#  undef NOVA_GCC_VERSION_CHECK
#endif
#ifdef NOVA_GCC_VERSION
#  define NOVA_GCC_VERSION_CHECK(major, minor, patch)                          \
    (NOVA_GCC_VERSION >= NOVA_VERSION_ENCODE_INTERNAL_(major, minor, patch))
#else
#  define NOVA_GCC_VERSION_CHECK(major, minor, patch) (0)
#endif
/*!
 * @def NOVA_GCC_VERSION_CHECK
 * @brief GCC/G++ compiler version check macro.
 *
 * This macro is always defined, and provides a convenient way to check for
 * features based on the version number.
 *
 * Example
 * ```
 * #if NOVA_GCC_VERSION_CHECK(11,0,0)
 * // Do a thing that only GCC 11+ supports
 * #endif
 * ```
 *
 * @return true (1) if the current compiler corresponds to the macro name, and
 * the compiler version is greater than or equal to the provided values.
 *
 * @see NOVA_GCC_VERSION
 */

// -----------------------------------------------------------------------------
// Compiler attribute check
// -----------------------------------------------------------------------------

#ifdef NOVA_HAS_ATTRIBUTE
#  undef NOVA_HAS_ATTRIBUTE
#endif
#ifdef __has_attribute
#  define NOVA_HAS_ATTRIBUTE(attribute) __has_attribute(attribute)
#else
#  define NOVA_HAS_ATTRIBUTE(attribute) (0)
#endif
/*!
 * @def NOVA_HAS_ATTRIBUTE
 * @brief Checks for the presence of an attribute named by `attribute`.
 *
 * Example
 * ```
 * #if NOVA_HAS_ATTRIBUTE(deprecated) // Check for an attribute
 * #  define DEPRECATED(msg) [[deprecated(msg)]]
 * #else
 * #  define DEPRECATED(msg)
 * #endif
 *
 * DEPRECATED("foo() has been deprecated") void foo();
 * ```
 *
 * @return non-zero value if the attribute is supported by the compiler.
 */

#ifdef NOVA_HAS_CPP_ATTRIBUTE
#  undef NOVA_HAS_CPP_ATTRIBUTE
#endif
#if defined(__has_cpp_attribute) && defined(__cplusplus)
#  define NOVA_HAS_CPP_ATTRIBUTE(attribute) __has_cpp_attribute(attribute)
#else
#  define NOVA_HAS_CPP_ATTRIBUTE(attribute) (0)
#endif
/*!
 * @def NOVA_HAS_CPP_ATTRIBUTE
 * @brief Checks for the presence of a C++ compiler attribute named by
 * `attribute`.
 *
 * Example
 * ```
 * #if NOVA_HAS_CPP_ATTRIBUTE(nodiscard)
 * [[nodiscard]]
 * #endif
 * int foo(int i) { return i * i; }
 * ```
 *
 * @return non-zero value if the attribute is supported by the compiler.
 */

// -----------------------------------------------------------------------------
// Compiler builtin check
// -----------------------------------------------------------------------------

#ifdef NOVA_HAS_BUILTIN
#  undef NOVA_HAS_BUILTIN
#endif
#ifdef __has_builtin
#  define NOVA_HAS_BUILTIN(builtin) __has_builtin(builtin)
#else
#  define NOVA_HAS_BUILTIN(builtin) (0)
#endif
/*!
 * @def NOVA_HAS_BUILTIN
 * @brief Checks for the presence of a compiler builtin function named by
 * `builtin`.
 *
 * Example
 * ```
 * #if NOVA_HAS_BUILTIN(__builtin_trap)
 *   __builtin_trap();
 * #else
 *   abort();
 * #endif
 * ```
 *
 * @return non-zero value if the builtin function is supported by the compiler.
 */

// -----------------------------------------------------------------------------
// Compiler feature check
// -----------------------------------------------------------------------------

#ifdef NOVA_HAS_FEATURE
#  undef NOVA_HAS_FEATURE
#endif
#ifdef __has_feature
#  define NOVA_HAS_FEATURE(feature) __has_feature(feature)
#else
#  define NOVA_HAS_FEATURE(feature) (0)
#endif
/*!
 * @def NOVA_HAS_FEATURE
 * @brief Checks for the presence of a compiler feature named by `feature`.
 *
 * Example
 * ```
 * #if NOVA_HAS_FEATURE(attribute_overloadable) || NOVA_HAS_FEATURE(blocks)
 * ...
 * #endif
 * ```
 *
 * @return non-zero value if the feature is supported by the compiler.
 */

// -----------------------------------------------------------------------------
// Pragma
// -----------------------------------------------------------------------------

// NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if)
#if (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L))               \
  || defined(__clang__) || NOVA_GCC_VERSION_CHECK(3, 0, 0)
#  define NOVA_PRAGMA(value) _Pragma(#value)
#elif NOVA_MSVC_VERSION_CHECK(15, 0, 0)
#  define NOVA_PRAGMA(value) __pragma(value)
#else
#  define NOVA_PRAGMA(value)
#endif
/*!
 * @def NOVA_PRAGMA
 *
 * @brief Produce a `pragma` directive for the compiler.
 *
 * Pragma directives specify machine-specific or operating system-specific
 * compiler features. There are different ways compilers support the
 * specification of pragmas and this macro will simply abstract these ways in a
 * single generic way.
 */

// -----------------------------------------------------------------------------
// Compiler diagnostics
// -----------------------------------------------------------------------------

#ifdef NOVA_DIAGNOSTIC_PUSH
#  undef NOVA_DIAGNOSTIC_PUSH
#endif
#ifdef NOVA_DIAGNOSTIC_POP
#  undef NOVA_DIAGNOSTIC_POP
#endif
#ifdef NOVA_DIAGNOSTIC_DISABLE
#  undef NOVA_DIAGNOSTIC_DISABLE
#endif
#ifdef NOVA_DIAGNOSTIC_DISABLE_CLANG
#  undef NOVA_DIAGNOSTIC_DISABLE_CLANG
#endif
#ifdef NOVA_DIAGNOSTIC_DISABLE_MSVC
#  undef NOVA_DIAGNOSTIC_DISABLE_MSVC
#endif
#ifdef NOVA_DIAGNOSTIC_DISABLE_GCC
#  undef NOVA_DIAGNOSTIC_DISABLE_GCC
#endif

#ifdef NOVA_CLANG_VERSION
#  define NOVA_DIAGNOSTIC_PUSH _Pragma("clang diagnostic push")
#  define NOVA_DIAGNOSTIC_POP _Pragma("clang diagnostic pop")
#  define NOVA_DIAGNOSTIC_DISABLE(id) NOVA_PRAGMA(clang diagnostic ignored id)
#  define NOVA_DIAGNOSTIC_DISABLE_CLANG(id) NOVA_DIAGNOSTIC_DISABLE(id)
#  ifdef NOVA_MSVC_VERSION
#    define NOVA_DIAGNOSTIC_DISABLE_MSVC(id) __pragma(warning(disable : (id)))
#  else
#    define NOVA_DIAGNOSTIC_DISABLE_MSVC(id)
#  endif
#  define NOVA_DIAGNOSTIC_DISABLE_GCC(id)
#elif NOVA_GCC_VERSION_CHECK(4, 6, 0)
#  define NOVA_DIAGNOSTIC_PUSH _Pragma("GCC diagnostic push")
#  define NOVA_DIAGNOSTIC_POP _Pragma("GCC diagnostic pop")
#  define NOVA_DIAGNOSTIC_DISABLE(id) NOVA_PRAGMA(GCC diagnostic ignored id)
#  define NOVA_DIAGNOSTIC_DISABLE_CLANG(id)
#  define NOVA_DIAGNOSTIC_DISABLE_MSVC(id)
#  define NOVA_DIAGNOSTIC_DISABLE_GCC(id) NOVA_DIAGNOSTIC_DISABLE(id)
#elif NOVA_MSVC_VERSION_CHECK(15, 0, 0)
#  define NOVA_DIAGNOSTIC_PUSH __pragma(warning(push))
#  define NOVA_DIAGNOSTIC_POP __pragma(warning(pop))
#  define NOVA_DIAGNOSTIC_DISABLE(id) __pragma(warning(disable : id))
#  define NOVA_DIAGNOSTIC_DISABLE_CLANG(id)
#  define NOVA_DIAGNOSTIC_DISABLE_MSVC(id) NOVA_DIAGNOSTIC_DISABLE(id)
#  define NOVA_DIAGNOSTIC_DISABLE_GCC(id)
#else
#  define NOVA_DIAGNOSTIC_PUSH
#  define NOVA_DIAGNOSTIC_POP
#  define NOVA_DIAGNOSTIC_DISABLE(id)
#  define NOVA_DIAGNOSTIC_DISABLE_CLANG(id)
#  define NOVA_DIAGNOSTIC_DISABLE_MSVC(id)
#  define NOVA_DIAGNOSTIC_DISABLE_GCC(id)
#endif
/*!
 * @def NOVA_DIAGNOSTIC_PUSH
 *
 * @brief Remember the current state of the compiler's diagnostics.
 *
 * Example
 * ```
 * NOVA_DIAGNOSTIC_PUSH
 * #if defined(__clang__) && NOVA_HAS_WARNING("-Wunused-const-variable")
 * #pragma clang diagnostic ignored "-Wunused-const-variable"
 * #endif
 * const char *const FOO = "foo";
 * NOVA_DIAGNOSTIC_POP
 * ```
 *
 * @see NOVA_DIAGNOSTIC_POP
 */

/*!
 * @def NOVA_DIAGNOSTIC_POP
 *
 * @brief Return the state of the compiler's diagnostics to the value from the
 * last push.
 *
 * Example
 * ```
 * NOVA_DIAGNOSTIC_PUSH
 * #if defined(__clang__) && NOVA_HAS_WARNING("-Wunused-const-variable")
 * #pragma clang diagnostic ignored "-Wunused-const-variable"
 * #endif
 * const char *const FOO = "foo";
 * NOVA_DIAGNOSTIC_POP
 * ```
 *
 * @see NOVA_DIAGNOSTIC_PUSH
 */

/*!
 * @def NOVA_DIAGNOSTIC_DISABLE
 *
 * @brief Disable a specific diagnostic warning for the compiler.
 *
 * This macro disables a specific diagnostic warning for the supported compilers
 * (Clang, GCC, and MSVC).
 *
 * Example
 * ```
 * NOVA_DIAGNOSTIC_PUSH
 * NOVA_DIAGNOSTIC_DISABLE(26495) // For MSVC
 * NOVA_DIAGNOSTIC_DISABLE(-Wpadded) // For Clang
 * NOVA_DIAGNOSTIC_DISABLE(-Wuninitialized) // For GCC
 * // Your code here
 * NOVA_DIAGNOSTIC_POP
 * ```
 *
 * @see NOVA_DIAGNOSTIC_PUSH
 * @see NOVA_DIAGNOSTIC_POP
 */

#ifdef NOVA_HAS_WARNING
#  undef NOVA_HAS_WARNING
#endif
#ifdef __has_warning
#  define NOVA_HAS_WARNING(warning) __has_warning(warning)
#else
#  define NOVA_HAS_WARNING(warning) (0)
#endif
/*!
 * @def NOVA_HAS_WARNING
 * @brief Checks for the presence of a compiler warning named by `warning`.
 *
 * Example
 * ```
 * NOVA_DIAGNOSTIC_PUSH
 * #if defined(__clang__) && NOVA_HAS_WARNING("-Wunused-const-variable")
 * #pragma clang diagnostic ignored "-Wunused-const-variable"
 * #endif
 * const char *const FOO = "foo";
 * NOVA_DIAGNOSTIC_POP
 * ```
 *
 * @return non-zero value if the feature is supported by the compiler.
 */

// -----------------------------------------------------------------------------
// assume, unreachable, unreachable return
// -----------------------------------------------------------------------------

#ifdef NOVA_UNREACHABLE
#  undef NOVA_UNREACHABLE
#endif
#ifdef NOVA_UNREACHABLE_RETURN
#  undef NOVA_UNREACHABLE_RETURN
#endif
#ifdef NOVA_ASSUME
#  undef NOVA_ASSUME
#endif
#if NOVA_MSVC_VERSION_CHECK(13, 10, 0)
#  define NOVA_ASSUME(expr) __assume(expr)
#elif NOVA_HAS_BUILTIN(__builtin_assume)
#  define NOVA_ASSUME(expr) __builtin_assume(expr)
#endif
#if NOVA_HAS_BUILTIN(__builtin_unreachable) || NOVA_GCC_VERSION_CHECK(4, 5, 0)
#  define NOVA_UNREACHABLE() __builtin_unreachable()
#elif defined(NOVA_ASSUME)
#  define NOVA_UNREACHABLE() NOVA_ASSUME(0)
#endif
#ifndef NOVA_ASSUME
#  if defined(NOVA_UNREACHABLE)
#    define NOVA_ASSUME(expr)                                                  \
      NOVA_STATIC_CAST(void, ((expr) ? 1 : (NOVA_UNREACHABLE(), 1)))
#  else
#    define NOVA_ASSUME(expr) NOVA_STATIC_CAST(void, expr)
#  endif
#endif
#ifdef NOVA_UNREACHABLE
#  define NOVA_UNREACHABLE_RETURN(value) NOVA_UNREACHABLE()
#else
#  define NOVA_UNREACHABLE_RETURN(value) return (value)
#endif
#ifndef NOVA_UNREACHABLE
#  define NOVA_UNREACHABLE() NOVA_ASSUME(0)
#endif

/*!
 * @def NOVA_UNREACHABLE
 *
 * @brief Inform the compiler/analyzer that the code should never be reached
 * (even with invalid input).
 *
 * Example
 * ```
 * switch (foo) {
 * 	case BAR:
 * 		do_something();
 * 		break;
 * 	case BAZ:
 * 		do_something_else();
 * 		break;
 * 	default:
 * 		NOVA_UNREACHABLE();
 * 		break;
 * }
 * ```
 * @see NOVA_UNREACHABLE_RETURN
 */

/*!
 * @def NOVA_UNREACHABLE_RETURN
 *
 * @brief Inform the compiler/analyzer that the code should never be reached or,
 * for compilers which don't provide a way to provide such information, return a
 * value.
 *
 * Example
 * ```
 * static int handle_code(enum Foo code) {
 *   switch (code) {
 *   case FOO_BAR:
 *   case FOO_BAZ:
 *   case FOO_QUX:
 *     return 0;
 *   }
 *
 * NOVA_UNREACHABLE_RETURN(0);
 * }
 * ```
 * @see NOVA_UNREACHABLE
 */

/*!
 * @def NOVA_ASSUME
 *
 * @brief Inform the compiler/analyzer that the provided expression should
 * always evaluate to a non-false value.
 *
 * Note that the compiler is free to assume that the expression never evaluates
 * to true and therefore can elide code paths where it does evaluate to true.
 *
 * Example
 * ```
 * unsigned sum_small(unsigned data[], size_t count) {
 *   __builtin_assume(count <= 4);
 *   unsigned sum = 0;
 *   for (size_t i = 0; i < count; ++i) {
 *     sum += data[i];
 *   }
 *   return sum;
 * }
 * ```
 */

// -----------------------------------------------------------------------------
// fallthrough
// -----------------------------------------------------------------------------

#ifdef NOVA_FALL_THROUGH
#  undef NOVA_FALL_THROUGH
#endif
#if NOVA_HAS_ATTRIBUTE(fallthrough) || NOVA_GCC_VERSION_CHECK(7, 0, 0)
#  define NOVA_FALL_THROUGH __attribute__((__fallthrough__))
#else
#  define NOVA_FALL_THROUGH
#endif

/*!
 * @def NOVA_FALL_THROUGH
 *
 * @brief Explicitly tell the compiler to fall through a case in the switch
 * statement. Without this, some compilers may think you accidentally omitted a
 * "break;" and emit a diagnostic.
 *
 * Example
 * ```
 * switch (foo) {
 * 	case FOO:
 * 		handle_foo();
 * 		NOVA_FALL_THROUGH;
 * 	case BAR:
 * 		handle_bar();
 * 		break;
 * }
 * ```
 */

// -----------------------------------------------------------------------------
// no unique address
// -----------------------------------------------------------------------------

#ifdef NOVA_NO_UNIQUE_ADDRESS
#  undef NOVA_NO_UNIQUE_ADDRESS
#endif
#ifdef _MSC_VER
#  define NOVA_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#  define NOVA_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

/*!
 * @def NOVA_NO_UNIQUE_ADDRESS
 *
 * @brief Attaches to a non-static data member to indicate that the member
 * should not have a separate address from other members of the class.
 *
 * This allows the compiler to optimize the layout of classes that contain
 * empty members (like empty lambdas or stateless objects) by overlapping
 * their storage.
 *
 * @note MSVC and Clang on Windows require `[[msvc::no_unique_address]]` to
 * enable this optimization, as they ignore the standard C++20 attribute for ABI
 * compatibility reasons.
 *
 * Example
 * ```
 * struct Empty {};
 * struct Foo {
 *   NOVA_NO_UNIQUE_ADDRESS Empty e;
 *   int i;
 * };
 * static_assert(sizeof(Foo) == sizeof(int));
 * ```
 */

// NOLINTEND(cppcoreguidelines-macro-usage)
