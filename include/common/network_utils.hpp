#ifndef FENRIS_COMMON_NETWORK_UTILS_HPP
#define FENRIS_COMMON_NETWORK_UTILS_HPP

#include <cstdint>
#include <cstring>
#include <unistd.h>
#include <vector>

namespace fenris {
namespace common {
namespace network {

static const uint32_t DELAY = 100;

/**
 * @enum NetworkError
 * @brief Represents different kinds of network operation errors
 */
enum class NetworkError {
    SUCCESS,            // Operation completed successfully
    DISCONNECTED,       // Peer disconnected
    CONNECTION_REFUSED, // Connection was refused by peer
    SOCKET_ERROR,       // Error with the socket
    SEND_ERROR,         // Error occurred during send operation
    RECEIVE_ERROR,      // Error occurred during receive operation
    ALLOCATION_ERROR,   // Error allocating memory
};

/**
 * @brief Sends size of the data to be sent over the socket.
 * @param socket The socket to send the data to.
 * @param size The size of the data to be sent.
 * @param non_blocking_mode True if the socket is in non-blocking mode, false
 * @return NetworkError indicating success or failure type
 */
NetworkError
send_size(uint32_t socket, uint32_t size, bool non_blocking_mode = false);

/**
 * @brief Receives size of the data to be sent over the socket.
 * @param socket The socket to receive the data from.
 * @param size The size of the data to be received.
 * @param non_blocking_mode True if the socket is in non-blocking mode, false
 * @return NetworkError indicating success or failure type
 */
NetworkError
receive_size(uint32_t socket, uint32_t &size, bool non_blocking_mode = false);

/**
 * @brief Sends data over the socket.
 * @param socket The socket to send the data to.
 * @param data The data to be sent.
 * @param size The size of the data to be sent.
 * @param non_blocking_mode True if the socket is in non-blocking mode, false
 * @return NetworkError indicating success or failure type
 */
NetworkError send_data(uint32_t socket,
                       const std::vector<uint8_t> &data,
                       uint32_t size,
                       bool non_blocking_mode = false);

/**
 * @brief Receives data over the socket.
 * @param socket The socket to receive the data from.
 * @param data The data to be received.
 * @param size The size of the data to be received.
 * @param non_blocking_mode True if the socket is in non-blocking mode, false
 * otherwise.
 * @return NetworkError indicating success or failure type
 */
NetworkError receive_data(uint32_t socket,
                          std::vector<uint8_t> &data,
                          uint32_t size,
                          bool non_blocking_mode = false);

/**
 * @brief Sends data with size prefix over socket.
 * @param socket The socket to send the data to.
 * @param data The data to be sent.
 * @return NetworkError indicating success or failure type
 */
NetworkError send_prefixed_data(int socket, const std::vector<uint8_t> &data);

/**
 * @brief Receives data with size prefix from socket.
 * @param socket The socket to receive the data from.
 * @param data The data to be received.
 * @return NetworkError indicating success or failure type
 */
NetworkError receive_prefixed_data(int socket, std::vector<uint8_t> &data);

} // namespace network
} // namespace common
} // namespace fenris

#endif // FENRIS_COMMON_NETWORK_UTILS_HPP
