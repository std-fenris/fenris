#include "common/network_utils.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace fenris {
namespace common {
namespace network {

NetworkError send_data(uint32_t fd,
                       const std::vector<uint8_t> &data,
                       uint32_t len,
                       bool non_blocking_mode)
{
    size_t total_sent = 0;
    while (total_sent < len) {
        ssize_t sent = send(fd, data.data() + total_sent, len - total_sent, 0);
        if (sent <= 0) {
            if (non_blocking_mode &&
                (errno == EAGAIN || errno == EWOULDBLOCK)) {
                // In non-blocking mode, EAGAIN/EWOULDBLOCK means try again
                // later
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(DELAY)); // Small delay
                continue;
            }
            // For blocking sockets or other errors, report failure
            std::cerr << "Error sending data: " << strerror(errno) << std::endl;
            return NetworkError::SEND_ERROR;
        }
        total_sent += static_cast<size_t>(sent);
    }
    return NetworkError::SUCCESS;
}

NetworkError receive_data(uint32_t fd,
                          std::vector<uint8_t> &buf,
                          uint32_t len,
                          bool non_blocking_mode)
{
    size_t total_received = 0;
    while (total_received < len) {
        ssize_t received =
            recv(fd, buf.data() + total_received, len - total_received, 0);
        if (received <= 0) {
            if (non_blocking_mode &&
                (errno == EAGAIN || errno == EWOULDBLOCK)) {
                // In non-blocking mode, EAGAIN/EWOULDBLOCK means try again
                // later
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(DELAY)); // Small delay
                continue;
            }
            // For blocking sockets, 0 means disconnected, -1 means error
            if (received == 0) {
                return NetworkError::DISCONNECTED;
            } else {
                return NetworkError::RECEIVE_ERROR;
            }
        }
        total_received += static_cast<size_t>(received);
    }
    return NetworkError::SUCCESS;
}

NetworkError send_size(uint32_t fd, uint32_t size, bool non_blocking_mode)
{
    uint32_t size_net = htonl(size);
    size_t total_sent = 0;
    while (total_sent < sizeof(size_net)) {
        ssize_t sent = send(fd,
                            reinterpret_cast<uint8_t *>(&size_net) + total_sent,
                            sizeof(size_net) - total_sent,
                            0);

        if (sent <= 0) {
            if (non_blocking_mode &&
                (errno == EAGAIN || errno == EWOULDBLOCK)) {
                // In non-blocking mode, EAGAIN/EWOULDBLOCK means try again
                // later
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(DELAY)); // Small delay
                continue;
            }
            return NetworkError::SEND_ERROR;
        }
        total_sent += static_cast<size_t>(sent);
    }
    return NetworkError::SUCCESS;
}

NetworkError receive_size(uint32_t fd, uint32_t &size, bool non_blocking_mode)
{
    uint32_t size_net;
    size_t total_received = 0;
    while (total_received < sizeof(size_net)) {
        ssize_t received =
            recv(fd,
                 reinterpret_cast<uint8_t *>(&size_net) + total_received,
                 sizeof(size_net) - total_received,
                 0);
        if (received <= 0) {
            if (non_blocking_mode &&
                (errno == EAGAIN || errno == EWOULDBLOCK)) {
                // In non-blocking mode, EAGAIN/EWOULDBLOCK means try again
                // later
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(DELAY)); // Small delay
                continue;
            }
            if (received == 0) {
                return NetworkError::DISCONNECTED;
            }
            return NetworkError::RECEIVE_ERROR;
        }
        total_received += static_cast<size_t>(received);
    }
    size = ntohl(size_net);
    return NetworkError::SUCCESS;
}

NetworkError send_prefixed_data(int socket,
                                const std::vector<uint8_t> &data,
                                bool non_blocking_mode)
{
    NetworkError err;

    err = send_size(socket,
                    static_cast<uint32_t>(data.size()),
                    non_blocking_mode);
    if (err != NetworkError::SUCCESS) {
        return err;
    }

    err = send_data(socket,
                    data,
                    static_cast<uint32_t>(data.size()),
                    non_blocking_mode);
    if (err != NetworkError::SUCCESS) {
        return err;
    }

    return NetworkError::SUCCESS;
}

NetworkError receive_prefixed_data(int socket,
                                   std::vector<uint8_t> &data,
                                   bool non_blocking_mode)
{
    NetworkError err;
    uint32_t size = 0;

    err = receive_size(socket, size, non_blocking_mode);
    if (err != NetworkError::SUCCESS) {
        return err;
    }

    try {
        data.resize(size);
    } catch (const std::bad_alloc &) {
        return NetworkError::ALLOCATION_ERROR;
    }
    err = receive_data(socket, data, size, non_blocking_mode);
    if (err != NetworkError::SUCCESS) {
        return err;
    }

    return NetworkError::SUCCESS;
}

} // namespace network
} // namespace common
} // namespace fenris
