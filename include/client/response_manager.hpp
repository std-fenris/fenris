#ifndef FENRIS_CLIENT_RESPONSE_MANAGER_HPP
#define FENRIS_CLIENT_RESPONSE_MANAGER_HPP

#include "common/logging.hpp"
#include "fenris.pb.h"
#include <string>
#include <vector>

namespace fenris {
namespace client {

/**
 * @class ResponseManager
 * @brief Processes server responses and converts them to human-readable format
 *
 * This class is responsible for converting protobuf Response objects into
 * a vector of strings that can be displayed to the user through the TUI.
 */
class ResponseManager {
  public:
    /**
     * @brief Constructor
     */
    ResponseManager();

    /**
     * @brief Process a server response and format it for display
     * @param response The deserialized Protocol Buffer response
     * @return Vector of strings representing the formatted response
     */
    std::vector<std::string> handle_response(const fenris::Response &response);

  private:
    /**
     * @brief Format a PONG response
     * @param response The response object
     * @param result Vector to add formatted strings to
     */
    void handle_pong_response(const fenris::Response &response,
                              std::vector<std::string> &result);

    /**
     * @brief Format a FILE_INFO response
     * @param response The response object
     * @param result Vector to add formatted strings to
     */
    void handle_file_info_response(const fenris::Response &response,
                                   std::vector<std::string> &result);

    /**
     * @brief Format a FILE_CONTENT response
     * @param response The response object
     * @param result Vector to add formatted strings to
     */
    void handle_file_content_response(const fenris::Response &response,
                                      std::vector<std::string> &result);

    /**
     * @brief Format a DIR_LISTING response
     * @param response The response object
     * @param result Vector to add formatted strings to
     */
    void handle_directory_listing_response(const fenris::Response &response,
                                           std::vector<std::string> &result);

    /**
     * @brief Format a SUCCESS response
     * @param response The response object
     * @param result Vector to add formatted strings to
     */
    void handle_success_response(const fenris::Response &response,
                                 std::vector<std::string> &result);

    /**
     * @brief Format an ERROR response
     * @param response The response object
     * @param result Vector to add formatted strings to
     */
    void handle_error_response(const fenris::Response &response,
                               std::vector<std::string> &result);

    /**
     * @brief Format a TERMINATED response
     * @param response The response object
     * @param result Vector to add formatted strings to
     */
    void handle_terminated_response(const fenris::Response &response,
                                    std::vector<std::string> &result);

    /**
     * @brief Format file size with appropriate units (B, KB, MB, etc.)
     * @param size_bytes Size in bytes
     * @return Formatted size string
     */
    std::string format_file_size(uint64_t size_bytes);

    /**
     * @brief Format Unix timestamp to human-readable date
     * @param timestamp Unix timestamp
     * @return Formatted date string
     */
    std::string format_timestamp(uint64_t timestamp);

    /**
     * @brief Format Unix file permissions
     * @param permissions Numeric permissions (e.g., 0644)
     * @return Formatted permissions string (e.g., "rw-r--r-- (644)")
     */
    std::string format_permissions(uint32_t permissions);

    common::Logger m_logger;
};

} // namespace client
} // namespace fenris

#endif // FENRIS_CLIENT_RESPONSE_MANAGER_HPP
