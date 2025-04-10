// common request related data needed by both server and client

#include <cstdint>

enum class RequestType {
    UPLOAD_FILE,
    READ_FILE,
    APPEND_FILE,
    DELETE_FILE,
    INFO_FILE,
    CREATE_DIR,
    LIST_DIR,
    DELETE_DIR,
    TERMINATE
};

struct Request {
    RequestType command;
    char filename[256];
    uint32_t ip_addr;
    uint32_t size_d;
    uint8_t data[];
};
