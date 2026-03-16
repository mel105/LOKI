#include "loki/core/logger.hpp"

#include "loki/core/exceptions.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace loki;

namespace loki {

// -----------------------------------------------------------------------------
//  Static member definition
// -----------------------------------------------------------------------------

std::unique_ptr<ILogger> Logger::s_customInstance{nullptr};

// -----------------------------------------------------------------------------
//  Singleton access
// -----------------------------------------------------------------------------

ILogger& Logger::instance()
{
    if (s_customInstance) {
        return *s_customInstance;
    }
    return defaultInstance();
}

Logger& Logger::defaultInstance()
{
    static Logger singleton;
    return singleton;
}

void Logger::setInstance(std::unique_ptr<ILogger> customInstance)
{
    s_customInstance = std::move(customInstance);
}

// -----------------------------------------------------------------------------
//  Initialisation
// -----------------------------------------------------------------------------

void Logger::initDefault(const std::filesystem::path& logDir,
                          const std::string&           prefix,
                          LogLevel                     minLevel)
{
    defaultInstance().init(logDir, prefix, minLevel);
}

void Logger::init(const std::filesystem::path& logDir,
                  const std::string&           prefix,
                  LogLevel                     minLevel)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_minLevel = minLevel;

    std::error_code ec;
    std::filesystem::create_directories(logDir, ec);
    if (ec) {
        throw IoException("Logger::init: cannot create log directory '"
                          + logDir.string() + "': " + ec.message());
    }

    const std::string filename = prefix + "_" + _filenameTimestamp() + ".log";
    m_filePath = logDir / filename;

    m_file.open(m_filePath, std::ios::out | std::ios::trunc);
    if (!m_file.is_open()) {
        throw IoException("Logger::init: cannot open log file '"
                          + m_filePath.string() + "'.");
    }
}

// -----------------------------------------------------------------------------
//  Logging methods
// -----------------------------------------------------------------------------

void Logger::log(LogLevel         level,
                 std::string_view caller,
                 std::string_view message)
{
    if (level < m_minLevel) {
        return;
    }

    std::ostringstream line;
    line << '[' << _currentTimestamp() << ']'
         << " [" << _levelString(level) << ']'
         << " [" << std::left << std::setw(22) << caller << ']'
         << ' ' << message;

    const std::string formatted = line.str();

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_file.is_open()) {
        m_file << formatted << '\n';
        m_file.flush();
    }

    // WARNING and ERROR go to stderr, the rest to stdout.
    if (level >= LogLevel::WARNING) {
        std::cerr << formatted << '\n';
    } else {
        std::cout << formatted << '\n';
    }
}

void Logger::debug  (std::string_view caller, std::string_view message) { log(LogLevel::DEBUG,   caller, message); }
void Logger::info   (std::string_view caller, std::string_view message) { log(LogLevel::INFO,    caller, message); }
void Logger::warning(std::string_view caller, std::string_view message) { log(LogLevel::WARNING, caller, message); }
void Logger::error  (std::string_view caller, std::string_view message) { log(LogLevel::ERROR,   caller, message); }

// -----------------------------------------------------------------------------
//  Log file path
// -----------------------------------------------------------------------------

std::filesystem::path Logger::logFilePath() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_filePath;
}

// -----------------------------------------------------------------------------
//  Formatting helpers
// -----------------------------------------------------------------------------

std::string Logger::_currentTimestamp()
{
    const auto now   = std::chrono::system_clock::now();
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now.time_since_epoch()) % 1000;
    const std::time_t t = std::chrono::system_clock::to_time_t(now);

    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << nowMs.count();
    return oss.str();
}

std::string Logger::_filenameTimestamp()
{
    const auto        now = std::chrono::system_clock::now();
    const std::time_t t   = std::chrono::system_clock::to_time_t(now);

    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return oss.str();
}

std::string_view Logger::_levelString(LogLevel level)
{
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG  ";
        case LogLevel::INFO:    return "INFO   ";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR  ";
    }
    return "UNKNOWN";
}

} // namespace loki
