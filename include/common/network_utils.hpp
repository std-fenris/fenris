#ifndef FENRIS_COMMON_NETWORK_UTILS_HPP
#define FENRIS_COMMON_NETWORK_UTILS_HPP

#include <cstdint>
#include <cstring>
#include <unistd.h>
#include <vector>

namespace fenris {
namespace common {
namespace network{

static const uint32_t DELAY = 100;

/**
 * @brief Sends size of the data to be sent over the socket.
 * @param socket The socket to send the data to.
 * @param size The size of the data to be sent.
 * @param non_blocking_mode True if the socket is in non-blocking mode, false
 * @return True if the size was sent successfully, false otherwise.
 */
bool send_size(uint32_t socket, uint32_t size, bool non_blocking_mode = false);

/**
 * @brief Receives size of the data to be sent over the socket.
 * @param socket The socket to receive the data from.
 * @param size The size of the data to be received.
 * @param non_blocking_mode True if the socket is in non-blocking mode, false
 * @return True if the size was received successfully, false otherwise.
 */
bool receive_size(uint32_t socket,
                  uint32_t &size,
                  bool non_blocking_mode = false);

/**
 * @brief Sends data over the socket.
 * @param socket The socket to send the data to.
 * @param data The data to be sent.
 * @param size The size of the data to be sent.
 * @param non_blocking_mode True if the socket is in non-blocking mode, false
 * @return True if the data was sent successfully, false otherwise.
 */
bool send_data(uint32_t socket,
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
 * @return True if the data was received successfully, false otherwise.
 */
bool receive_data(uint32_t socket,
                  std::vector<uint8_t> &data,
                  uint32_t size,
                  bool non_blocking_mode = false);

} // namespace network
} // namespace common
} // namespace fenris

#endif // FENRIS_COMMON_NETWORK_UTILS_HPP
