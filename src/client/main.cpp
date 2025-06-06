#include "client/client.hpp"
#include "common/logging.hpp"
#include <argparse/argparse.hpp>
#include <iostream>
#include <stdexcept>

/**
 * Set up command line argument parser with all available options
 */
void setup_argument_parser(argparse::ArgumentParser &program)
{
    program.add_argument("--host", "-H")
        .help("Server hostname or IP address")
        .default_value(std::string("127.0.0.1"));

    program.add_argument("--port", "-p")
        .help("Server port")
        .default_value(std::string("5555"));

    program.add_argument("--log-level")
        .help("Logging level (trace, debug, info, warn, error, critical)")
        .default_value(std::string("info"));

    program.add_argument("--log-file")
        .help("Path to log file")
        .default_value(std::string("fenris_client.log"));

    program.add_argument("--no-console-log")
        .help("Disable logging to console")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("--file-log")
        .help("Enable logging to file")
        .default_value(false)
        .implicit_value(true);
}

/**
 * Parse arguments and handle parsing errors
 */
bool parse_arguments(argparse::ArgumentParser &program, int argc, char *argv[])
{
    try {
        program.parse_args(argc, argv);
        return true;
    } catch (const std::runtime_error &err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return false;
    }
}

std::unique_ptr<fenris::client::Client>
create_client(const argparse::ArgumentParser &program)
{
    auto client = std::make_unique<fenris::client::Client>("fenris_client");

    client->set_tui(std::make_unique<fenris::client::TUI>());

    std::string host = program.get("--host");
    std::string port = program.get("--port");

    std::unique_ptr<fenris::client::ConnectionManager> connection_manager;

    // Check if host is provided through command line arguments
    // If using default values, use the no-parameter constructor to prompt user
    // later
    bool is_host_from_args = program.is_used("--host");
    bool is_port_from_args = program.is_used("--port");

    if (is_host_from_args || is_port_from_args) {
        // At least one parameter was explicitly provided, use the constructor
        // with parameters
        connection_manager =
            std::make_unique<fenris::client::ConnectionManager>(
                host,
                port,
                "fenris_client_connection");
    } else {
        // No parameters were provided, use the default constructor
        connection_manager =
            std::make_unique<fenris::client::ConnectionManager>(
                "fenris_client_connection");
    }

    client->set_connection_manager(std::move(connection_manager));

    return client;
}

int main(int argc, char *argv[])
{
    argparse::ArgumentParser program("fenris_client");
    setup_argument_parser(program);

    if (!parse_arguments(program, argc, argv)) {
        return 1;
    }

    if (!fenris::common::configure_logging(program, "fenris_client")) {
        std::cerr << "Failed to initialize logging system" << std::endl;
        return 1;
    }

    auto logger = fenris::common::get_logger("fenris_client");

    try {
        auto client = create_client(program);
        client->run();
    } catch (const std::exception &e) {
        logger->error("exception occurred: {}", e.what());
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        logger->error("unknown exception occurred");
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }

    logger->info("fenris client shutting down");
    return 0;
}
