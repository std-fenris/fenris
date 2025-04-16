cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

include("${CMAKE_SOURCE_DIR}/cmake/Utils.cmake")
verbose_message("Setting up common unit tests...")

# Function to add a unit test with standardized settings
function(add_fenris_unittest test_name)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE
        gtest
        gtest_main
        fenris_common
        fenris_proto
    )
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_fenris_unittest(compression_test)
add_fenris_unittest(encryption_test)
add_fenris_unittest(ecdh_test)
add_fenris_unittest(file_operations_test)
add_fenris_unittest(request_test)
add_fenris_unittest(response_test)

verbose_message("Common unit tests setup - done")
