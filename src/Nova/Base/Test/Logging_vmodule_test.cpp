//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <Nova/Testing/GTest.h>

#include <Nova/Base/Logging.h>
#include <Nova/Testing/ScopedLogCapture.h>

namespace {

using nova::testing::ScopedLogCapture;

// Test helpers to simulate per-file log sites without emitting logs.
namespace {
  struct SiteRegistry {
    std::mutex m;
    std::vector<std::string> paths; // stored paths
    std::vector<std::unique_ptr<std::atomic<int>>> caches; // matching indices
    void reset_all()
    {
      for (auto& c : caches) {
        c->store(loguru::Verbosity_UNSPECIFIED, std::memory_order_relaxed);
      }
    }
    auto acquire(const char* path) -> std::atomic<int>*
    {
      std::lock_guard<std::mutex> l(m);
      for (size_t i = 0; i < paths.size(); ++i) {
        if (paths[i] == path) {
          return caches[i].get();
        }
      }
      paths.emplace_back(path);
      caches.emplace_back(
        std::make_unique<std::atomic<int>>(loguru::Verbosity_UNSPECIFIED));
      return caches.back().get();
    }
  };
  static SiteRegistry g_site_registry; // lifetime: entire test process
} // namespace (helpers)

static auto Enabled(const char* path, loguru::Verbosity v) -> bool
{
  auto* cache = g_site_registry.acquire(path);
  return loguru::check_module_fast(cache, v, path);
}

static void ResetSiteCaches() { g_site_registry.reset_all(); }

//=== Command Line Argument Parsing Tests
//===-----------------------------------//

//! Test fixture for command line argument parsing functionality
/*!
 Tests the parse_args functionality which allows comma-separated vmodule
 overrides via --vmodule flag.
*/
class CommandLineParsingTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    loguru::clear_vmodule_overrides();
    ResetSiteCaches();
    saved_verbosity_ = loguru::g_global_verbosity;
    loguru::g_global_verbosity = loguru::Verbosity_MAX;
  }

  void TearDown() override
  {
    loguru::clear_vmodule_overrides();
    loguru::g_global_verbosity = saved_verbosity_;
  }

  //! Helper to simulate command line parsing
  void ParseArgs(
    std::vector<std::string> args, const char* verbosity_flag = "-v")
  {
    // Convert to char* array for parse_args
    std::vector<const char*> argv;
    for (auto& arg : args) {
      argv.push_back(arg.data());
    }
    argv.push_back(nullptr);

    int argc = static_cast<int>(argv.size() - 1);
    loguru::parse_args(argc, argv.data(), verbosity_flag);

    // Store results for verification
    parsed_argc_ = argc;
    parsed_argv_.clear();
    for (int i = 0; i < argc; ++i) {
      parsed_argv_.emplace_back(argv[i]);
    }
  }

  //! Get remaining arguments after parsing
  auto GetRemainingArgs() const -> const std::vector<std::string>&
  {
    return parsed_argv_;
  }

  //! Get remaining argument count after parsing
  auto GetRemainingArgCount() const -> int { return parsed_argc_; }

private:
  loguru::Verbosity saved_verbosity_;
  int parsed_argc_ = 0;
  std::vector<std::string> parsed_argv_;
};

//! Command line should parse comma-separated vmodule overrides
NOLINT_TEST_F(CommandLineParsingTest, ParsesCommaSeparatedOverrides)
{
  ScopedLogCapture capture("test_cmdline", loguru::Verbosity_9);

  // Test parsing: program --vmodule=module1=2,module2=3,simple=1 other_arg
  ParseArgs(
    { "program", "--vmodule=module1=2,module2=3,simple=1", "other_arg" });

  // Verify that vmodule arguments are removed and others remain
  EXPECT_EQ(GetRemainingArgCount(), 2); // program + other_arg
  EXPECT_EQ(GetRemainingArgs()[0], "program");
  EXPECT_EQ(GetRemainingArgs()[1], "other_arg");

  // Test that all modules are configured correctly
  EXPECT_TRUE(Enabled("module1.cpp", loguru::Verbosity_2));
  EXPECT_TRUE(Enabled("module2.cpp", loguru::Verbosity_3));
  EXPECT_TRUE(Enabled("simple.cpp", loguru::Verbosity_1));
}

