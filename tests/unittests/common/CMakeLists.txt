cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

include("${CMAKE_SOURCE_DIR}/cmake/Utils.cmake")
verbose_message("Setting up common unit tests...")

add_fenris_test(compression_test compression_test.cpp)
add_fenris_test(crypto_test crypto_test.cpp)
add_fenris_test(file_operations_test file_operations_test.cpp)
