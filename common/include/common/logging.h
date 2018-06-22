//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#ifndef BLOCXXI_COMMON_LOGGING_H_
#define BLOCXXI_COMMON_LOGGING_H_

#include <string>  // for std::string
#include <stack>   // for stacking sinks
#include <thread>  // for std::mutex

#include <common/non_copiable.h>
#include <spdlog/fmt/ostr.h>  // for user defined objects logging
#include <spdlog/spdlog.h>

namespace blocxxi {
namespace logging {

/*!
 * @brief Exhaustive list of logger ids that can be used in declaring
 * Loggable<ID> classes.
 *
 * Apart from this use case, the rest of the logging macros only use
 * logger names.
 */
enum class Id {
  MISC,
  TESTING,
  COMMON,
  CODEC,
  CRYPTO,
  NAT,
  P2P,
  P2P_KADEMLIA,
  NDAGENT,
  INVALID_  // MUST BE THE LAST ONE
};

/**
 * @brief Logger wrapper for a spdlog logger.
 */
class Logger : private blocxxi::NonCopiable {
 public:
  /* This is simple mapping between Logger severity levels and spdlog severity
   * levels. The only reason for this mapping is to go around the fact that
   * spdlog defines level as err but the method to log at err level is called
   * LOGGER.error not LOGGER.err. All other level are fine spdlog::info
   * corresponds to LOGGER.info method.
   */
  enum class Level {
    trace = spdlog::level::trace,
    debug = spdlog::level::debug,
    info = spdlog::level::info,
    warn = spdlog::level::warn,
    error = spdlog::level::err,
    critical = spdlog::level::critical,
    off = spdlog::level::off
  };

  Logger(Logger &&other) noexcept
      : logger_(std::move(other.logger_)),
        logger_mutex_(std::move(other.logger_mutex_)){};

  Logger &operator=(Logger &&other) noexcept {
    Logger(std::move(other)).swap(*this);
    return *this;
  };

  ~Logger() = default;

  void swap(Logger &other) { std::swap(logger_, other.logger_); }

  const std::string &Name() const { return logger_->name(); }

  void setLevel(spdlog::level::level_enum level) { logger_->set_level(level); }

  static const char *DEFAULT_LOG_FORMAT;

 private:
  explicit Logger(std::string name, spdlog::sink_ptr sink);

  std::shared_ptr<spdlog::logger> logger_;
  std::unique_ptr<std::mutex> logger_mutex_ = std::make_unique<std::mutex>();
  friend class Registry;
};


class DelegatingSink : public spdlog::sinks::base_sink<std::mutex>, private NonCopiable {
public:
	DelegatingSink(spdlog::sink_ptr delegate) : sink_delegate_(delegate) {
	}
	DelegatingSink(DelegatingSink &&) = default;
	DelegatingSink& operator=(DelegatingSink &&) = default;
	DelegatingSink() = default;

	spdlog::sink_ptr SwapSink(spdlog::sink_ptr sink) {
		auto tmp = sink_delegate_; 
		sink_delegate_ = sink; 
		return tmp; 
	}

protected:
	void _sink_it(const spdlog::details::log_msg& msg) override {
		sink_delegate_->log(msg);
	}

	void _flush() override {
		sink_delegate_->flush();
	}

private:
	spdlog::sink_ptr sink_delegate_;
};


/*!
 * @brief A registry of all named loggers. Usable for adjusting levels of each
 * logger individually.
 *
 * TODO: Add Init() method with format, sinks and level
 */
class Registry {
 public:
  /*!
   * @brief Sets the minimum log severity required to print messages.
   * Messages below this loglevel will be suppressed.
   */
  static void SetLogLevel(spdlog::level::level_enum log_level);

  /**
   * Sets the log format.
   */
  static void SetLogFormat(const std::string &log_format);

  static spdlog::logger &GetLogger(Id id);

  static void PushSink(spdlog::sink_ptr sink);

  static void PopSink();

 private:
  /**
   * @return std::vector<Logger>& the installed loggers.
   */
  static std::vector<Logger> &Loggers();
  static std::vector<Logger> &all_loggers_();
  static std::recursive_mutex loggers_mutex_;

  static std::stack<spdlog::sink_ptr> &sinks_();
  static std::mutex sinks_mutex_;

