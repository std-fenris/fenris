cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

include("${CMAKE_SOURCE_DIR}/cmake/Utils.cmake")
verbose_message("Setting up server executable...")

# Define server executable
set(SERVER_SOURCES
    main.cpp
    cache_manager.cpp
    client_info.cpp
    connection_manager.cpp
    request_manager.cpp
    server.cpp
)

# Create server executable
add_library(fenris_server STATIC ${SERVER_SOURCES})

# Configure compile options
target_compile_features(fenris_server PRIVATE cxx_std_20)

# Link libraries
target_link_libraries(fenris_server
    PRIVATE
    pthread
    fenris_common
)

# Client executable
add_executable(server main.cpp)

# Link libraries to the client executable
target_link_libraries(server
    PRIVATE
    fenris_server
    fenris_common
    fenris_proto
)
# Install the server executable
install(TARGETS fenris_server
    RUNTIME DESTINATION bin
)

verbose_message("Server setup - done")
