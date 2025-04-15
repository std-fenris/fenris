#include "common/logging.hpp"
#include <iostream>

int main(int argc, char *argv[])
{
    fenris::common::LoggingConfig config;
    config.level = fenris::common::LogLevel::INFO;
    config.file_logging = true;
    config.log_file_path = "fenris_server.log";

    if (!fenris::common::initialize_logging(config, "fenris_server")) {
        std::cerr << "Failed to initialize logging system" << std::endl;
        return 1;
    }

    auto logger = fenris::common::get_logger("fenris_server");

    logger->info("Fenris server starting up");
    logger->info("Fenris server shutting down");
    return 0;
}