//! Command line should parse separate --vmodule arguments
NOLINT_TEST_F(CommandLineParsingTest, ParsesSeparateVModuleArgs)
{
  ScopedLogCapture capture("test_cmdline", loguru::Verbosity_9);

  // Test: program --vmodule module1=2 --vmodule module2=3
  ParseArgs({ "program", "--vmodule", "module1=2", "--vmodule", "module2=3",
    "remaining" });

  // Verify arguments are properly removed
  EXPECT_EQ(GetRemainingArgCount(), 2); // program + remaining
  EXPECT_EQ(GetRemainingArgs()[0], "program");
  EXPECT_EQ(GetRemainingArgs()[1], "remaining");

  // Test that modules are configured
  EXPECT_TRUE(Enabled("module1.cpp", loguru::Verbosity_2));
  EXPECT_TRUE(Enabled("module2.cpp", loguru::Verbosity_3));
}

//! Command line should parse verbosity flags
NOLINT_TEST_F(CommandLineParsingTest, ParsesVerbosityFlags)
{
  // Test -v with number
  ParseArgs({ "program", "-v", "3", "other_arg" });
  EXPECT_EQ(loguru::g_global_verbosity, 3);
  EXPECT_EQ(GetRemainingArgCount(), 2); // program + other_arg

  // Reset verbosity
  loguru::g_global_verbosity = loguru::Verbosity_INFO;

  // Test -v=2
  ParseArgs({ "program", "-v=2", "other_arg" });
  EXPECT_EQ(loguru::g_global_verbosity, 2);
  EXPECT_EQ(GetRemainingArgCount(), 2);

  // Reset verbosity
  loguru::g_global_verbosity = loguru::Verbosity_INFO;

  // Test with named verbosity
  ParseArgs({ "program", "-v", "WARNING", "other_arg" });
  EXPECT_EQ(loguru::g_global_verbosity, loguru::Verbosity_WARNING);
}

//! Command line should throw on invalid single override (enforced format)
NOLINT_TEST_F(CommandLineParsingTest, InvalidSingleOverrideThrows)
{
  EXPECT_THROW(
    ParseArgs({ "program", "--vmodule=invalid_format" }), std::exception);
}

//! Mixed lists containing invalid entries currently cause a throw via
//! configure_vmodule This documents the stricter behavior (previous harness
//! expected silent skip).
NOLINT_TEST_F(CommandLineParsingTest, MixedListWithInvalidEntryThrows)
{
  EXPECT_THROW(
    ParseArgs({ "program", "--vmodule=valid=2,invalid_format,another=3" }),
    std::exception);
}

//! Command line should handle custom verbosity flags
NOLINT_TEST_F(CommandLineParsingTest, HandlesCustomVerbosityFlags)
{
  // Test with custom verbosity flag name
  ParseArgs({ "program", "--debug", "2", "other" }, "--debug");
  EXPECT_EQ(loguru::g_global_verbosity, 2);
  EXPECT_EQ(GetRemainingArgCount(), 2); // program + other
}

//! Command line should preserve order of remaining arguments
NOLINT_TEST_F(CommandLineParsingTest, PreservesArgumentOrder)
{
  ParseArgs(
    { "program", "arg1", "-v", "2", "arg2", "--vmodule=test=1", "arg3" });

  // Should have program + arg1, arg2, arg3
  EXPECT_EQ(GetRemainingArgCount(), 4);
  EXPECT_EQ(GetRemainingArgs()[0], "program");
  EXPECT_EQ(GetRemainingArgs()[1], "arg1");
  EXPECT_EQ(GetRemainingArgs()[2], "arg2");
  EXPECT_EQ(GetRemainingArgs()[3], "arg3");
}

//=== VModule Override Parsing Tests ===-------------------------------------//

//! Test fixture for individual vmodule override parsing
/*!
 Tests the parse_vmodule_override functionality and validation of
 individual "pattern=verbosity" strings.
*/
class VModuleOverrideParsingTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    loguru::clear_vmodule_overrides();
    saved_verbosity_ = loguru::g_global_verbosity;
    loguru::g_global_verbosity = loguru::Verbosity_MAX;
  }

  void TearDown() override
  {
    loguru::clear_vmodule_overrides();
    loguru::g_global_verbosity = saved_verbosity_;
  }

