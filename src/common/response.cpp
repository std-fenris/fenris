#include "common/response.hpp"
#include "fenris.pb.h"
#include <google/protobuf/util/json_util.h>
#include <string>

namespace fenris {
namespace common {

using namespace google::protobuf::util;

std::vector<uint8_t> serialize_response(const fenris::Response &response)
{
    std::string serialized;
    if (!response.SerializeToString(&serialized)) {
        // Handle serialization error (return empty vector)
        return {};
    }

    // Convert string to vector<uint8_t>
    return std::vector<uint8_t>(serialized.begin(), serialized.end());
}

fenris::Response deserialize_response(const std::vector<uint8_t> &data)
{
    fenris::Response response;

    if (data.empty()) {
        // Handle empty data
        return response;
    }

    // Convert vector<uint8_t> to string
    std::string serialized(data.begin(), data.end());

    if (!response.ParseFromString(serialized)) {
        // Handle parse error (return empty response)
        return fenris::Response();
    }

    return response;
}

std::string response_to_json(const fenris::Response &response)
{
    std::string json_output;
    JsonPrintOptions options;
    options.add_whitespace = true;
    options.always_print_primitive_fields = true;

    MessageToJsonString(response, &json_output, options);
    return json_output;
}

} // namespace common
} // namespace fenris
