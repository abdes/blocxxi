//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Nova/Testing/GTest.h>

#include <Nova/Base/Logging.h>

#if LOGURU_USE_FMTLIB

// File-scope ADL test in a named namespace to ensure ADL finds free functions
namespace adl_ns {
struct OtherNS {
  int v { 7 };
};
inline auto to_string(OtherNS const& o) -> std::string
{
  return "nsADL:" + std::to_string(o.v);
}
} // namespace adl_ns

// ADL to_string that returns std::string_view. Tests that adapter accepts
// string_view return values from ADL to_string. Place the type in a named
// namespace so we can provide a fmt::formatter specialization to teach fmt
// how to format the type via the ADL to_string that returns string_view.
namespace adl_sv_ns {
struct ToStringView {
  int v { 5 };
};
inline auto to_string(ToStringView const& /*t*/) -> std::string_view
{
  return "sv-5";
}
} // namespace adl_sv_ns

// ADL lifetime helpers for tests: one returning string_view into an
// lvalue's member, one returning string_view into a temporary (rvalue).
namespace adl_lifetime {
struct LvalueBacked {
  std::string data;
};
inline auto to_string(LvalueBacked const& l) -> std::string_view
{
  return std::string_view(l.data);
}

struct RvalueView {
  std::string data { "tmp-view" };
};
inline auto to_string(RvalueView const& r) -> std::string_view
{
  return std::string_view(r.data);
}
} // namespace adl_lifetime

namespace {

using ::testing::AllOf;
using ::testing::Ge;
using ::testing::HasSubstr;
using ::testing::Not;

//=== Test Helper Types ===---------------------------------------------------//

struct ADLType {
  int v { 0 };
};
inline auto to_string(ADLType const& t) -> std::string
{
  return "ADL:" + std::to_string(t.v);
}

// Type that is string-like (convertible to string_view) but also has an
// ADL to_string. The adapter should prefer ADL to_string over string-like
// conversion.
struct MaybeStringLike {
  int v { 9 };
  operator std::string_view() const { return "maybe"; }
};
inline auto to_string(MaybeStringLike const& /*m*/) -> std::string
{
  return "adl-9";
}

// Simple ADL-enabled loggable type used by LOG_F capture test.
struct Loggable {
  int v { 0 };
};
[[maybe_unused]] inline auto to_string(Loggable const& l) -> std::string
{
  return "LOG:" + std::to_string(l.v);
}

// ADL rvalue helper used by ADLRvalue test
struct Rval {
  int v { 11 };
};
inline auto to_string(Rval const& r) -> std::string
{
  return "R:" + std::to_string(r.v);
}

//=== Base Test Fixture ===---------------------------------------------------//

// Reorganized fixtures:
// - BasicLoggingFixture: saves/restores verbosity and provides capture helpers.
// - NoExceptLoggingTests: sanity tests that extend BasicLoggingFixture.
// - LogWithAdapterTests: adapter-focused tests that extend BasicLoggingFixture.

class BasicLoggingFixture : public testing::Test {
protected:
  void SetUp() override
  {
    saved_verbosity_ = loguru::g_global_verbosity;
    saved_log_to_stderr_ = loguru::g_log_to_stderr;
    // Default tests to INFO so individual tests don't need to set verbosity.
    loguru::g_global_verbosity = loguru::Verbosity_INFO;
    loguru::g_log_to_stderr = true;
  }

  void TearDown() override
  {
    loguru::g_global_verbosity = saved_verbosity_;
    loguru::g_log_to_stderr = saved_log_to_stderr_;
  }

  void SetVerbosity(loguru::Verbosity v) { loguru::g_global_verbosity = v; }