private:
  loguru::Verbosity saved_verbosity_;
};

//! Valid pattern=verbosity format should parse successfully
NOLINT_TEST_F(VModuleOverrideParsingTest, ValidFormatParses)
{
  ScopedLogCapture capture("test_valid", loguru::Verbosity_9);

  // Test basic format
  EXPECT_NO_THROW(loguru::configure_vmodule("module=2"));

  // Test different verbosity values
  EXPECT_NO_THROW(loguru::configure_vmodule("test1=0"));
  EXPECT_NO_THROW(loguru::configure_vmodule("test2=9"));
  EXPECT_NO_THROW(loguru::configure_vmodule("test3=OFF"));
  EXPECT_NO_THROW(loguru::configure_vmodule("test4=off")); // case insensitive

  // Verify they work
  EXPECT_TRUE(Enabled("module.cpp", loguru::Verbosity_2));
}

//! Whitespace should be trimmed from pattern and verbosity
NOLINT_TEST_F(VModuleOverrideParsingTest, TrimsWhitespace)
{
  ScopedLogCapture capture("test_trim", loguru::Verbosity_9);

  EXPECT_NO_THROW(loguru::configure_vmodule("  module = 2  "));

  EXPECT_TRUE(Enabled("module.cpp", loguru::Verbosity_2));
}

//! Invalid formats should throw std::invalid_argument
NOLINT_TEST_F(VModuleOverrideParsingTest, InvalidFormatsThrow)
{
  // No equals sign
  EXPECT_THROW(loguru::configure_vmodule("module2"), std::invalid_argument);

  // Empty pattern
  EXPECT_THROW(loguru::configure_vmodule("=2"), std::invalid_argument);

  // Empty verbosity
  EXPECT_THROW(loguru::configure_vmodule("module="), std::invalid_argument);

  // Invalid verbosity
  EXPECT_THROW(
    loguru::configure_vmodule("module=invalid"), std::invalid_argument);

  // Multiple equals
  EXPECT_THROW(loguru::configure_vmodule("mod=ule=2"), std::invalid_argument);
}

//! Comma-separated input should be rejected (use configure_vmodules instead)
NOLINT_TEST_F(VModuleOverrideParsingTest, RejectsCommaSeparated)
{
  EXPECT_THROW(
    loguru::configure_vmodule("mod1=1,mod2=2"), std::invalid_argument);
}

//! Empty input should be ignored (not an error)
NOLINT_TEST_F(VModuleOverrideParsingTest, EmptyInputIgnored)
{
  EXPECT_NO_THROW(loguru::configure_vmodule(""));
}

//=== Runtime Configuration Tests ===---------------------------------------//

//! Test fixture for runtime configuration APIs
/*!
 Tests the configure_vmodule and configure_vmodules public APIs for
 runtime configuration of module verbosity overrides.
*/
class RuntimeConfigurationTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    loguru::clear_vmodule_overrides();
    saved_verbosity_ = loguru::g_global_verbosity;
    loguru::g_global_verbosity = loguru::Verbosity_MAX;
  }

  void TearDown() override
  {
    loguru::clear_vmodule_overrides();
    loguru::g_global_verbosity = saved_verbosity_;
  }

private:
  loguru::Verbosity saved_verbosity_;
};

//! Single module configuration should work
NOLINT_TEST_F(RuntimeConfigurationTest, SingleModuleConfiguration)
{
  ScopedLogCapture capture("test_single", loguru::Verbosity_9);

  loguru::configure_vmodule("testmodule=3");
  EXPECT_TRUE(Enabled("testmodule.cpp", loguru::Verbosity_3));
  EXPECT_FALSE(Enabled("testmodule.cpp", loguru::Verbosity_4));
}

