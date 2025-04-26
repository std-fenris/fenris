cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(fenris LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add cmake directory to module path
list(APPEND CMAKE_MODULE_PATH "cmake")

# SPDLOG
set(SPDLOG_ENABLE_PCH ON)
add_subdirectory(vendor/spdlog)

# ZLIB
set(ZLIB_BUILD_TESTING OFF)
add_subdirectory(vendor/zlib)

# ARGPARSE
set(ARGPARSE_BUILD_TESTS OFF)
add_subdirectory(vendor/argparse)

# Setup Crypto++ as an imported library
set(CRYPTOPP_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/cryptopp)
set(CRYPTOPP_INCLUDE_DIR ${CRYPTOPP_SOURCE_DIR})
set(CRYPTOPP_LIBRARY_DIR ${CMAKE_BINARY_DIR}/cryptopp)

# Create imported target for Crypto++
add_library(cryptopp STATIC IMPORTED)
set_target_properties(cryptopp PROPERTIES
    IMPORTED_LOCATION ${CRYPTOPP_LIBRARY_DIR}/libcryptopp.a
    INTERFACE_INCLUDE_DIRECTORIES ${CRYPTOPP_INCLUDE_DIR}
)

# Add custom command to build Crypto++ using its GNUmakefile
add_custom_command(
    OUTPUT ${CRYPTOPP_LIBRARY_DIR}/libcryptopp.a
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CRYPTOPP_LIBRARY_DIR}
    COMMAND $(MAKE) -C ${CRYPTOPP_SOURCE_DIR} static
    COMMAND ${CMAKE_COMMAND} -E copy ${CRYPTOPP_SOURCE_DIR}/libcryptopp.a ${CRYPTOPP_LIBRARY_DIR}/libcryptopp.a
    WORKING_DIRECTORY ${CRYPTOPP_SOURCE_DIR}
    DEPENDS ${CRYPTOPP_SOURCE_DIR}/GNUmakefile
    COMMENT "Building Crypto++ library"
    VERBATIM
)

# Add custom target for Crypto++
add_custom_target(build_cryptopp DEPENDS ${CRYPTOPP_LIBRARY_DIR}/libcryptopp.a)
add_dependencies(cryptopp build_cryptopp)

# Include protobuf files
include(FindProtobuf)
find_package(Protobuf 3.21 EXACT REQUIRED)
message(STATUS "Protobuf version: ${Protobuf_VERSION}")
add_subdirectory(proto)

# Define variables for libraries to use throughout the project
set(FENRIS_EXTERNAL_LIBS cryptopp spdlog::spdlog zlib argparse ${PROTOBUF_LIBRARIES})
set(FENRIS_EXTERNAL_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/vendor ${PROTOBUF_INCLUDE_DIRS})

# Define release type flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wpedantic -Wfatal-errors")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-braces -Wno-unused-parameter")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()

# Set verbose output option
option(${PROJECT_NAME}_VERBOSE_OUTPUT "Enable verbose output" OFF)

# Add include directory
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_BINARY_DIR}/proto)

# Add subdirectory for common components first
add_subdirectory(src/common)

# Add subdirectories for client and server
add_subdirectory(src/client)
add_subdirectory(src/server)

if(UNIT_TESTING)
  enable_testing()
  message(STATUS "Build unit tests for the project. Tests should always be found in the tests folder\n")
  add_subdirectory(tests)
endif()
