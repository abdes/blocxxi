//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

/*!
 * @file
 *
 * @brief Contains compiler definitions and macros for platform detection.
 */

#pragma once

// ----------------------------------------------------------------------------
//   Operating System detection
// ----------------------------------------------------------------------------

// WINDOWS
#if defined(_WIN32) // defined for 32-bit and 64-bit environments
#  define NOVA_WINDOWS
#  if defined(__CYGWIN__) // non-POSIX CygWin
#    define NOVA_WINDOWS_CYGWIN
#  endif
#  if defined(__MINGW32__) || defined(__MINGW64__)
#    define NOVA_WINDOWS_MINGW
#  endif
// Not a Windows
// UNIX-style OS
// All UNIX-style OSes define some form of the unix symbol, except for Apple.
// GCC with CygWin also defines unix symbols even when building WIN32 apps and
// this is why UNIX detection is within the `#elif` of `_WIN32`
#elif (defined(__unix__) || defined(__unix)                                    \
  || (defined(__APPLE__) && defined(__MACH__)))
#  define NOVA_UNIX // UNIX-style OS.
// Apple OSX, iOS, Darwin
#  if defined(__APPLE__) && defined(__MACH__)
#    define NOVA_APPLE // Apple OSX and iOS (Darwin)
#    include <TargetConditionals.h>
#    if TARGET_IPHONE_SIMULATOR == 1
#      define NOVA_APPLE_IOS_SIMULATOR // iOS in Xcode simulator
#    elif TARGET_OS_IPHONE == 1
#      define NOVA_APPLE_IOS // iOS on iPhone, iPad, etc.
#    elif TARGET_OS_MAC == 1
#      define NOVA_APPLE_OSX // OSX
#    endif
#  endif
// CygWin (not WIN32)
#  if defined(__CYGWIN__)
#    define NOVA_CYGWIN
#  endif
// Any Linux based OS, including Gnu/Linux and Android
#  if defined(__linux__)
#    define NOVA_LINUX
#    if defined(__gnu_linux__) // Specifically Gnu/Linux
#      define NOVA_GNU_LINUX
#    endif
#    if defined(__ANDROID__) // Android (which also defines __linux__)
#      define NOVA_ANDROID
#    endif
#  endif
// Solaris and SunOS
#  if defined(sun) || defined(__sun)
#    define NOVA_SUN
#    if defined(__SVR4) || defined(__svr4__)
#      define NOVA_SUN_SOLARIS // Solaris
#    else
#      define NOVA_SUN_SUNOS // SunOS
#    endif
#  endif
// HP-UX
#  if defined(__hpux)
#    define NOVA_HPUX
#  endif
// IBM AIX
#  if defined(_AIX)
#    define NOVA_AIX
#  endif
// BSD (DragonFly BSD, FreeBSD, OpenBSD, NetBSD)
#  if defined(BSD)
#    include <sys/param.h>
#    define NOVA_BSD
#  endif
#endif
