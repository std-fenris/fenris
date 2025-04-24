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

NetworkError send_prefixed_data(int socket, const std::vector<uint8_t> &data)
{
    uint32_t size = htonl(static_cast<uint32_t>(data.size()));

    if (send(socket, &size, sizeof(size), 0) != sizeof(size)) {
        return NetworkError::SEND_ERROR;
    }

    if (send(socket, data.data(), data.size(), 0) !=
        static_cast<ssize_t>(data.size())) {
        return NetworkError::SEND_ERROR;
    }

    return NetworkError::SUCCESS;
}

NetworkError receive_prefixed_data(int socket, std::vector<uint8_t> &data)
{
    uint32_t size_network;

    if (recv(socket, &size_network, sizeof(size_network), 0) !=
        sizeof(size_network)) {
        return NetworkError::RECEIVE_ERROR;
    }

    uint32_t size = ntohl(size_network);

    try {
        data.resize(size);
    } catch (const std::exception &e) {
        return NetworkError::ALLOCATION_ERROR;
    }

    ssize_t total_received = 0;

    while (total_received < static_cast<ssize_t>(size)) {
        ssize_t bytes_received = recv(socket,
                                      data.data() + total_received,
                                      size - total_received,
                                      0);

        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                return NetworkError::DISCONNECTED;
            } else {
                return NetworkError::RECEIVE_ERROR;
            }
        }
        total_received += bytes_received;
    }
    return NetworkError::SUCCESS;
}

} // namespace network
} // namespace common
} // namespace fenris
