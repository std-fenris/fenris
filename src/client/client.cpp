#include <cstdint>
#include <string>
namespace fenris {
namespace client {
struct ServerInfo {
    uint32_t server_id;
    uint32_t socket;
    std::string address;
    uint16_t port;
    std::string current_directory;
    // Key
    // IV
};
} // namespace client
} // namespace fenris