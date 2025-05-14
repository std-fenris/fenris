#include "common/logging.hpp"
#include "server/request_manager.hpp"
#include "server/server.hpp"
#include <argparse/argparse.hpp>
#include <atomic>
#include <csignal>
#include <iostream>
#include <stdexcept>

/**
 * Set up command line argument parser with all available options
 */
void setup_argument_parser(argparse::ArgumentParser &program)
{
    program.add_argument("--host", "-H")
        .help("Hostname or IP address to bind to")
        .default_value(std::string("0.0.0.0"));

    program.add_argument("--port", "-p")
        .help("Port to listen on")
        .default_value(std::string("5555"));

    program.add_argument("--log-level")
        .help("Logging level (trace, debug, info, warn, error, critical)")
        .default_value(std::string("info"));

    program.add_argument("--log-file")
        .help("Path to log file")
        .default_value(std::string("fenris_server.log"));

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

std::unique_ptr<fenris::server::Server>
create_server(const argparse::ArgumentParser &program)
{
    std::string host = program.get("--host");
    std::string port = program.get("--port");

    const std::string logger_name = "fenris_server";

    auto server =
        std::make_unique<fenris::server::Server>(host, port, logger_name);

    // Create and set a file handler for client requests - pass the logger name
    auto file_handler =
        std::make_unique<fenris::server::ClientHandler>(logger_name);
    server->set_client_handler(std::move(file_handler));

    return server;
}

int main(int argc, char *argv[])
{
    argparse::ArgumentParser program("fenris_server");
    setup_argument_parser(program);

    if (!parse_arguments(program, argc, argv)) {
        return 1;
    }

    if (!fenris::common::configure_logging(program, "fenris_server")) {
        std::cerr << "Failed to initialize logging system" << std::endl;
        return 1;
    }

    auto logger = fenris::common::get_logger("fenris_server");
    logger->info("Starting Fenris server with logging level: {}",
                 program.get("--log-level"));

    // Flag to track running state
    static std::atomic<bool> running{true};

    // Set up signal handler for graceful shutdown
    std::signal(SIGINT, [](int) {
        std::cout << "\nReceived interrupt signal, shutting down..."
                  << std::endl;
        running = false;
    });

    try {
        auto server = create_server(program);

        if (!server->start()) {
            logger->error("Failed to start server");
            return 1;
        }

        logger->info("Server started successfully on {}:{}",
                     program.get("--host"),
                     program.get("--port"));
        logger->info("Press Ctrl+C to stop the server");

        // Keep the main thread alive until interrupted
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Graceful shutdown
        logger->info("Stopping server...");
        server->stop();
        logger->info("Server stopped successfully");

    } catch (const std::exception &e) {
        logger->error("exception occurred: {}", e.what());
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        logger->error("unknown exception occurred");
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }

    logger->info("Fenris server shutting down");
    return 0;
}
