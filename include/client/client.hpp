#ifndef FENRIS_CLIENT_HPP
#define FENRIS_CLIENT_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace fenris {
namespace client {

struct ServerInfo {
    uint32_t server_id;
    uint32_t socket;
    std::string address;
    std::string port;
    std::string current_directory;
    std::vector<uint8_t> encryption_key;
};

} // namespace client
} // namespace fenris

#endif // FENRIS_CLIENT_HPP
