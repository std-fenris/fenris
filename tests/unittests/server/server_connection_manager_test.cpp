#include "fenris.pb.h"
#include "server/connection_manager.hpp"
#include "server/server.hpp"

#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector> // Include vector

// Helper function to serialize Request
std::vector<uint8_t> serialize_request(const fenris::Request &request)
{
    std::string serialized_data;
    request.SerializeToString(&serialized_data);
    return std::vector<uint8_t>(serialized_data.begin(), serialized_data.end());
}

// Helper function to deserialize Response
fenris::Response deserialize_response(const std::vector<uint8_t> &data)
{
    fenris::Response response;
    response.ParseFromArray(data.data(), data.size());
    return response;
}

// Helper function to send size-prefixed data
bool send_prefixed_data(int sock, const std::vector<uint8_t> &data)
{
    uint32_t size = htonl(static_cast<uint32_t>(data.size()));

    if (send(sock, &size, sizeof(size), 0) != sizeof(size)) {
        return false;
    }

    if (send(sock, data.data(), data.size(), 0) !=
        static_cast<ssize_t>(data.size())) {
        return false;
    }

    return true;
}

// Helper function to receive size-prefixed data
bool receive_prefixed_data(int sock, std::vector<uint8_t> &data)
{
    uint32_t size_network;

    if (recv(sock, &size_network, sizeof(size_network), 0) !=
        sizeof(size_network)) {
        std::cerr << "Failed to receive size prefix" << std::endl;
        return false;
    }

    uint32_t size = ntohl(size_network);
    // Validate size to prevent unreasonable allocations
    // Using 100MB as a reasonable upper limit for test data
    constexpr uint32_t MAX_REASONABLE_SIZE = 100 * 1024 * 1024;
    if (size == 0 || size > MAX_REASONABLE_SIZE) {
        std::cerr << "Invalid size received: " << size
                  << " (possibly corrupted data)" << std::endl;
        return false;
    }

    try {
        data.resize(size);
    } catch (const std::exception &e) {
        std::cerr << "Failed to allocate buffer for size " << size << ": "
                  << e.what() << std::endl;
        return false;
    }

    ssize_t total_received = 0;

    while (total_received < static_cast<ssize_t>(size)) {
        ssize_t bytes_received =
            recv(sock, data.data() + total_received, size - total_received, 0);

        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                std::cerr << "Connection closed by peer" << std::endl;
            } else {
                std::cerr << "Error receiving data: " << strerror(errno)
                          << std::endl;
            }
            return false; // Error or connection closed
        }
        total_received += bytes_received;
    }
    return true;
}

namespace fenris {
namespace server {
namespace tests{

// Mock implementation of ClientHandler for testing
class MockClientHandler : public ClientHandler {
  public:
    explicit MockClientHandler(bool keep_connection = true,
                               int max_requests = 10)
        : m_keep_connection(keep_connection), m_max_requests(max_requests),
          m_request_count(0)
    {
    }

    // Updated handle_request signature
    std::pair<fenris::Response, bool>
    handle_request(uint32_t client_socket,
                   const fenris::Request &request) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_handled_client_sockets.push_back(client_socket);
        m_received_requests.push_back(
            request); // Store the received protobuf request
        m_request_count++;

        // Create a simple default response
        fenris::Response response;
        response.set_success(true);
        switch (request.command()) {
        case fenris::RequestType::PING:
            response.set_type(fenris::ResponseType::PONG);
            response.set_data("PING");
            break;
        case fenris::RequestType::READ_FILE:
            response.set_type(fenris::ResponseType::FILE_CONTENT);
            response.set_data("READ_FILE");
            break;
        case fenris::RequestType::WRITE_FILE:
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_data("WRITE_FILE");
            break;
        case fenris::RequestType::LIST_DIR:
            response.set_type(fenris::ResponseType::DIR_LISTING);
            response.set_data("LIST_DIR");
            break;
        case fenris::RequestType::DELETE_DIR:
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_data("DELETE_DIRECTORY");
            break;
        case fenris::RequestType::DELETE_FILE:
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_data("DELETE_FILE");
            break;
        case fenris::RequestType::CREATE_DIR:
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_data("CREATE_DIR");
            break;
        case fenris::RequestType::CREATE_FILE:
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_data("CREATE_FILE");
            break;
        case fenris::RequestType::TERMINATE:
            response.set_type(fenris::ResponseType::TERMINATED);
            response.set_data("TERMINATE");
            m_keep_connection = false; // Set to false to close connection
            break;
        default:
            response.set_success(false);
            response.set_error_message("Unknown command");
        }
        bool should_keep_connection =
            m_keep_connection && (m_request_count < m_max_requests);
        return {response, should_keep_connection};
    }

