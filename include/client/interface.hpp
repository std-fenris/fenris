#ifndef FENRIS_CLIENT_INTERFACE_HPP
#define FENRIS_CLIENT_INTERFACE_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fenris {
namespace client {

/**
 * @class ITUI
 * @brief Interface for Terminal User Interface operations
 *
 * Abstract base class that defines the interface for user interaction
 * via terminal. This allows for easier testing through mock implementations.
 */
class ITUI {
  public:
    /**
     * @brief Virtual destructor for proper cleanup in derived classes
     */
    virtual ~ITUI() = default;

    /**
     * @brief Get server IP address from user
     * @return IP address as string
     */
    virtual std::string get_server_IP() = 0;

    /**
     * @brief Get command from user input
     * @return Vector of command parts
     */
    virtual std::vector<std::string> get_command() = 0;

    /**
     * @brief Display result of command execution
     * @param success Whether command was successful
     * @param result Result message or data
     */
    virtual void display_result(bool success, const std::string &result) = 0;

    /**
     * @brief Update current directory
     * @param new_dir New current directory
     */
    virtual void update_current_directory(const std::string &new_dir) = 0;

    /**
     * @brief Get current directory
     * @return Current directory path
     */
    virtual std::string get_current_directory() const = 0;

    /**
     * @brief Display help information about available commands
     */
    virtual void display_help() = 0;
};

/**
 * @class TUI
 * @brief Terminal User Interface for client operations
 *
 * Concrete implementation of ITUI that interacts with the user via terminal,
 * including getting commands, validating input, and displaying results.
 */
class TUI : public ITUI {
  public:
    /**
     * @brief Constructor initializes valid commands and current directory
     */
    TUI();

    /**
     * @brief Get server IP address from user
     * @return IP address as string
     */
    std::string get_server_IP() override;

    /**
     * @brief Get command from user input
     * @return Vector of command parts
     */
    std::vector<std::string> get_command() override;

    /**
     * @brief Display result of command execution
     * @param success Whether command was successful
     * @param result Result message or data
     */
    void display_result(bool success, const std::string &result) override;

    /**
     * @brief Update current directory
     * @param new_dir New current directory
     */
    void update_current_directory(const std::string &new_dir) override;

    /**
     * @brief Get current directory
     * @return Current directory path
     */
    std::string get_current_directory() const override;

    /**
     * @brief Display help information about available commands
     */
    void display_help() override;

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
