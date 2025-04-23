cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

include("${CMAKE_SOURCE_DIR}/cmake/Utils.cmake")
verbose_message("Setting up common library...")

set(
    COMMON_SOURCES
    compression.cpp
    crypto_manager.cpp
    file_operations.cpp
    logging.cpp
    request.cpp
    response.cpp
    network_utils.cpp
    ${PROTO_SRCS}
)

# Create a static library from the common sources
add_library(fenris_common STATIC ${COMMON_SOURCES})

add_dependencies(fenris_common fenris_proto)

# Configure compile options
target_compile_features(fenris_common PRIVATE cxx_std_20)

# Make the include directory available to users of this library
target_include_directories(fenris_common PUBLIC
    ${FENRIS_EXTERNAL_INCLUDES}
    ${CMAKE_BINARY_DIR}/proto
)

# Link needed libraries
target_link_libraries(fenris_common
    PUBLIC
    ${FENRIS_EXTERNAL_LIBS}
)

verbose_message("Common library setup - done")
