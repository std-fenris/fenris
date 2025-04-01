cmake_minimum_required(VERSION 3.15)

project(
  ${CMAKE_PROJECT_NAME}_test
  LANGUAGES CXX
)

include("${CMAKE_SOURCE_DIR}/cmake/Utils.cmake")

verbose_message("Adding tests under ${CMAKE_PROJECT_NAME}_test...")

set(test_sources
  ${CMAKE_CURRENT_SOURCE_DIR}/src/example_test.cpp
)

foreach(file ${test_sources})
  string(REGEX REPLACE "(.*/)([a-zA-Z0-9_ ]+)(\.cpp)" "\\2" test_name ${file})
  add_executable(${test_name} ${file})
  target_compile_features(${test_name} PUBLIC cxx_std_20)
  find_package(GTest REQUIRED)

  target_link_libraries(
  ${test_name}
  PUBLIC
    GTest::GTest
    GTest::Main
)

  add_test(
    NAME
      ${test_name}
    COMMAND
      ${test_name}
  )
endforeach()

verbose_message("Finished adding unit tests for ${CMAKE_PROJECT_NAME}.")
