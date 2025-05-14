#include "client/interface.hpp"
#include "client/colors.hpp"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <unordered_map>

namespace fenris {
namespace client {

TUI::TUI() : curr_dir("/")
{
    // Initialize valid command prefixes
    valid_commands = {
        "cd",     // Change directory
        "ls",     // List directory
        "cat",    // Display file contents
        "upload", // Upload file
        "ping",   // Ping server
        "write",  // Write to file
        "append", // Append to file
        "rm",     // Remove file
        "info",   // Get file info
        "mkdir",  // Create directory
        "rmdir",  // Remove directory
        "help",   // Display help information
        "exit"    // Exit client
    };

    // Initialize command descriptions for help
    command_descriptions = {
        {"cd", "Change the current directory (cd <directory>)"},
        {"ls", "List contents of a directory (ls [directory])"},
        {"cat", "Display contents of a file (cat <file>)"},
        {"upload", "Upload a file to the server (upload <local_file>)"},
        {"ping", "Check if server is responsive (ping)"},
        {"write", "Create a new file with content (write <file> <content>)"},
        {"rm", "Remove a file (rm <file>)"},
        {"info", "Display file information (info <file>)"},
        {"mkdir", "Create a new directory (mkdir <directory>)"},
        {"cd", "Change the current directory (cd <directory>)"},
        {"rmdir", "Remove a directory (rmdir <directory>)"},
        {"help", "Display available commands (help)"},
        {"exit", "Exit the client (exit)"}};
}

std::string TUI::get_server_IP()
{
    std::string ip;
    std::cout << colors::BOLD << colors::CYAN
              << "Enter server IP address: " << colors::RESET;
    std::getline(std::cin, ip);

    // Use regex to validate IP format (IPv4 address validation)
    // This regex checks that each octet is between 0-255 and properly formatted
    static const std::regex ipv4_pattern(
        "^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)"
        "\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)"
        "\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)"
        "\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");

    // Also accept hostname format
    static const std::regex hostname_pattern(
        "^([a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?\\.)+[a-zA-Z]{2,}$");

    // Allow localhost as a special case
    if (ip == "localhost") {
        return "127.0.0.1";
    }

    if (ip.empty()) {
        ip = "127.0.0.1"; // Default to localhost
        std::cout << colors::info("Using default IP: " + ip) << std::endl;
    } else if (!std::regex_match(ip, ipv4_pattern) &&
               !std::regex_match(ip, hostname_pattern)) {
        std::cout << colors::warning("Invalid IP address or hostname format. "
                                     "Using default instead.")
                  << std::endl;
        ip = "127.0.0.1";
        std::cout << colors::info("Using default IP: " + ip) << std::endl;
    }

    return ip;
}

std::string TUI::get_port_number()
{
    std::string port;
    std::cout << colors::BOLD << colors::CYAN
              << "Enter server port number: " << colors::RESET;
    std::getline(std::cin, port);

    // Validate port number (should be a number between 1 and 65535)
    if (port.empty() || !std::all_of(port.begin(), port.end(), ::isdigit) ||
        std::stoi(port) < 1 || std::stoi(port) > 65535) {
        std::cout << colors::warning(
                         "Invalid port number. Using default port 7777.")
                  << std::endl;
        return "7777"; // Default port
    }

    return port;
}

std::vector<std::string> TUI::get_command()
{
    std::string input;
    std::vector<std::string> command_parts;

    // Display prompt with current directory
    std::cout << colors::CYAN << "fenris:" << colors::GREEN << curr_dir
              << colors::CYAN << "> " << colors::RESET;
    std::getline(std::cin, input);

    if (input.empty()) {
        return {};
    }

    std::istringstream iss(input);
    std::string part;

    while (iss >> part) {
        command_parts.push_back(part);
    }

    // Validate command if we have at least one part
    if (!command_parts.empty() && !validate_command(command_parts)) {
        return {};
    }

    if (is_prefix(command_parts[0], "help")) {
        display_help();
        return {};
    }

    return command_parts;
}

bool TUI::validate_command(const std::vector<std::string> &command_parts)
{
    if (command_parts.empty()) {
        return false;
    }

    const std::string &cmd = command_parts[0];
    const size_t arg_count = command_parts.size() - 1;

    // Verify the command is in our set of valid commands
    if (valid_commands.find(cmd) == valid_commands.end()) {
        std::cout << colors::error("Invalid command: " + cmd) << std::endl;
        return false;
    }

    static const std::unordered_map<std::string, std::pair<size_t, size_t>>
        command_args = {// command -> {min_args, max_args}
                        {"cd", {1, 1}},
                        {"ls", {0, 1}},
                        {"cat", {1, 1}},
                        {"upload", {1, 1}},
                        {"ping", {0, 0}},
                        {"write", {2, 2}},
                        {"append", {2, 2}},
                        {"rm", {1, 1}},
                        {"info", {1, 1}},
                        {"mkdir", {1, 1}},
                        {"cd", {1, 1}},
                        {"rmdir", {1, 1}},
                        {"help", {0, 0}},
                        {"exit", {0, 0}}};

    auto it = command_args.find(cmd);
    if (it != command_args.end()) {
        const auto &[min_args, max_args] = it->second;

        if (arg_count < min_args || arg_count > max_args) {
            if (min_args == max_args) {
                std::cout << colors::error(
                                 "Error: " + cmd + " requires exactly " +
                                 std::to_string(min_args) + " argument" +
                                 (min_args != 1 ? "s" : ""))
                          << std::endl;
            } else {
                std::cout << colors::error(
                                 "Error: " + cmd + " requires between " +
                                 std::to_string(min_args) + " and " +
                                 std::to_string(max_args) + " arguments")
                          << std::endl;
            }
            return false;
        }
    }

    return true;
}

bool TUI::is_prefix(const std::string &str, const std::string &prefix)
{
    return str == prefix;
}

void TUI::display_result(bool success, const std::string &result)
{
    if (success) {
        if (result.empty()) {
            std::cout << colors::success("Command completed successfully.")
                      << std::endl;
        } else {
            // Check if the result is already colorized (contains color codes)
            if (result.find("\033[") != std::string::npos) {
                std::cout << result << std::endl;
            } else {
                std::cout << colors::info(result) << std::endl;
            }
        }
    } else {
        // Check if the result is already colorized or starts with "Error:"
        if (result.find("\033[") != std::string::npos) {
            std::cout << result << std::endl;
        } else if (result.find("Error:") == 0) {
            std::cout << colors::error(result) << std::endl;
        } else {
            std::cout << colors::error(result) << std::endl;
        }
    }
}

void TUI::update_current_directory(const std::string &new_dir)
{
    curr_dir = new_dir;

    // Ensure curr_dir starts with /
    if (curr_dir.empty() || curr_dir[0] != '/') {
        curr_dir = "/" + curr_dir;
    }

    // Ensure curr_dir doesn't end with / unless it's the root
    if (curr_dir.length() > 1 && curr_dir.back() == '/') {
        curr_dir.pop_back();
    }
}

std::string TUI::get_current_directory() const
{
    return curr_dir;
}

void TUI::display_help()
{
    std::cout << "\n"
              << colors::BOLD << colors::MAGENTA
              << "Available Commands:" << colors::RESET << "\n";
    std::cout << colors::CYAN << "==================" << colors::RESET << "\n";

    size_t max_cmd_length = 0;
    for (const auto &[cmd, _] : command_descriptions) {
        max_cmd_length = std::max(max_cmd_length, cmd.length());
    }

    std::vector<std::string> cmd_names;
    for (const auto &[cmd, _] : command_descriptions) {
        cmd_names.push_back(cmd);
    }

    std::sort(cmd_names.begin(), cmd_names.end());

    for (const auto &cmd : cmd_names) {
        // Get description and split into command name part and argument part
        std::string desc = command_descriptions[cmd];
        std::string arg_part;

        size_t pos = desc.find('(');
        if (pos != std::string::npos &&
            desc.find(')', pos) != std::string::npos) {
            arg_part = desc.substr(pos);
            desc = desc.substr(0, pos - 1); // -1 to remove trailing space
        }

        std::cout << colors::BOLD << colors::GREEN << std::left
                  << std::setw(max_cmd_length + 4) << cmd << colors::RESET
                  << desc;

        if (!arg_part.empty()) {
            std::cout << " " << colors::YELLOW << arg_part << colors::RESET;
        }

        std::cout << std::endl;
    }
    std::cout << std::endl;
}

} // namespace client
} // namespace fenris
