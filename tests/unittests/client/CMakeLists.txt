cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

include("${CMAKE_SOURCE_DIR}/cmake/Utils.cmake")
verbose_message("Setting up client unit tests...")

# Function to add a unit test with standardized settings
function(add_fenris_client_unittest test_name)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE
        gtest
        gtest_main
        fenris_common
        fenris_client
        fenris_proto
    )
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/client
        ${CMAKE_SOURCE_DIR}/include
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_fenris_client_unittest(client_connection_manager_test)