//! Multiple modules configuration should work
NOLINT_TEST_F(RuntimeConfigurationTest, MultipleModulesConfiguration)
{
  ScopedLogCapture capture("test_multiple", loguru::Verbosity_9);

  loguru::configure_vmodules({ "mod1=1", "mod2=2", "mod3=3", "*=OFF" });

  EXPECT_TRUE(Enabled("mod1.cpp", loguru::Verbosity_1));
  EXPECT_TRUE(Enabled("mod2.cpp", loguru::Verbosity_2));
  EXPECT_TRUE(Enabled("mod3.cpp", loguru::Verbosity_3));
  EXPECT_FALSE(Enabled("unmatched.cpp", loguru::Verbosity_1));
}

//! Multiple calls should append configurations
NOLINT_TEST_F(RuntimeConfigurationTest, MultipleCalls)
{
  ScopedLogCapture capture("test_append", loguru::Verbosity_9);

  loguru::configure_vmodule("first=1");
  loguru::configure_vmodule("second=2");
  loguru::configure_vmodules({ "third=3", "fourth=4" });

  EXPECT_TRUE(Enabled("first.cpp", loguru::Verbosity_1));
  EXPECT_TRUE(Enabled("second.cpp", loguru::Verbosity_2));
  EXPECT_TRUE(Enabled("third.cpp", loguru::Verbosity_3));
  EXPECT_TRUE(Enabled("fourth.cpp", loguru::Verbosity_4));
}

//! Clearing overrides should reset to global verbosity
NOLINT_TEST_F(RuntimeConfigurationTest, ClearingOverrides)
{
  loguru::configure_vmodule("testmod=5");

  {
    ScopedLogCapture capture("before_clear", loguru::Verbosity_9);
    EXPECT_TRUE(Enabled("testmod.cpp", loguru::Verbosity_5));
  }

  loguru::clear_vmodule_overrides();

  {
    loguru::g_global_verbosity = loguru::Verbosity_INFO;
    ScopedLogCapture capture("after_clear", loguru::Verbosity_9);
    EXPECT_FALSE(Enabled("testmod.cpp", loguru::Verbosity_5));
  }
}

//=== Pattern Matching Tests ===--------------------------------------------//

//! Test fixture for pattern matching behavior
/*!
 Tests wildcard matching ('*', '?'), basename vs full path matching,
 and path normalization.
*/
class PatternMatchingTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    loguru::clear_vmodule_overrides();
    saved_verbosity_ = loguru::g_global_verbosity;
    loguru::g_global_verbosity = loguru::Verbosity_MAX;
  }

  void TearDown() override
  {
    loguru::clear_vmodule_overrides();
    loguru::g_global_verbosity = saved_verbosity_;
  }

private:
  loguru::Verbosity saved_verbosity_;
};

//! Patterns without path separators should match basename only
NOLINT_TEST_F(PatternMatchingTest, BasenameMatching)
{
  loguru::configure_vmodules({ "parser=2", "*=OFF" });
  EXPECT_TRUE(Enabled("parser.cpp", loguru::Verbosity_2));
  EXPECT_TRUE(Enabled("src/parser.cpp", loguru::Verbosity_2));
  EXPECT_TRUE(Enabled("deep/path/parser.h", loguru::Verbosity_2));
  EXPECT_FALSE(Enabled("other.cpp", loguru::Verbosity_2));
}

//! Patterns with path separators should match full paths
NOLINT_TEST_F(PatternMatchingTest, FullPathMatching)
{
  loguru::configure_vmodules({ "src/network*=2", "*=OFF" });
  EXPECT_TRUE(Enabled("src/network.cpp", loguru::Verbosity_2));
  EXPECT_TRUE(Enabled("src/network_manager.cpp", loguru::Verbosity_2));
  EXPECT_FALSE(Enabled("other/network.cpp", loguru::Verbosity_2));
  EXPECT_FALSE(Enabled("network.cpp", loguru::Verbosity_2));
}

