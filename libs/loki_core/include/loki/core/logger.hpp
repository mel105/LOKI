#pragma once

#include "loki/timeseries/timeStamp.hpp"
#include "loki/core/exceptions.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace loki {

// ─────────────────────────────────────────────────────────────────────────────
//  LogLevel
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Severity levels for log messages.
 *
 * Messages below the configured minimum level are silently discarded.
 * Levels in ascending order of severity: DEBUG < INFO < WARNING < ERROR.
 */
enum class LogLevel : int {
    DEBUG   = 0,
    INFO    = 1,
    WARNING = 2,
    ERROR   = 3,
};

// ─────────────────────────────────────────────────────────────────────────────
//  ILogger — abstract interface (enables mock injection in tests)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Abstract logger interface.
 *
 * Derive from this class to create test doubles (MockLogger) or alternative
 * logging back-ends without modifying call sites.
 */
class ILogger {
public:
    virtual ~ILogger() = default;

    virtual void log(LogLevel level,
                     std::string_view caller,
                     std::string_view message) = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Logger — singleton, writes to file and stdout simultaneously
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Thread-safe singleton logger that writes to a file and stdout.
 *
 * ### Initialisation
 * Call init() once at application startup before any log calls:
 * @code
 *   namespace fs = std::filesystem;
 *   Logger::instance().init(fs::path("/workspace/OUTPUT/LOG"), "loki_homogeneity");
 *   // Creates: /workspace/OUTPUT/LOG/loki_homogeneity_20260313_142301.log
 * @endcode
 *
 * ### Usage via macros (recommended)
 * @code
 *   LOKI_INFO("Opened file: " + path.string());
 *   LOKI_WARNING("Line 42: unexpected column count, skipping.");
 *   LOKI_ERROR("File not found: " + path.string());
 * @endcode
 *
 * ### Test injection
 * Replace the active instance with a mock before running tests:
 * @code
 *   Logger::setInstance(std::make_unique<MockLogger>());
 * @endcode
 * Call setInstance(nullptr) to restore the default logger.
 *
 * ### Log format
 * @code
 *   [2026-03-13 14:23:01.123] [INFO   ] [Loader::load          ] Opened: sensor.csv
 *   [2026-03-13 14:23:01.125] [WARNING] [Loader::load          ] Line 42: bad columns.
 * @endcode
 */
class Logger : public ILogger {
public:

    // ── Singleton access ──────────────────────────────────────────────────────

    /**
     * @brief Returns the active logger instance.
     *
     * If a custom instance has been set via setInstance(), returns that.
     * Otherwise returns the default Logger singleton.
     */
    static ILogger& instance();

    /**
     * @brief Replaces the active logger with a custom implementation.
     *
     * Pass nullptr to restore the default Logger singleton.
     * Primarily intended for unit test injection.
     *
     * @param customInstance Owning pointer to the replacement logger, or nullptr.
     */
    static void setInstance(std::unique_ptr<ILogger> customInstance);

    // ── Initialisation ────────────────────────────────────────────────────────

    /**
     * @brief Opens the log file and sets the minimum log level.
     *
     * The log file name is constructed as:
     *   <logDir>/<prefix>_<YYYYMMDD_HHMMSS>.log
     *
     * The log directory is created if it does not exist.
     * If init() is not called, messages are written only to stdout.
     *
     * @param logDir   Directory where the log file will be created.
     * @param prefix   File name prefix (e.g. "loki_homogeneity", "test_loader").
     * @param minLevel Minimum severity to record; default INFO.
     * @throws IoException if the log file cannot be opened.
     */
    /**
     * @brief Static convenience accessor — initialises the default Logger singleton.
     *
     * Equivalent to: static_cast<Logger&>(Logger::instance()).init(...)
     * Avoids the need for a cast at call sites.
     */
    static void initDefault(const std::filesystem::path& logDir,
                             const std::string&           prefix,
                             LogLevel                     minLevel = LogLevel::INFO);

    void init(const std::filesystem::path& logDir,
              const std::string&           prefix,
              LogLevel                     minLevel = LogLevel::INFO);

    // ── Logging methods ───────────────────────────────────────────────────────

    /**
     * @brief Writes a message at the specified level.
     * @param level   Message severity.
     * @param caller  Name of the calling function or class (shown in the log).
     * @param message Log message text.
     */
    void log(LogLevel level,
             std::string_view caller,
             std::string_view message) override;

    /// @brief Convenience wrapper for DEBUG messages.
    void debug  (std::string_view caller, std::string_view message);

    /// @brief Convenience wrapper for INFO messages.
    void info   (std::string_view caller, std::string_view message);

    /// @brief Convenience wrapper for WARNING messages.
    void warning(std::string_view caller, std::string_view message);

    /// @brief Convenience wrapper for ERROR messages.
    void error  (std::string_view caller, std::string_view message);

    // ── Log file path ─────────────────────────────────────────────────────────

    /**
     * @brief Returns the full path of the currently open log file.
     *
     * Returns an empty path if init() has not been called.
     */
    [[nodiscard]] std::filesystem::path logFilePath() const;

private:

    // Private constructor — instantiated only by instance().
    Logger() = default;

    /// Returns the default singleton (created on first call).
    static Logger& defaultInstance();

    // ── Formatting helpers ────────────────────────────────────────────────────

    /// @brief Formats the current wall-clock time as "YYYY-MM-DD hh:mm:ss.mmm".
    static std::string _currentTimestamp();

    /// @brief Formats the current wall-clock time as "YYYYMMDD_HHMMSS" for filenames.
    static std::string _filenameTimestamp();

    /// @brief Returns the fixed-width string representation of a LogLevel.
    static std::string_view _levelString(LogLevel level);

    // ── State ─────────────────────────────────────────────────────────────────

    std::ofstream            m_file;
    std::filesystem::path    m_filePath;
    LogLevel                 m_minLevel{LogLevel::INFO};
    mutable std::mutex       m_mutex;

    /// Active instance pointer — nullptr means "use defaultInstance()".
    static std::unique_ptr<ILogger> s_customInstance;
};

} // namespace loki

// ─────────────────────────────────────────────────────────────────────────────
//  Convenience macros
//
//  __func__ is standard C++11 and gives the unqualified function name.
//  For a fully-qualified name (ClassName::method) callers may pass it manually.
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Logs a DEBUG message. Caller name is filled from __func__ automatically.
#define LOKI_DEBUG(msg)   ::loki::Logger::instance().log( \
    ::loki::LogLevel::DEBUG,   __func__, (msg))

/// @brief Logs an INFO message. Caller name is filled from __func__ automatically.
#define LOKI_INFO(msg)    ::loki::Logger::instance().log( \
    ::loki::LogLevel::INFO,    __func__, (msg))

/// @brief Logs a WARNING message. Caller name is filled from __func__ automatically.
#define LOKI_WARNING(msg) ::loki::Logger::instance().log( \
    ::loki::LogLevel::WARNING, __func__, (msg))

/// @brief Logs an ERROR message. Caller name is filled from __func__ automatically.
#define LOKI_ERROR(msg)   ::loki::Logger::instance().log( \
    ::loki::LogLevel::ERROR,   __func__, (msg))
