#ifndef FENRIS_COMMON_REQUEST_HPP
#define FENRIS_COMMON_REQUEST_HPP

#include "fenris.pb.h"
#include <string>

namespace fenris {
namespace common {

std::vector<uint8_t> serialize_request(const fenris::Request &request);

fenris::Request deserialize_request(const std::vector<uint8_t> &data);

std::string request_to_json(const fenris::Request &request);

} // namespace common
} // namespace fenris

#endif // FENRIS_COMMON_REQUEST_HPP
