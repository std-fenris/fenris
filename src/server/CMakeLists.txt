cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

include("${CMAKE_SOURCE_DIR}/cmake/Utils.cmake")
verbose_message("Setting up server executable...")

# Define server executable
set(SERVER_SOURCES
    main.cpp
    server.cpp
    connection_manager.cpp
    request_manager.cpp
    response_manager.cpp
    cache_manager.cpp
)

# Create server executable
add_executable(fenris_server ${SERVER_SOURCES})

# Configure compile options
target_compile_features(fenris_server PRIVATE cxx_std_20)

# Include directories from found packages
include_directories(${ZLIB_INCLUDE_DIRS})
include_directories(${CRYPTOPP_INCLUDE_DIRS})

# Link libraries
target_link_libraries(fenris_server
    PRIVATE
    pthread
    ${ZLIB_LIBRARIES}
    ${CRYPTOPP_LIBRARIES}
)

# Install the server executable
install(TARGETS fenris_server
    RUNTIME DESTINATION bin
)

verbose_message("Server setup - done")
