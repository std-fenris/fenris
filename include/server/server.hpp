#ifndef FENRIS_SERVER_HPP
#define FENRIS_SERVER_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace fenris {
namespace server {

struct ClientInfo {
    uint32_t client_id;
    uint32_t socket;
    std::string address;
    std::string port;
    std::string current_directory;
    std::vector<uint8_t> encryption_key;
};

} // namespace server
} // namespace fenris

#endif // FENRIS_SERVER_HPP
