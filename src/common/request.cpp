#include "common/request.hpp"
#include "fenris.pb.h"
#include <google/protobuf/util/json_util.h>
#include <string>

namespace fenris {
namespace common {

std::vector<uint8_t> serialize_request(const fenris::Request &request)
{
    std::string serialized;
    if (!request.SerializeToString(&serialized)) {
        // Handle serialization error (return empty vector)
        return {};
    }

    // Convert string to vector<uint8_t>
    return std::vector<uint8_t>(serialized.begin(), serialized.end());
}

fenris::Request deserialize_request(const std::vector<uint8_t> &data)
{
    fenris::Request request;

    if (data.empty()) {
        // Handle empty data
        return request;
    }

    // Convert vector<uint8_t> to string
    std::string serialized(data.begin(), data.end());

    if (!request.ParseFromString(serialized)) {
        // Handle parse error (return empty request)
        return fenris::Request();
    }

    return request;
}

std::string request_to_json(const fenris::Request &request)
{
    std::string json_output;
    google::protobuf::util::JsonPrintOptions options;
    options.add_whitespace = true;
    options.always_print_primitive_fields = true;

    google::protobuf::util::MessageToJsonString(request, &json_output, options);
    return json_output;
}

} // namespace common
} // namespace fenris
