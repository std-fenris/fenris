cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(fenris LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add cmake directory to module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Set output directories for binaries directly in build folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Find dependencies
find_package(ZLIB REQUIRED)
find_package(CryptoPP REQUIRED)
find_package(spdlog REQUIRED)

# Define variables for libraries to use throughout the project
set(FENRIS_EXTERNAL_LIBS ${ZLIB_LIBRARIES} ${CRYPTOPP_LIBRARIES} spdlog::spdlog)
set(FENRIS_EXTERNAL_INCLUDES ${ZLIB_INCLUDE_DIRS} ${CRYPTOPP_INCLUDE_DIRS} ${SPDLOG_INCLUDE_DIR})

# Add GTest package for unit testing
if(UNIT_TESTING)
    find_package(GTest REQUIRED)
    if(NOT GTest_FOUND)
        message(FATAL_ERROR "Google Test framework not found!")
    endif()
endif()

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