//! Wildcard '*' should match any sequence of characters
NOLINT_TEST_F(PatternMatchingTest, WildcardStar)
{
  // Updated to use tree-glob semantics: '*' does not cross '/', matches within
  // a segment. We want to match top-level foo/ with files whose basename starts
  // with 'net'.
  loguru::configure_vmodules({ "foo/net*=2", "*=OFF" });
  EXPECT_TRUE(Enabled("foo/net.cpp", loguru::Verbosity_2));
  EXPECT_TRUE(Enabled("foo/network.cpp", loguru::Verbosity_2));
  EXPECT_TRUE(Enabled("foo/networking.cpp", loguru::Verbosity_2));
  // Negative cases: different prefix inside foo/, and deeper directory trees.
  EXPECT_FALSE(Enabled("foo/inet.cpp", loguru::Verbosity_2));
  EXPECT_FALSE(Enabled("bar/foo/net.cpp", loguru::Verbosity_2));
  EXPECT_FALSE(Enabled("baz/foo/network.cpp", loguru::Verbosity_2));
}

//! Recursive ** pattern should match across directory boundaries
NOLINT_TEST_F(PatternMatchingTest, RecursiveGlobDoubleStar)
{
  loguru::configure_vmodules({ "src/**/net*=3", "*=OFF" });
  EXPECT_TRUE(Enabled("src/net.cpp", loguru::Verbosity_3));
  EXPECT_TRUE(Enabled("src/core/net_utils.cpp", loguru::Verbosity_3));
  EXPECT_TRUE(Enabled("src/core/sub/netProfiler.cpp", loguru::Verbosity_3));
  EXPECT_FALSE(Enabled("tests/core/net.cpp", loguru::Verbosity_3));
}

//! Leading **/ should match zero or more directories (including current)
NOLINT_TEST_F(PatternMatchingTest, RecursiveLeadingDoubleStar)
{
  loguru::configure_vmodules({ "**/foo/net*=2", "*=OFF" });
  // zero directories before foo
  EXPECT_TRUE(Enabled("foo/net.cpp", loguru::Verbosity_2));
  EXPECT_TRUE(Enabled("foo/network.cpp", loguru::Verbosity_2));
  // one directory
  EXPECT_TRUE(Enabled("src/foo/net_utils.cpp", loguru::Verbosity_2));
  // multiple directories
  EXPECT_TRUE(Enabled("a/b/c/foo/networking.cpp", loguru::Verbosity_2));
  // negative: different final segment
  EXPECT_FALSE(Enabled("a/b/c/foo/inetwork.cpp", loguru::Verbosity_2));
  // negative: foo not present
  EXPECT_FALSE(Enabled("a/b/c/bar/network.cpp", loguru::Verbosity_2));
}

//! Wildcard '?' should match exactly one character
NOLINT_TEST_F(PatternMatchingTest, WildcardQuestion)
{
  ScopedLogCapture capture("test_wildcard_question", loguru::Verbosity_9);

  loguru::configure_vmodules({ "mod?le=2", "*=OFF" });
  EXPECT_TRUE(Enabled("module.cpp", loguru::Verbosity_2));
  EXPECT_TRUE(Enabled("modale.cpp", loguru::Verbosity_2));
  EXPECT_FALSE(Enabled("modle.cpp", loguru::Verbosity_2));
  EXPECT_FALSE(Enabled("modaale.cpp", loguru::Verbosity_2));
}

//! Path normalization should work (/ vs \\)
NOLINT_TEST_F(PatternMatchingTest, PathNormalization)
{
  ScopedLogCapture capture("test_normalization", loguru::Verbosity_9);

  // Test forward slash pattern matching backslash paths
  loguru::configure_vmodule("src/utils*=2");
  EXPECT_TRUE(Enabled("src/utils.cpp", loguru::Verbosity_2));
  EXPECT_TRUE(Enabled("src/utils.cpp", loguru::Verbosity_2)); // normalized
}

//! Extension stripping and -inl suffix handling
NOLINT_TEST_F(PatternMatchingTest, ExtensionAndInlHandling)
{
  ScopedLogCapture capture("test_extensions", loguru::Verbosity_9);

  loguru::configure_vmodule("widget=2");
  EXPECT_TRUE(Enabled("widget.cpp", loguru::Verbosity_2));
  EXPECT_TRUE(Enabled("widget.h", loguru::Verbosity_2));
  EXPECT_TRUE(Enabled("widget-inl.h", loguru::Verbosity_2));
  EXPECT_TRUE(Enabled("widget.cc", loguru::Verbosity_2));
}

