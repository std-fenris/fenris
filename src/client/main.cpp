#include "common/logging.hpp"
#include <iostream>

int main(int argc, char *argv[])
{
    fenris::common::LoggingConfig config;
    config.level = fenris::common::LogLevel::DEBUG;
    config.file_logging = true;
    config.log_file_path = "fenris_client.log";

    if (!fenris::common::initialize_logging(config, "fenris_client")) {
        std::cerr << "failed to initialize logging system" << std::endl;
        return 1;
    }

    auto logger = fenris::common::get_logger("fenris_client");

    logger->info("fenris client starting up");
    logger->info("fenris client shutting down");
    return 0;
}
