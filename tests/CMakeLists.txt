cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

include("${CMAKE_SOURCE_DIR}/cmake/Utils.cmake")
verbose_message("Setting up tests...")

add_subdirectory(googletest)
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

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