    std::vector<uint32_t> get_handled_client_ids()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_handled_client_sockets;
    }

    std::vector<fenris::Request>
    get_received_requests() // Return vector of protobuf Requests
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_received_requests;
    }

    // Removed get_received_data as we now store Request objects

    int get_request_count()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_request_count;
    }

  private:
    bool m_keep_connection; // Renamed from m_return_value for clarity
    int m_max_requests;
    int m_request_count;
    std::mutex m_mutex;
    std::vector<uint32_t> m_handled_client_sockets;
    std::vector<fenris::Request> m_received_requests; // Store protobuf Requests
    // Removed m_received_data
};

// Helper function to create a client socket and connect to server
int create_and_connect_client_socket(const char *server_ip, int server_port)
{
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        return -1;
    }
    std::cout << "Client socket created: " << client_socket << std::endl;

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(client_socket,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {
        close(client_socket);
        return -1;
    }
    std::cout << "Client connected to server: " << server_ip << ":"
              << server_port << std::endl;
    return client_socket;
}

// Test fixture for ConnectionManager
class ServerConnectionManagerTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        // Use a high port number that's likely to be available
        m_port = 12345 + (rand() % 1000); // Random high port to avoid conflicts
        m_port_str = std::to_string(m_port);
        // Keep connection open by default in mock handler for most tests
        m_handler =
            std::make_shared<MockClientHandler>(true,
                                                10); // Allow multiple requests

        // Create connection manager
        m_connection_manager =
            std::make_unique<ConnectionManager>("127.0.0.1", m_port_str);
        m_connection_manager->set_client_handler(m_handler);

        // Enable non-blocking mode for testing to avoid hanging
        m_connection_manager->set_non_blocking_mode(false);
    }

    void TearDown() override
    {
        // Ensure proper server shutdown regardless of test result
        if (m_connection_manager) {
            m_connection_manager->stop();
            // Allow time for the server to fully shutdown
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        // Close any remaining client sockets
        for (int sock : m_client_sockets) {
            if (sock >= 0) {
                close(sock);
            }
        }
        m_client_sockets.clear();
    }

    int connect_test_client()
    {
        int client_sock = create_and_connect_client_socket("127.0.0.1", m_port);
        if (client_sock >= 0) {
            m_client_sockets.push_back(client_sock);
        }
        return client_sock;
    }

    // Helper method to safely stop the server with verification
    void safe_stop_server()
    {
        if (m_connection_manager) {
            m_connection_manager->stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            int test_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (test_socket >= 0) {
                int flags = fcntl(test_socket, F_GETFL);
                fcntl(test_socket, F_SETFL, flags | O_NONBLOCK);

                sockaddr_in server_addr{};
                server_addr.sin_family = AF_INET;
                server_addr.sin_port = htons(m_port);
                inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

                int result = connect(test_socket,
                                     reinterpret_cast<sockaddr*>(&server_addr),
                                     sizeof(server_addr));

                if (result < 0 && errno == EINPROGRESS) {
                    for (int i = 0; i < 5; i++) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));

                        int err = 0;
                        socklen_t len = sizeof(err);
                        getsockopt(test_socket, SOL_SOCKET, SO_ERROR, &err, &len);

                        if (!err) {
                            FAIL() << "Connection succeeded; server may not be fully stopped.";
                            break;
                        } else if (err == ECONNREFUSED || err == ETIMEDOUT) {
                            // Server is properly shut down
                            break;
                        }
                    }
                } else if (result == 0) {
                    // Connection succeeded immediately
                    FAIL() << "Connection succeeded immediately; server may not be fully stopped.";
                }

                close(test_socket);
            }
        }
    }

    std::unique_ptr<ConnectionManager> m_connection_manager;
    std::shared_ptr<MockClientHandler> m_handler;
    int m_port;
    std::string m_port_str;
    std::vector<int> m_client_sockets;
};

// Test starting and stopping the connection manager
TEST_F(ServerConnectionManagerTest, StartAndStop)
{
    ASSERT_EQ(m_connection_manager->get_active_client_count(), 0);

    // Start the connection manager
    m_connection_manager->start();

    // Give it time to fully start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop the connection manager using the safe method
    safe_stop_server();

    // Check that no clients are connected
    ASSERT_EQ(m_connection_manager->get_active_client_count(), 0);
}

