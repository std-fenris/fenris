#ifndef FENRIS_COMMON_RESPONSE_HPP
#define FENRIS_COMMON_RESPONSE_HPP

#include "fenris.pb.h"
#include <string>

namespace fenris {
namespace common {

std::vector<uint8_t> serialize_response(const fenris::Response &response);

fenris::Response deserialize_response(const std::vector<uint8_t> &data);

std::string response_to_json(const fenris::Response &response);

} // namespace common
} // namespace fenris

#endif // FENRIS_COMMON_RESPONSE_HPP
