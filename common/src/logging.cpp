//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <common/logging.h>

#include <sstream>     // std::ostringstream
#include <iomanip>      // std::setw

namespace blocxxi {
namespace logging {

//TODO: restore default logging config
//const char *Logger::DEFAULT_LOG_FORMAT =
//    "[%Y-%m-%d %T.%e] [%t] [%^%l%$] [%n] %v";
const char *Logger::DEFAULT_LOG_FORMAT =
    "[%t] [%^%L%$] [%n] %v";

std::mutex Registry::sinks_mutex_;
std::recursive_mutex Registry::loggers_mutex_;

Logger::Logger(const std::string &name) {
  auto sinks = Registry::Sinks();
  logger_ = std::make_shared<spdlog::logger>(name, std::begin(sinks),
                                             std::end(sinks));
  logger_->set_pattern(DEFAULT_LOG_FORMAT);
  logger_->set_level(spdlog::level::trace);
  // Ensure that critical errors, especially ASSERT/PANIC, get flushed
  logger_->flush_on(spdlog::level::critical);
}

namespace {

Id &operator++(Id &target) {
  target = static_cast<Id>(static_cast<int>(target) + 1);
  return target;
}

/// Get the corresponding logger name for a logger id.
inline constexpr const char *LoggerName(Id id) {
  switch (id) {
    // clang-format off
    case Id::MISC:          return "misc    ";
    case Id::TESTING:       return "testing ";
    case Id::COMMON:        return "common  ";
    case Id::CODEC:         return "codec   ";
    case Id::CRYPTO:        return "crypto  ";
    case Id::NAT:           return "nat     ";
    case Id::P2P:           return "p2p     ";
    case Id::P2P_KADEMLIA:  return "kademlia";
    case Id::NDAGENT:       return "ndagent ";
    case Id::INVALID_:      return "__DO_NOT_USE__";
      // clang-format on
      // omit default case to trigger compiler warning for missing cases
  };
  return "__INVALID__";
}

}  // namespace

spdlog::logger &Registry::GetLogger(Id id) {
  std::lock_guard<std::recursive_mutex> lock(loggers_mutex_);
  return *(Loggers()[static_cast<int>(id)].logger_);
}

void Registry::AddSink(spdlog::sink_ptr sink) {
  std::lock_guard<std::mutex> lock(sinks_mutex_);
  auto &sinks = sinks_();
  sinks.emplace_back(sink);
}

/**
 * Sets the minimum log severity required to print messages.
 * Messages below this loglevel will be suppressed.
 */
void Registry::SetLogLevel(spdlog::level::level_enum log_level) {
  std::lock_guard<std::recursive_mutex> lock(loggers_mutex_);
  auto &loggers = Loggers();
  std::for_each(loggers.begin(), loggers.end(), [log_level](Logger &log) {
    // Thread safe
    log.setLevel(log_level);
  });
}

/**
 * Sets the log format.
 */
void Registry::SetLogFormat(const std::string &log_format) {
  std::lock_guard<std::recursive_mutex> lock(loggers_mutex_);
  auto &loggers = Loggers();
  std::for_each(loggers.begin(), loggers.end(), [log_format](Logger &log) {
    // Not thread safe
    std::lock_guard<std::mutex> log_lock(*log.logger_mutex_.get());
    log.logger_->set_pattern(log_format);
  });
}

/**
 * @return std::vector<Logger>& the installed loggers.
 */
std::vector<Logger> &Registry::Loggers() {
  static auto &all_loggers_static = all_loggers_();
  return all_loggers_static;
}

std::vector<Logger> &Registry::all_loggers_() {
  auto sinks = Registry::Sinks();
  if (sinks.empty()) {
    // Add a default console sink
#if defined _WIN32 && !defined(__cplusplus_winrt)
    auto default_sink =
        std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>();
#else
    auto default_sink =
        std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
#endif
    AddSink(default_sink);
  }

  static auto *all_loggers = new std::vector<Logger>();
  for (auto id = Id::MISC; id < Id::INVALID_; ++id) {
    auto name = LoggerName(id);
    all_loggers->emplace_back(Logger(name));
  }
  return *all_loggers;
}

std::vector<spdlog::sink_ptr> Registry::Sinks() {
  static auto &sinks_static = sinks_();
  return sinks_static;
}

std::vector<spdlog::sink_ptr> &Registry::sinks_() {
  static auto *sinks = new std::vector<spdlog::sink_ptr>();
  return *sinks;
}

std::string FormatFileAndLine(char const *file, char const *line) {
  constexpr static int FILE_MAX_LENGTH = 70;
  std::ostringstream ostr;
  std::string fstr(file);
  if (fstr.length() > FILE_MAX_LENGTH) {
    fstr = fstr.substr(0, 7).append("...").append(fstr.substr(
        fstr.length() - FILE_MAX_LENGTH + 10, FILE_MAX_LENGTH - 10));
  }
  ostr << "[" << std::setw(FILE_MAX_LENGTH) << std::right << fstr << ":"
       << std::setw(5) << std::setfill('0') << std::right << line << "] ";
  return ostr.str();
}

}  // namespace logging
}  // namespace blocxxi