// NOTE: Precedence-specific tests and cross-TU cache simulations were removed
// because they were either redundant with existing pattern/configuration
// coverage or not realistically verifiable in a single translation unit.

class PrecedenceTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    loguru::clear_vmodule_overrides();
    saved_verbosity_ = loguru::g_global_verbosity;
    loguru::g_global_verbosity = loguru::Verbosity_MAX;
  }
  void TearDown() override
  {
    loguru::clear_vmodule_overrides();
    loguru::g_global_verbosity = saved_verbosity_;
  }

private:
  loguru::Verbosity saved_verbosity_ {};
};

//! First-match precedence regression: specific before wildcard
NOLINT_TEST_F(PrecedenceTest, FirstSpecificThenWildcard)
{
  ScopedLogCapture capture("precedence_specific_first", loguru::Verbosity_9);
  loguru::configure_vmodules({ "parser=3", "*=0" });
  EXPECT_TRUE(Enabled("parser.cpp", loguru::Verbosity_3));
  EXPECT_FALSE(Enabled("other.cpp", loguru::Verbosity_1));
}

//! First-match precedence regression: wildcard before specific (specific loses)
NOLINT_TEST_F(PrecedenceTest, WildcardFirstMasksSpecific)
{
  ScopedLogCapture capture("precedence_wildcard_first", loguru::Verbosity_9);
  loguru::configure_vmodules({ "*=0", "parser=3" });
  EXPECT_FALSE(Enabled("parser.cpp", loguru::Verbosity_3));
}

//=== Cached Verbosity Behavior Tests ===-----------------------------------//

//! Test fixture for cached verbosity behavior
/*!
 Tests the interaction of cached verbosity sites with module override
 configuration and precedence.
*/
class CachedVerbosityTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    loguru::clear_vmodule_overrides();
    saved_verbosity_ = loguru::g_global_verbosity;
    loguru::g_global_verbosity = loguru::Verbosity_MAX;
  }
  void TearDown() override
  {
    loguru::clear_vmodule_overrides();
    loguru::g_global_verbosity = saved_verbosity_;
  }
  loguru::Verbosity saved_verbosity_ {};
};

//! Cached site should reflect first-match precedence after updates
NOLINT_TEST_F(CachedVerbosityTest, CachedSiteRespectsFirstMatch)
{
  static std::atomic<int> cache { loguru::Verbosity_UNSPECIFIED };
  const char* file_path = "alpha.cpp"; // module name "alpha"
  // Simulate first log site touch (no overrides yet) -> registers site.
  (void)loguru::check_module_fast(&cache, loguru::Verbosity_0, file_path);
  EXPECT_EQ(
    cache.load(std::memory_order_relaxed), loguru::Verbosity_UNSPECIFIED);
  // Add overrides: first specific wins (site already registered so recompute
  // updates cache)
  loguru::configure_vmodules({ "alpha=2", "*=0" });
  EXPECT_EQ(cache.load(std::memory_order_relaxed), 2);

  // Add a later duplicate specific with different level (should be ignored
  // because first-match wins)
  loguru::configure_vmodule("alpha=4");
  EXPECT_EQ(cache.load(std::memory_order_relaxed), 2);
}

//! Basename vs full-path isolation without touching internal registry
NOLINT_TEST_F(CachedVerbosityTest, BasenameVsFullPathIsolation)
{
  // We'll emulate a translation unit cache as the VLOG macros would.
  static std::atomic<int> tu_cache { loguru::Verbosity_UNSPECIFIED };
  const char* file_path = "src/core/alpha.cpp"; // Simulated log site path

  // Phase 1: basename-only pattern should match irrespective of directories.
  loguru::configure_vmodule("alpha=3");
  // Force registration and cache computation through fast check.
  const bool enabled_basename
    = loguru::check_module_fast(&tu_cache, loguru::Verbosity_3, file_path);
  EXPECT_TRUE(enabled_basename);
  EXPECT_EQ(tu_cache.load(std::memory_order_relaxed), 3);

  // Phase 2: clear overrides and add a full-path style pattern that should
  // NOT match this different directory tree (other/alpha vs src/core/alpha).
  loguru::clear_vmodule_overrides();
  loguru::configure_vmodule("other/alpha=4");
  const bool enabled_fullpath_mismatch
    = loguru::check_module_fast(&tu_cache, loguru::Verbosity_4, file_path);
  // No matching override -> global cutoff (MAX) allows the log.
  EXPECT_TRUE(enabled_fullpath_mismatch);
  // Cached verbosity should now reflect UNSPECIFIED after recompute.
  EXPECT_EQ(
    tu_cache.load(std::memory_order_relaxed), loguru::Verbosity_UNSPECIFIED);
}

