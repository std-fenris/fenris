cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

# Set output directory for generated files
set(PROTO_GENERATED_DIR "${CMAKE_BINARY_DIR}/proto")
file(MAKE_DIRECTORY ${PROTO_GENERATED_DIR})

# Set the path to the proto files
set(PROTO_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/fenris.proto"
)

# Compile proto files
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})
add_library(fenris_proto ${PROTO_SRCS} ${PROTO_HDRS})
