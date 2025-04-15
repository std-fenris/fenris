cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

include("${CMAKE_SOURCE_DIR}/cmake/Utils.cmake")
verbose_message("Setting up unit tests...")

# Common components unit tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/common")
  add_subdirectory(common)
endif()

verbose_message("Unit tests setup - done")
