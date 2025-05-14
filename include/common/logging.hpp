#ifndef FENRIS_COMMON_LOGGING_HPP
#define FENRIS_COMMON_LOGGING_HPP

#include <argparse/argparse.hpp>
#include <memory>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace fenris {
namespace common {

using Logger = std::shared_ptr<spdlog::logger>;

/**
 * Enum that mirrors spdlog::level::level_enum but is part of our API
 */
enum class LogLevel {
    TRACE = spdlog::level::trace,
    DEBUG = spdlog::level::debug,
    INFO = spdlog::level::info,
    WARN = spdlog::level::warn,
    ERROR = spdlog::level::err,
    CRITICAL = spdlog::level::critical,
    OFF = spdlog::level::off
};

/**
 * Logging configuration for the application
 */
struct LoggingConfig {
    LogLevel level = LogLevel::INFO; // Global log level
    bool console_logging = true;     // Whether to log to console
    bool file_logging = false;       // Whether to log to file
    std::string log_file_path =
        "fenris.log"; // Path to log file (only used if file_logging is true)
    size_t max_file_size = 1048576; // Maximum size of log file in bytes (1 MB)
    size_t max_files = 3;           // Maximum number of log files to keep
};

/**
 * Initialize the logging system
 *
 * @param config Logging configuration
 * @param logger_name Name of the logger
 * @return Whether initialization succeeded
 */
bool initialize_logging(const LoggingConfig &config,
                        const std::string &logger_name = "fenris");

/**
 * Configure and initialize logging based on command line arguments
 *
 * @param program Argument parser with command line arguments
 * @return Whether configuration succeeded
 */
bool configure_logging(const argparse::ArgumentParser &program,
                       const std::string &log_name = "fenris");

/**
 * Get the logger instance
 *
 * @param logger_name Name of the logger to get
 * @return Logger instance
 */
Logger get_logger(const std::string &logger_name = "fenris");

/**
 * Set the global log level
 *
 * @param level New log level
 */
void set_log_level(LogLevel level);

/**
 * Convert a LogLevel to string representation
 *
 * @param level Log level to convert
 * @return String representation of the log level
 */
std::string log_level_to_string(LogLevel level);

} // namespace common
} // namespace fenris

#endif // FENRIS_COMMON_LOGGING_HPP