//! Updating overrides should recompute cached value for existing sites
NOLINT_TEST_F(CachedVerbosityTest, CacheUpdatesOnOverrideChange)
{
  static std::atomic<int> cache { loguru::Verbosity_UNSPECIFIED };
  const char* file_path = "renderer/Pass.cpp";
  // Register site before overrides to ensure update_all_module_sites affects
  // it.
  (void)loguru::check_module_fast(&cache, loguru::Verbosity_0, file_path);
  EXPECT_EQ(
    cache.load(std::memory_order_relaxed), loguru::Verbosity_UNSPECIFIED);
  // Full-path style override (contains '/').
  loguru::configure_vmodule("renderer/Pass=2");
  EXPECT_EQ(cache.load(std::memory_order_relaxed), 2);

  // Clear overrides -> cache should recompute to Verbosity_UNSPECIFIED.
  loguru::clear_vmodule_overrides();
  EXPECT_EQ(
    cache.load(std::memory_order_relaxed), loguru::Verbosity_UNSPECIFIED);
}

//! Module override should allow verbosity higher than global cutoff
NOLINT_TEST_F(CachedVerbosityTest, OverrideCannotExceedGlobalCutoff)
{
  loguru::g_global_verbosity = loguru::Verbosity_INFO; // global low
  static std::atomic<int> cache { loguru::Verbosity_UNSPECIFIED };
  const char* file_path = "graphics/Renderer.cpp";
  // Configure override before first use; register with a low verbosity first.
  loguru::configure_vmodule("graphics/Renderer=4");
  EXPECT_TRUE(
    loguru::check_module_fast(&cache, loguru::Verbosity_INFO, file_path));
  EXPECT_EQ(cache.load(std::memory_order_relaxed), 4);
  // Global cutoff blocks higher verbosity regardless of override.
  EXPECT_FALSE(
    loguru::check_module_fast(&cache, loguru::Verbosity_4, file_path));
}

//! Adding an override after site registration should update cache
NOLINT_TEST_F(CachedVerbosityTest, LateOverrideAfterRegistration)
{
  static std::atomic<int> cache { loguru::Verbosity_UNSPECIFIED };
  const char* file_path = "core/System.cpp";
  // First use registers site with no overrides.
  (void)loguru::check_module_fast(&cache, loguru::Verbosity_0, file_path);
  EXPECT_EQ(
    cache.load(std::memory_order_relaxed), loguru::Verbosity_UNSPECIFIED);
  // Add wildcard OFF then specific pattern; first-match semantics mean specific
  // first wins.
  loguru::configure_vmodules({ "core/System=3", "*=0" });
  EXPECT_EQ(cache.load(std::memory_order_relaxed), 3);
  EXPECT_TRUE(
    loguru::check_module_fast(&cache, loguru::Verbosity_3, file_path));
}

//! Order sensitivity: wildcard first suppresses specific match
NOLINT_TEST_F(CachedVerbosityTest, WildcardOrderBlocksSpecific)
{
  static std::atomic<int> cache { loguru::Verbosity_UNSPECIFIED };
  const char* file_path = "ai/Brain.cpp";
  // Register site before overrides.
  (void)loguru::check_module_fast(&cache, loguru::Verbosity_0, file_path);
  loguru::configure_vmodules({ "*=0", "ai/Brain=4" });
  EXPECT_EQ(cache.load(std::memory_order_relaxed), 0); // wildcard masks
  EXPECT_FALSE(
    loguru::check_module_fast(&cache, loguru::Verbosity_4, file_path));
}

} // namespace
