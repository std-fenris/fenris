#ifndef FENRIS_CLIENT_INTERFACE_HPP
#define FENRIS_CLIENT_INTERFACE_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fenris {
namespace client {

/**
 * @class TUI
 * @brief Terminal User Interface for client operations
 *
 * Provides methods for interacting with the user via terminal,
 * including getting commands, validating input, and displaying results.
 */
class TUI {
  public:
    /**
     * @brief Constructor initializes valid commands and current directory
     */
    TUI();

    /**
     * @brief Get server IP address from user
     * @return IP address as string
     */
    std::string get_server_IP();

    /**
     * @brief Get command from user input
     * @return Vector of command parts
     */
    std::vector<std::string> get_command();

    /**
     * @brief Display result of command execution
     * @param success Whether command was successful
     * @param result Result message or data
     */
    void display_result(bool success, const std::string &result);

    /**
     * @brief Update current directory
     * @param new_dir New current directory
     */
    void update_current_directory(const std::string &new_dir);

    /**
     * @brief Get current directory
     * @return Current directory path
     */
    std::string get_current_directory() const;

    /**
     * @brief Display help information about available commands
     */
    void display_help();

  private:
    /**
     * @brief Validate that command and its arguments are valid
     * @param command_parts Command and its arguments to validate
     * @return True if command is valid, false otherwise
     */
    bool validate_command(const std::vector<std::string> &command_parts);

    /**
     * @brief Check if a string matches another string
     * @param str String to check
     * @param prefix Prefix to check against
     * @return True if str equals prefix, false otherwise
     */
    bool is_prefix(const std::string &str, const std::string &prefix);

    // Current directory on server
    std::string curr_dir;

    // Set of valid command prefixes
    std::unordered_set<std::string> valid_commands;

    // Map of commands to their descriptions for help display
    std::unordered_map<std::string, std::string> command_descriptions;
};

} // namespace client
} // namespace fenris

#endif // FENRIS_CLIENT_INTERFACE_HPP
