// common request related data needed by both server and client
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