// Test accepting a client connection
TEST_F(ServerConnectionManagerTest, AcceptClientConnection)
{
    m_connection_manager->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Connect client (includes magic number exchange)
    int client_sock = connect_test_client();
    ASSERT_GE(client_sock, 0);

    // Wait for server to register connection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(m_connection_manager->get_active_client_count(), 1);

    // Create a PING request protobuf
    fenris::Request ping_request;
    ping_request.set_command(fenris::RequestType::PING);

    // Serialize and send the request with size prefix
    std::vector<uint8_t> serialized_request = serialize_request(ping_request);
    ASSERT_TRUE(send_prefixed_data(client_sock, serialized_request));
    // Wait for server to process and respond
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Receive the response
    std::vector<uint8_t> received_data;
    ASSERT_TRUE(receive_prefixed_data(client_sock, received_data));
    // Deserialize and verify the response
    fenris::Response response = deserialize_response(received_data);
    ASSERT_TRUE(response.success());
    ASSERT_EQ(response.data(), "PING"); // Mock handler echoes command name

    // Verify handler received the request
    auto received_requests = m_handler->get_received_requests();
    ASSERT_EQ(received_requests.size(), 1);
    ASSERT_TRUE(
        google::protobuf::util::MessageDifferencer::Equals(received_requests[0],
                                                           ping_request));
    // Send terminate
    fenris::Request terminate_request;
    terminate_request.set_command(fenris::RequestType::TERMINATE);
    std::vector<uint8_t> serialized_terminate_request =
        serialize_request(terminate_request);
    ASSERT_TRUE(send_prefixed_data(client_sock, serialized_terminate_request));
    // Wait for server to process and respond
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // Receive the response
    std::vector<uint8_t> terminate_received_data;
    ASSERT_TRUE(receive_prefixed_data(client_sock, terminate_received_data));
    // Deserialize and verify the response
    fenris::Response terminate_response =
        deserialize_response(terminate_received_data);
    ASSERT_TRUE(terminate_response.success());
    ASSERT_EQ(terminate_response.data(), "TERMINATE");

    // Close client socket (will be handled by TearDown)
    // safe_stop_server(); // Let TearDown handle stop
}