  // Helper to capture stderr while running a callable and return the output.
  template <typename F> auto CaptureStderr(F&& f) -> std::string
  {
    testing::internal::CaptureStderr();
    std::forward<F>(f)();
    return testing::internal::GetCapturedStderr();
  }

private:
  loguru::Verbosity saved_verbosity_ { loguru::Verbosity_OFF };
  bool saved_log_to_stderr_ { true };
};

//=== Basic Logging Functionality Tests ===-----------------------------------//

//! Verify that LOG_F emits output when verbosity allows it
NOLINT_TEST_F(BasicLoggingFixture, BasicLogging_EmitsWhenVerbosityAllows)
{
  // Arrange
  const std::string expected_content = "Smoke: 1";

  // Act
  auto output = CaptureStderr([]() -> void { LOG_F(INFO, "Smoke: {}", 1); });

  // Assert
  EXPECT_THAT(output, HasSubstr(expected_content));
}

//=== Exception Safety Tests ===----------------------------------------------//

class NoExceptLoggingTests : public BasicLoggingFixture { };

//! Verify that logging does not throw exceptions under normal operation
NOLINT_TEST_F(NoExceptLoggingTests, ExceptionSafety_DoesNotThrowOnLog)
{
  // Arrange
  const std::string expected_message = "Test log message";

  // Act & Assert
  std::string output = CaptureStderr([&]() -> void {
    NOLINT_EXPECT_NO_THROW({ LOG_F(INFO, expected_message.c_str()); });
  });

  EXPECT_THAT(output, HasSubstr(expected_message));
}

//! Verify that null format strings are handled gracefully without throwing
NOLINT_TEST_F(
  NoExceptLoggingTests, ExceptionSafety_HandlesNullFormatStringGracefully)
{
  // Arrange
  const char* null_fmt = nullptr;

  // Act
  std::string output = CaptureStderr(
    [&] { NOLINT_EXPECT_NO_THROW({ LOG_F(INFO, null_fmt); }); });

  // Assert
  EXPECT_THAT(output, HasSubstr("Null format string"));
}

//! Verify that format errors are reported without throwing exceptions
NOLINT_TEST_F(
  NoExceptLoggingTests, ExceptionSafety_ReportsFormatErrorsWithoutThrowing)
{
  // Act
  std::string output = CaptureStderr([&]() -> void {
    NOLINT_EXPECT_NO_THROW({
      LOG_F(INFO, "Invalid format: {0} {1} {2} {bogus}", 42, 3.14, "test");
    });
  });

  // Assert
  EXPECT_THAT(output, HasSubstr("argument not found"));
}

//! Verify that very long messages are handled without throwing
NOLINT_TEST_F(NoExceptLoggingTests, ExceptionSafety_HandlesVeryLongMessage)
{
  // Arrange
  const std::string long_msg(10000, 'x');

  // Act
  auto output = CaptureStderr([&]() -> void { LOG_F(INFO, "{}", long_msg); });

  // Assert
  EXPECT_THAT(output.size(), Ge(long_msg.size()));
}

//! Verify that nullptr arguments are handled safely
NOLINT_TEST_F(NoExceptLoggingTests, ExceptionSafety_HandlesNullptrArguments)
{
  // Arrange
  const char* ptr = nullptr;

  // Act
  std::string output = CaptureStderr(
    [&] { NOLINT_EXPECT_NO_THROW({ LOG_F(INFO, "Null pointer: {}", ptr); }); });

  // Assert
  EXPECT_THAT(output, HasSubstr("Null pointer: (null)"));
}

//! Verify that empty format strings produce output without errors
NOLINT_TEST_F(NoExceptLoggingTests, ExceptionSafety_EmptyFormatProducesOutput)
{
  // Act
  std::string output
    = CaptureStderr([&] { NOLINT_EXPECT_NO_THROW({ LOG_F(INFO, ""); }); });

  // Assert
  EXPECT_FALSE(output.empty());

  // Convert to lowercase for case-insensitive search
  auto lower = output;
  std::transform(lower.begin(), lower.end(), lower.begin(),
    [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });
  EXPECT_THAT(lower, AllOf(Not(HasSubstr("error")), Not(HasSubstr("fail"))));
}

//=== Adapter-Focused Tests ===-----------------------------------------------//

// Adapter-focused tests; these use textprintf and validate returned strings.
class LogWithAdapterTests : public BasicLoggingFixture { };

//! Verify that ADL to_string conversion works for custom types
NOLINT_TEST_F(LogWithAdapterTests, ADLConversion_UsesADLToStringForCustomTypes)
{
  // Arrange
  ADLType x { 42 };

  // Act
  auto output = CaptureStderr([&]() -> void { LOG_F(INFO, "{}", x); });

  // Assert
  EXPECT_THAT(output, HasSubstr("ADL:42"));
}

//! Verify that string-like types are converted to std::string properly
NOLINT_TEST_F(
  LogWithAdapterTests, StringConversion_ConvertsStringLikeTypesToString)
{
  // Arrange
  const char* cstr = "hello";
  std::string str = "world";
  std::string_view sv = "sv";

  // Act
  auto output1 = CaptureStderr([&]() -> void { LOG_F(INFO, "{}", cstr); });
  auto output2 = CaptureStderr([&]() -> void { LOG_F(INFO, "{}", str); });
  auto output3 = CaptureStderr([&]() -> void { LOG_F(INFO, "{}", sv); });

  // Assert
  EXPECT_THAT(output1, HasSubstr("hello"));
  EXPECT_THAT(output2, HasSubstr("world"));
  EXPECT_THAT(output3, HasSubstr("sv"));
}

//! Verify that numeric types pass through fmt formatting unchanged
NOLINT_TEST_F(LogWithAdapterTests, NumericFormatting_FormatsNumericPassthrough)
{
  // Arrange
  int i = 7;

  // Act
  auto output = CaptureStderr(
    [&]() -> void { LOG_F(INFO, "{} + {} = {}", i, 3, i + 3); });

  // Assert
  EXPECT_THAT(output, HasSubstr("7 + 3 = 10"));
}

//! Verify that ADL to_string is preferred over string-like conversion
NOLINT_TEST_F(
  LogWithAdapterTests, ADLPriority_ADLPreferredOverStringLikeConversion)
{
  // Arrange
  MaybeStringLike m {};

  // Act
  auto output = CaptureStderr([&]() -> void { LOG_F(INFO, "{}", m); });

  // Assert
  EXPECT_THAT(output, HasSubstr("adl-9"));
}

//! Verify that ADL functions are found in other namespaces
NOLINT_TEST_F(LogWithAdapterTests, ADLLookup_FindsADLInOtherNamespace)
{
  // Arrange
  adl_ns::OtherNS o { 7 };

  // Act
  auto output = CaptureStderr([&]() -> void { LOG_F(INFO, "{}", o); });

  // Assert
  EXPECT_THAT(output, HasSubstr("nsADL:7"));
}

//! Verify that ADL functions returning string_view work correctly
NOLINT_TEST_F(LogWithAdapterTests, ADLStringView_HandlesADLReturningStringView)
{
  // Arrange
  adl_sv_ns::ToStringView t { 5 };

  // Act
  auto output = CaptureStderr([&]() -> void { LOG_F(INFO, "{}", t); });

  // Assert
  EXPECT_THAT(output, HasSubstr("sv-5"));
}

//! Verify that string_view into lvalue data is handled correctly
NOLINT_TEST_F(
  LogWithAdapterTests, LifetimeManagement_HandlesADLStringViewIntoLvalue)
{
  // Arrange
  adl_lifetime::LvalueBacked v { std::string("from-lvalue") };

  // Act
  auto output = CaptureStderr([&]() -> void { LOG_F(INFO, "{}", v); });

  // Assert
  EXPECT_THAT(output, HasSubstr("from-lvalue"));
}

//! Verify that dangling string_view from rvalue is prevented
NOLINT_TEST_F(
  LogWithAdapterTests, LifetimeManagement_DisallowsDanglingStringViewFromRvalue)
{
  // Arrange
  adl_lifetime::RvalueView r;

  // Act
  auto output = CaptureStderr([&]() -> void { LOG_F(INFO, "{}", r); });

  // Assert
  EXPECT_THAT(output, HasSubstr("tmp-view"));
}

//! Verify that all argument lifetimes are preserved during formatting
NOLINT_TEST_F(
  LogWithAdapterTests, LifetimeManagement_PreservesAllArgumentLifetimes)
{
  // Arrange
  adl_lifetime::LvalueBacked v { std::string("L") };

  // Act
  auto output = CaptureStderr(
    [&]() -> void { LOG_F(INFO, "{} {} {}", v, std::string("T"), 123); });

  // Assert
  EXPECT_THAT(output, AllOf(HasSubstr("L"), HasSubstr("T"), HasSubstr("123")));
}

//! Verify that char arrays and string literals are formatted correctly
NOLINT_TEST_F(LogWithAdapterTests, StringFormatting_FormatsCharArrayAndLiteral)
{
  // Arrange
  char arr[] = "abc"; // char[4]

  // Act
  auto output
    = CaptureStderr([&]() -> void { LOG_F(INFO, "{} {}", arr, "z"); });

  // Assert
  EXPECT_THAT(output, HasSubstr("abc z"));
}

//! Verify that temporary rvalue strings are handled correctly
NOLINT_TEST_F(LogWithAdapterTests, RvalueHandling_HandlesTemporaryRvalueStrings)
{
  // Act
  auto output = CaptureStderr([&]() -> void {
    LOG_F(INFO, "{} {}", std::string("x"), std::string("y"));
  });

  // Assert
  EXPECT_THAT(output, HasSubstr("x y"));
}

//! Verify that ADL rvalue temporaries are formatted correctly
NOLINT_TEST_F(LogWithAdapterTests, RvalueHandling_FormatsADLRvalueTemporaries)
{
  // Act
  auto output = CaptureStderr([&]() -> void { LOG_F(INFO, "{}", Rval {}); });

  // Assert
  EXPECT_THAT(output, HasSubstr("R:11"));
}

//! Verify that dynamic precision formatting works with numeric passthrough
NOLINT_TEST_F(
  LogWithAdapterTests, NumericFormatting_HandlesDynamicPrecisionAndName)
{
  // Arrange
  const double duration_sec = 1.23456;
  const int precision = 3;
  const char* name = "unit";

  // Act
  auto output = CaptureStderr([&]() -> void {
    LOG_F(INFO, "{:.{}f} s: {:s}", duration_sec, precision, name);
  });

  // Assert (rounded to 3 decimals)
  EXPECT_THAT(output, HasSubstr("1.235 s: unit"));
}

} // namespace

#else // LOGURU_USE_FMTLIB

namespace {

//! Fallback test when fmt is disabled - ensures test suite compiles and runs
NOLINT_TEST(LoggingFallback, FmtDisabled_TestSuiteStillCompiles)
{
  // This test ensures the test suite compiles and runs even when
  // LOGURU_USE_FMTLIB is disabled
  NOLINT_SUCCEED();
}

} // namespace

#endif // LOGURU_USE_FMTLIB