  /// The deleagting sink used for all loggers.
  static DelegatingSink *delegating_sink_();
  static std::shared_ptr<DelegatingSink> &delegating_sink();
};

/*!
 * @brief Mixin class that allows any class to peform logging with a logger of a
 * particular ID.
 */
template <Id ID>
class Loggable {
 protected:
  /*!
   * @brief Do not use this directly, use macros defined below.
   * @return spdlog::logger& the static log instance to use for class local
   * logging.
   */
  static spdlog::logger &__log_do_not_use_read_comment() {
    static spdlog::logger &instance = Registry::GetLogger(ID);
    return instance;
  }
};

// Convert the line macro to a string literal for concatenation in log macros.
#define DO_STRINGIZE(x) STRINGIZE(x)
#define STRINGIZE(x) #x
#define LINE_STRING DO_STRINGIZE(__LINE__)
#ifndef NDEBUG
std::string FormatFileAndLine(char const *file, char const *line);
//#define LOG_PREFIX "[" __FILE__ ":" LINE_STRING "] "
//#define LOG_PREFIX " "
#define LOG_PREFIX blocxxi::logging::FormatFileAndLine(__FILE__, LINE_STRING)
#else
#define LOG_PREFIX " "
#endif  // NDEBUG

/// @name Logging macros
//@{
/*!
 * Base logging macros. It is expected that users will use the
 * convenience macros below rather than invoke these directly.
 */

#define BXLOG_COMP_LEVEL(LOGGER, LEVEL)    \
  (static_cast<spdlog::level::level_enum>( \
       blocxxi::logging::Logger::Level::LEVEL) >= LOGGER.level())

// Compare levels before invoking logger. This is an optimization to avoid
// executing expressions computing log contents when they would be suppressed.
// The same filtering will also occur in spdlog::logger.

#define _SELECT(PREFIX, _13, _12, _11, _10, _9, _8, _7, _6, _5, _4, _3, _2, \
                _1, SUFFIX, ...)                                            \
  PREFIX##_##SUFFIX
#define _SELECT_IMPL(args) _SELECT args
#define BX_DO_LOG(...)                                                       \
  _SELECT_IMPL((_BXLOG, __VA_ARGS__, N, N, N, N, N, N, N, N, N, N, 3, 2, 1)) \
  (__VA_ARGS__)

#define _BXLOG_1(LOGGER) LOGGER.debug("no logger level - no message")
#define _BXLOG_2(LOGGER, LEVEL) LOGGER.LEVEL("no message")
#define _BXLOG_3(LOGGER, LEVEL, MSG) LOGGER.LEVEL("{}" MSG, LOG_PREFIX)
#define _BXLOG_N(LOGGER, LEVEL, MSG, ...) \
  LOGGER.LEVEL("{}" MSG, LOG_PREFIX, __VA_ARGS__)

#ifndef NDEBUG
#define BXLOG_COMP_AND_LOG(LOGGER, LEVEL, ...) \
  do {                                         \
    if (BXLOG_COMP_LEVEL(LOGGER, LEVEL)) {     \
      BX_DO_LOG(LOGGER, LEVEL, __VA_ARGS__);   \
    }                                          \
  } while (0)
#else  // NDEBUG
#define BXLOG_COMP_AND_LOG(LOGGER, LEVEL, ...) \
  do {                                         \
    if (BXLOG_COMP_LEVEL(LOGGER, LEVEL)) {     \
      LOGGER.LEVEL(__VA_ARGS__);               \
    }                                          \
  } while (0)
#endif  // NDEBUG

#define BXLOG_CHECK_LEVEL(LEVEL) BXLOG_COMP_LEVEL(BXLOGGER(), LEVEL)

/**
 * Convenience macro to log to a user-specified logger.
 * Maps directly to BXLOG_COMP_AND_LOG - it could contain macro logic itself,
 * without redirection, but left in case various implementations are required in
 * the future (based on log level for example).
 */
#define BXLOG_TO_LOGGER(LOGGER, LEVEL, ...) \
  BXLOG_COMP_AND_LOG(LOGGER, LEVEL, __VA_ARGS__)

/**
 * Convenience macro to get logger.
 */
#define BXLOGGER() __log_do_not_use_read_comment()

/**
 * Convenience macro to flush logger.
 */
#define BXFLUSH_LOG() BXLOGGER().flush()

/**
 * Convenience macro to log to the class' logger.
 */
#define BXLOG(LEVEL, ...) BXLOG_TO_LOGGER(BXLOGGER(), LEVEL, __VA_ARGS__)

/**
 * Convenience macro to log to the misc logger, which allows for logging without
 * of direct access to a logger.
 */
#define GET_MISC_LOGGER() \
  blocxxi::logging::Registry::GetLogger(blocxxi::logging::Id::MISC)
#define BXLOG_MISC(LEVEL, ...) \
  BXLOG_TO_LOGGER(GET_MISC_LOGGER(), LEVEL, __VA_ARGS__)

//@}

}  // namespace logging
}  // namespace blocxxi

#endif  // BLOCXXI_COMMON_LOGGING_H_