// Test multiple client connections
TEST_F(ServerConnectionManagerTest, MultipleClientConnections)
{
    m_connection_manager->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Lambda to perform a client request/response exchange
    auto perform_client_exchange = [&](int sock, fenris::RequestType cmd_type) {
        // Create request
        fenris::Request request;
        request.set_command(cmd_type);
        if (cmd_type == fenris::RequestType::READ_FILE) {
            request.set_filename("test.txt"); // Add filename for READ_FILE
        }

        // Serialize and send
        std::vector<uint8_t> serialized_request = serialize_request(request);
        ASSERT_TRUE(send_prefixed_data(sock, serialized_request));

        // Receive response
        std::vector<uint8_t> received_data;
        ASSERT_TRUE(receive_prefixed_data(sock, received_data));

        // Deserialize and verify
        fenris::Response response = deserialize_response(received_data);
        ASSERT_TRUE(response.success());
        // Check if message matches the command name echoed by mock handler
        ASSERT_EQ(response.data(),
                  request.command() == fenris::RequestType::READ_FILE
                      ? "READ_FILE"
                      : (request.command() == fenris::RequestType::LIST_DIR
                             ? "LIST_DIR"
                             : "PING")); // Adjust based on command type
    };

    auto terminate_client = [&](int sock) {
        fenris::Request terminate_request;
        terminate_request.set_command(fenris::RequestType::TERMINATE);
        std::vector<uint8_t> serialized_terminate_request =
            serialize_request(terminate_request);
        ASSERT_TRUE(send_prefixed_data(sock, serialized_terminate_request));
        // Wait for server to process and respond
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        // Receive the response
        std::vector<uint8_t> terminate_received_data;
        ASSERT_TRUE(receive_prefixed_data(sock, terminate_received_data));

        // Deserialize and verify the response
        fenris::Response terminate_response =
            deserialize_response(terminate_received_data);
        ASSERT_TRUE(terminate_response.success());
        ASSERT_EQ(terminate_response.data(), "TERMINATE");
    };

    int client1 = connect_test_client();
    int client2 = connect_test_client();
    int client3 = connect_test_client();

    ASSERT_GE(client1, 0);
    ASSERT_GE(client2, 0);
    ASSERT_GE(client3, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_EQ(m_connection_manager->get_active_client_count(), 3);

    perform_client_exchange(client1, fenris::RequestType::PING);
    perform_client_exchange(
        client2,
        fenris::RequestType::READ_FILE); // Use different command
    perform_client_exchange(
        client3,
        fenris::RequestType::LIST_DIR); // Use another command

    // Wait for server processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto handled_sockets = m_handler->get_handled_client_ids();
    ASSERT_EQ(handled_sockets.size(),
              3); // Check if handler processed 3 requests

    auto received_requests = m_handler->get_received_requests();
    ASSERT_EQ(received_requests.size(), 3);
    ASSERT_EQ(received_requests[0].command(), fenris::RequestType::PING);
    ASSERT_EQ(received_requests[1].command(), fenris::RequestType::READ_FILE);
    ASSERT_EQ(received_requests[2].command(), fenris::RequestType::LIST_DIR);

    // Terminate all clients
    terminate_client(client1);
    terminate_client(client2);
    terminate_client(client3);

    // safe_stop_server(); // Let TearDown handle stop
}

// Test client disconnection handling
TEST_F(ServerConnectionManagerTest, ClientDisconnection)
{
    m_connection_manager->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int client_sock = connect_test_client();
    ASSERT_GE(client_sock, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(m_connection_manager->get_active_client_count(), 1);

    // Send a request to ensure connection is fully handled before closing
    fenris::Request ping_request;
    ping_request.set_command(fenris::RequestType::PING);

    std::vector<uint8_t> serialized_request = serialize_request(ping_request);
    ASSERT_TRUE(send_prefixed_data(client_sock, serialized_request));
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100)); // Give time to process

    // Close the client socket abruptly
    close(client_sock);
    // Remove from list so TearDown doesn't try to close again
    m_client_sockets.erase(std::remove(m_client_sockets.begin(),
                                       m_client_sockets.end(),
                                       client_sock),
                           m_client_sockets.end());

    // Wait for server to detect disconnection
    bool disconnected = false;
    for (int i = 0; i < 20; i++) { // Increased timeout/checks
        if (m_connection_manager->get_active_client_count() == 0) {
            disconnected = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    ASSERT_TRUE(disconnected) << "Server did not detect client disconnection";
    ASSERT_EQ(m_connection_manager->get_active_client_count(), 0);

    // safe_stop_server(); // Let TearDown handle stop
}

// Test handling different request types (simulated)
TEST_F(ServerConnectionManagerTest, HandleDifferentRequestTypes)
{
    m_connection_manager->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int client_sock = connect_test_client();
    ASSERT_GE(client_sock, 0);

    // Wait for connection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Create and send a READ_FILE request
    fenris::Request read_request;
    read_request.set_command(fenris::RequestType::READ_FILE);

    read_request.set_filename("example.dat");

    std::vector<uint8_t> serialized_read_req = serialize_request(read_request);
    ASSERT_TRUE(send_prefixed_data(client_sock, serialized_read_req));

    // Receive and verify response (mock just echoes command name)
    std::vector<uint8_t> read_response_data;
    ASSERT_TRUE(receive_prefixed_data(client_sock, read_response_data));
    fenris::Response read_response = deserialize_response(read_response_data);
    ASSERT_TRUE(read_response.success());
    ASSERT_EQ(read_response.data(), "READ_FILE");

    // Create and send a WRITE_FILE request
    fenris::Request write_request;
    write_request.set_command(fenris::RequestType::WRITE_FILE);

    write_request.set_filename("output.log");
    // write_request.set_data("Log data here"); // Assuming data is part of
    // request proto

    std::vector<uint8_t> serialized_write_req =
        serialize_request(write_request);
    ASSERT_TRUE(send_prefixed_data(client_sock, serialized_write_req));

    // Receive and verify response
    std::vector<uint8_t> write_response_data;
    ASSERT_TRUE(receive_prefixed_data(client_sock, write_response_data));
    fenris::Response write_response = deserialize_response(write_response_data);
    ASSERT_TRUE(write_response.success());
    ASSERT_EQ(write_response.data(), "WRITE_FILE");

    // Give some time for server to process
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Check that handler received the expected requests
    auto received_requests = m_handler->get_received_requests();
    ASSERT_EQ(received_requests.size(), 2);

    // Verify first request (READ_FILE)
    ASSERT_EQ(received_requests[0].command(), fenris::RequestType::READ_FILE);
    ASSERT_EQ(received_requests[0].filename(), "example.dat");

    // Verify second request (WRITE_FILE)
    ASSERT_EQ(received_requests[1].command(), fenris::RequestType::WRITE_FILE);

    ASSERT_EQ(received_requests[1].filename(), "output.log");
    // ASSERT_EQ(received_requests[1].data(), "Log data here");

    // Send terminate request
    fenris::Request terminate_request;
    terminate_request.set_command(fenris::RequestType::TERMINATE);
    std::vector<uint8_t> serialized_terminate_request =
        serialize_request(terminate_request);
    ASSERT_TRUE(send_prefixed_data(client_sock, serialized_terminate_request));
    std::cout << "Sent terminate request: " << terminate_request.DebugString()
              << std::endl;
    // Wait for server to process and respond
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // Receive the response
    std::vector<uint8_t> terminate_received_data;
    ASSERT_TRUE(receive_prefixed_data(client_sock, terminate_received_data));
    std::cout << "Received terminate data size: "
              << terminate_received_data.size() << std::endl;

    // safe_stop_server(); // Let TearDown handle stop
}

} // namespace tests
} // namespace server
} // namespace fenris
