#include "common/logging.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <unordered_map>

namespace fenris {
namespace common {

namespace {
// Static map for logger instances
std::unordered_map<std::string, Logger> loggers;

// Static map for level to string conversion
const std::unordered_map<LogLevel, std::string> level_to_string_map = {
    {LogLevel::TRACE, "trace"},
    {LogLevel::DEBUG, "debug"},
    {LogLevel::INFO, "info"},
    {LogLevel::WARN, "warn"},
    {LogLevel::ERROR, "error"},
    {LogLevel::CRITICAL, "critical"},
    {LogLevel::OFF, "off"}};
} // namespace

bool initialize_logging(const LoggingConfig &config,
                        const std::string &logger_name)
{
    try {
        // Create sinks based on configuration
        std::vector<spdlog::sink_ptr> sinks;

        if (config.console_logging) {
            auto console_sink =
                std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(
                static_cast<spdlog::level::level_enum>(config.level));
            sinks.push_back(console_sink);
        }

        if (config.file_logging) {
            auto file_sink =
                std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    config.log_file_path,
                    config.max_file_size,
                    config.max_files);
            file_sink->set_level(
                static_cast<spdlog::level::level_enum>(config.level));
            sinks.push_back(file_sink);
        }

        // Create logger with all sinks
        auto logger = std::make_shared<spdlog::logger>(logger_name,
                                                       sinks.begin(),
                                                       sinks.end());
        logger->set_level(static_cast<spdlog::level::level_enum>(config.level));

        // Set pattern
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");

        // Register with spdlog and store in our map
        spdlog::register_logger(logger);
        loggers[logger_name] = logger;

        // Set as default logger if it's the "fenris" logger
        if (logger_name == "fenris") {
            spdlog::set_default_logger(logger);
        }

        return true;
    } catch (const spdlog::spdlog_ex &ex) {
        std::cerr << "Logging initialization failed: " << ex.what()
                  << std::endl;
        return false;
    }
}

/**
 * Configure and initialize logging system based on command line arguments
 */
bool configure_logging(const argparse::ArgumentParser &program)
{
    LoggingConfig logging_config;
    std::string log_level = program.get("--log-level");

    // Convert string log level to enum
    if (log_level == "trace") {
        logging_config.level = LogLevel::TRACE;
    } else if (log_level == "debug") {
        logging_config.level = LogLevel::DEBUG;
    } else if (log_level == "info") {
        logging_config.level = LogLevel::INFO;
    } else if (log_level == "warn") {
        logging_config.level = LogLevel::WARN;
    } else if (log_level == "error") {
        logging_config.level = LogLevel::ERROR;
    } else if (log_level == "critical") {
        logging_config.level = LogLevel::CRITICAL;
    } else if (log_level == "off") {
        logging_config.level = LogLevel::OFF;
    } else {
        std::cerr << "Invalid log level: " << log_level << std::endl;
        return false;
    }

    logging_config.console_logging = !program.get<bool>("--no-console-log");
    logging_config.file_logging = program.get<bool>("--file-log");
    logging_config.log_file_path = program.get("--log-file");

    return initialize_logging(logging_config, "fenris_client");
}

Logger get_logger(const std::string &logger_name)
{
    auto it = loggers.find(logger_name);
    if (it != loggers.end()) {
        return it->second;
    }

    // Return default logger if the requested one doesn't exist
    return spdlog::default_logger();
}

void set_log_level(LogLevel level)
{
    // Set level for all loggers
    for (auto &[name, logger] : loggers) {
        logger->set_level(static_cast<spdlog::level::level_enum>(level));
    }

    // Also set default level
    spdlog::set_level(static_cast<spdlog::level::level_enum>(level));
}

std::string log_level_to_string(LogLevel level)
{
    auto it = level_to_string_map.find(level);
    if (it != level_to_string_map.end()) {
        return it->second;
    }
    return "info"; // Default
}

} // namespace common
} // namespace fenris
