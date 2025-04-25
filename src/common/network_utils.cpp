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

std::string network_result_to_string(NetworkResult result)
{
    switch (result) {
    case NetworkResult::SUCCESS:
        return "success";
    case NetworkResult::DISCONNECTED:
        return "peer disconnected";
    case NetworkResult::CONNECTION_REFUSED:
        return "connection refused by peer";
    case NetworkResult::SOCKET_ERROR:
        return "socket error";
    case NetworkResult::SEND_ERROR:
        return "error during send operation";
    case NetworkResult::RECEIVE_ERROR:
        return "error during receive operation";
    case NetworkResult::ALLOCATION_ERROR:
        return "memory allocation error";
    default:
        return "unrecognized network error";
    }
}

NetworkResult send_data(uint32_t fd,
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
            return NetworkResult::SEND_ERROR;
        }
        total_sent += static_cast<size_t>(sent);
    }
    return NetworkResult::SUCCESS;
}

NetworkResult receive_data(uint32_t fd,
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
                return NetworkResult::DISCONNECTED;
            } else {
                return NetworkResult::RECEIVE_ERROR;
            }
        }
        total_received += static_cast<size_t>(received);
    }
    return NetworkResult::SUCCESS;
}

NetworkResult send_size(uint32_t fd, uint32_t size, bool non_blocking_mode)
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
            return NetworkResult::SEND_ERROR;
        }
        total_sent += static_cast<size_t>(sent);
    }
    return NetworkResult::SUCCESS;
}

NetworkResult receive_size(uint32_t fd, uint32_t &size, bool non_blocking_mode)
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
                return NetworkResult::DISCONNECTED;
            }
            return NetworkResult::RECEIVE_ERROR;
        }
        total_received += static_cast<size_t>(received);
    }
    size = ntohl(size_net);
    return NetworkResult::SUCCESS;
}

NetworkResult send_prefixed_data(uint32_t socket,
                                 const std::vector<uint8_t> &data,
                                 bool non_blocking_mode)
{
    NetworkResult result;

    result = send_size(socket,
                       static_cast<uint32_t>(data.size()),
                       non_blocking_mode);
    if (result != NetworkResult::SUCCESS) {
        return result;
    }

    result = send_data(socket,
                       data,
                       static_cast<uint32_t>(data.size()),
                       non_blocking_mode);
    if (result != NetworkResult::SUCCESS) {
        return result;
    }

    return NetworkResult::SUCCESS;
}

NetworkResult receive_prefixed_data(uint32_t socket,
                                    std::vector<uint8_t> &data,
                                    bool non_blocking_mode)
{
    NetworkResult result;
    uint32_t size = 0;

    result = receive_size(socket, size, non_blocking_mode);
    if (result != NetworkResult::SUCCESS) {
        return result;
    }

    try {
        data.resize(size);
    } catch (const std::bad_alloc &) {
        return NetworkResult::ALLOCATION_ERROR;
    }
    result = receive_data(socket, data, size, non_blocking_mode);
    if (result != NetworkResult::SUCCESS) {
        return result;
    }

    return NetworkResult::SUCCESS;
}

} // namespace network
} // namespace common
} // namespace fenris
