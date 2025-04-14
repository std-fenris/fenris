cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

include("${CMAKE_SOURCE_DIR}/cmake/Utils.cmake")
verbose_message("Setting up tests...")

# Common test setup
set(TEST_COMMON_LIBS
    GTest::GTest
    GTest::Main
    pthread
    fenris_common
)

# Create a function to simplify test creation
function(add_fenris_test test_name test_sources)
    add_executable(${test_name} ${test_sources})

    target_compile_features(${test_name} PRIVATE cxx_std_20)

    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${GTEST_INCLUDE_DIRS}
    )

    target_link_libraries(${test_name} PRIVATE ${TEST_COMMON_LIBS})

    add_test(NAME ${test_name} COMMAND ${test_name})

    verbose_message("Added test: ${test_name}")
endfunction()

# Function to add a test subdirectory if it exists
function(add_test_subdirectory dir)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/CMakeLists.txt")
    add_subdirectory(${dir})
    verbose_message("Added test subdirectory: ${dir}")
  else()
    verbose_message("Test subdirectory not found: ${dir}")
  endif()
endfunction()

# Include test categories
add_test_subdirectory(unittests)

verbose_message("Tests setup - done")
