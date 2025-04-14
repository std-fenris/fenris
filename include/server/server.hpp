#include <cstdint>
#include <string>

namespace fenris {
namespace server {

struct ClientInfo {
    uint32_t client_id;
    uint32_t socket;
    std::string address;
    uint16_t port;
    std::string current_directory;
    // Key
    // IV
};

} // namespace server
} // namespace fenris
