cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

include("${CMAKE_SOURCE_DIR}/cmake/Utils.cmake")
verbose_message("Setting up client executable...")

# Define client executable
set(CLIENT_SOURCES
    main.cpp
    client.cpp
    connection_manager.cpp
    interface.cpp
    request_manager.cpp
    response_manager.cpp
    colors.cpp
)

# Create client executable
add_library(fenris_client STATIC ${CLIENT_SOURCES})

# Configure compile options
target_compile_features(fenris_client PRIVATE cxx_std_20)

# Link libraries
target_link_libraries(fenris_client
    PRIVATE
    pthread
    fenris_common
)

# Client executable
add_executable(client main.cpp)

# Link libraries to the client executable
target_link_libraries(client
    PRIVATE
    fenris_client
    fenris_common
    fenris_proto
)

# Install the client executable
install(TARGETS client
    RUNTIME DESTINATION bin
)

verbose_message("Client setup - done")
