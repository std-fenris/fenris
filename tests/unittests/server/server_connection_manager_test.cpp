#include "common/crypto_manager.hpp"
#include "common/network_utils.hpp"
#include "common/request.hpp"
#include "common/response.hpp"
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
#include <vector>

namespace fenris {
namespace server {
namespace tests {

using namespace fenris::common;
using namespace fenris::common::network;
using namespace google::protobuf::util;

// Mock implementation of ClientHandler for testing
class MockClientHandler : public ClientHandler {
  public:
    explicit MockClientHandler(bool keep_connection = true,
                               int max_requests = 10)
        : m_keep_connection(keep_connection), m_max_requests(max_requests),
          m_request_count(0)
    {
    }

    std::pair<fenris::Response, bool>
    handle_request(uint32_t client_socket,
                   const fenris::Request &request) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_handled_client_sockets.push_back(client_socket);
        m_received_requests.push_back(request);
        m_request_count++;

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
            m_keep_connection = false;
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

    std::vector<fenris::Request> get_received_requests()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_received_requests;
    }

    int get_request_count()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_request_count;
    }

  private:
    bool m_keep_connection;
    int m_max_requests;
    int m_request_count;
    std::mutex m_mutex;
    std::vector<uint32_t> m_handled_client_sockets;
    std::vector<fenris::Request> m_received_requests;
};

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

bool perform_client_key_exchange(int sock, std::vector<uint8_t> &shared_key)
{
    crypto::CryptoManager crypto_manager;

    auto [private_key, public_key, keygen_result] =
        crypto_manager.generate_ecdh_keypair();
    if (keygen_result != crypto::ECDHResult::SUCCESS) {
        std::cerr << "Failed to generate client ECDH keypair: "
                  << ecdh_result_to_string(keygen_result) << std::endl;
        return false;
    }

    NetworkResult send_result = send_prefixed_data(sock, public_key);
    if (send_result != NetworkResult::SUCCESS) {
        std::cerr << "Failed to send client public key: "
                  << network_result_to_string(send_result) << std::endl;
        return false;
    }

    std::vector<uint8_t> server_public_key;
    NetworkResult receive_result =
        receive_prefixed_data(sock, server_public_key);
    if (receive_result != NetworkResult::SUCCESS) {
        std::cerr << "Failed to receive server public key: "
                  << network_result_to_string(receive_result) << std::endl;
        return false;
    }

    auto [shared_secret, ss_result] =
        crypto_manager.compute_ecdh_shared_secret(private_key,
                                                  server_public_key);
    if (ss_result != crypto::ECDHResult::SUCCESS) {
        std::cerr << "Failed to compute shared secret: "
                  << ecdh_result_to_string(ss_result) << std::endl;
        return false;
    }

    auto [derived_key, key_derive_result] =
        crypto_manager.derive_key_from_shared_secret(shared_secret, 16);
    if (key_derive_result != crypto::ECDHResult::SUCCESS) {
        std::cerr << "Failed to derive key from shared secret: "
                  << ecdh_result_to_string(key_derive_result) << std::endl;
        return false;
    }

    shared_key = derived_key;
    return true;
}

class ServerConnectionManagerTest : public ::testing::Test {
  protected:
    void SetUp() override
    {

        m_port = 12345 + (rand() % 1000);
        m_port_str = std::to_string(m_port);

        auto handler_ptr = std::make_unique<MockClientHandler>(true, 10);
        m_mock_handler_ptr = handler_ptr.get();
        m_connection_manager =
            std::make_unique<ConnectionManager>("127.0.0.1",
                                                m_port_str,
                                                "TestServerConnectionManager");

        m_connection_manager->set_client_handler(std::move(handler_ptr));
        m_connection_manager->set_non_blocking_mode(false);
    }

    void TearDown() override
    {

        if (m_connection_manager) {
            m_connection_manager->stop();

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

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

            std::vector<uint8_t> shared_key;
            if (!perform_client_key_exchange(client_sock, shared_key)) {
                std::cerr << "Key exchange failed during test client connection"
                          << std::endl;
                close(client_sock);
                m_client_sockets.pop_back();
                return -1;
            }
            std::cout << "Key exchange successful for client socket: "
                      << client_sock << std::endl;
        }
        return client_sock;
    }

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
                                     reinterpret_cast<sockaddr *>(&server_addr),
                                     sizeof(server_addr));

                if (result < 0 && errno == EINPROGRESS) {
                    for (int i = 0; i < 5; i++) {
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(50));

                        int err = 0;
                        socklen_t len = sizeof(err);
                        getsockopt(test_socket,
                                   SOL_SOCKET,
                                   SO_ERROR,
                                   &err,
                                   &len);
                        if (!err) {
                            FAIL() << "Connection succeeded; server may not be "
                                      "fully stopped.";
                            break;
                        } else if (err == ECONNREFUSED || err == ETIMEDOUT) {

                            break;
                        }
                    }
                } else if (result == 0) {
                    FAIL() << "Connection succeeded immediately; server may "
                              "not be fully stopped.";
                }
                close(test_socket);
            }
        }
    }

    std::unique_ptr<ConnectionManager> m_connection_manager;

    MockClientHandler *m_mock_handler_ptr = nullptr;
    int m_port;
    std::string m_port_str;
    std::vector<int> m_client_sockets;
};

TEST_F(ServerConnectionManagerTest, StartAndStop)
{
    ASSERT_EQ(m_connection_manager->get_active_client_count(), 0);

    m_connection_manager->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    safe_stop_server();

    ASSERT_EQ(m_connection_manager->get_active_client_count(), 0);
}

TEST_F(ServerConnectionManagerTest, AcceptClientConnection)
{
    m_connection_manager->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int client_sock = connect_test_client();
    ASSERT_GE(client_sock, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(m_connection_manager->get_active_client_count(), 1);

    fenris::Request ping_request;
    ping_request.set_command(fenris::RequestType::PING);

    std::vector<uint8_t> serialized_request = serialize_request(ping_request);
    ASSERT_EQ(send_prefixed_data(client_sock, serialized_request),
              NetworkResult::SUCCESS);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::vector<uint8_t> received_data;
    ASSERT_EQ(receive_prefixed_data(client_sock, received_data),
              NetworkResult::SUCCESS);

    fenris::Response response = deserialize_response(received_data);
    ASSERT_TRUE(response.success());
    ASSERT_EQ(response.data(), "PING");

    auto received_requests = m_mock_handler_ptr->get_received_requests();
    ASSERT_EQ(received_requests.size(), 1);
    ASSERT_TRUE(MessageDifferencer::Equals(received_requests[0], ping_request));

    fenris::Request terminate_request;
    terminate_request.set_command(fenris::RequestType::TERMINATE);
    std::vector<uint8_t> serialized_terminate_request =
        serialize_request(terminate_request);
    ASSERT_EQ(send_prefixed_data(client_sock, serialized_terminate_request),
              NetworkResult::SUCCESS);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::vector<uint8_t> terminate_received_data;
    ASSERT_EQ(receive_prefixed_data(client_sock, terminate_received_data),
              NetworkResult::SUCCESS);

    fenris::Response terminate_response =
        deserialize_response(terminate_received_data);
    ASSERT_TRUE(terminate_response.success());
    ASSERT_EQ(terminate_response.data(), "TERMINATE");

    // Close client socket (will be handled by TearDown)
}

TEST_F(ServerConnectionManagerTest, MultipleClientConnections)
{
    m_connection_manager->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto perform_client_exchange = [&](int sock, fenris::RequestType cmd_type) {
        fenris::Request request;
        request.set_command(cmd_type);
        if (cmd_type == fenris::RequestType::READ_FILE) {
            request.set_filename("test.txt");
        }

        std::vector<uint8_t> serialized_request = serialize_request(request);
        ASSERT_EQ(send_prefixed_data(sock, serialized_request),
                  NetworkResult::SUCCESS);

        std::vector<uint8_t> received_data;
        ASSERT_EQ(receive_prefixed_data(sock, received_data),
                  NetworkResult::SUCCESS);

        fenris::Response response = deserialize_response(received_data);
        ASSERT_TRUE(response.success());

        ASSERT_EQ(response.data(),
                  request.command() == fenris::RequestType::READ_FILE
                      ? "READ_FILE"
                      : (request.command() == fenris::RequestType::LIST_DIR
                             ? "LIST_DIR"
                             : "PING"));
    };

    auto terminate_client = [&](int sock) {
        fenris::Request terminate_request;
        terminate_request.set_command(fenris::RequestType::TERMINATE);
        std::vector<uint8_t> serialized_terminate_request =
            serialize_request(terminate_request);
        ASSERT_EQ(send_prefixed_data(sock, serialized_terminate_request),
                  NetworkResult::SUCCESS);

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::vector<uint8_t> terminate_received_data;
        ASSERT_EQ(receive_prefixed_data(sock, terminate_received_data),
                  NetworkResult::SUCCESS);

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
    perform_client_exchange(client2, fenris::RequestType::READ_FILE);
    perform_client_exchange(client3, fenris::RequestType::LIST_DIR);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto handled_sockets = m_mock_handler_ptr->get_handled_client_ids();
    ASSERT_EQ(handled_sockets.size(), 3);

    auto received_requests = m_mock_handler_ptr->get_received_requests();
    ASSERT_EQ(received_requests.size(), 3);
    ASSERT_EQ(received_requests[0].command(), fenris::RequestType::PING);
    ASSERT_EQ(received_requests[1].command(), fenris::RequestType::READ_FILE);
    ASSERT_EQ(received_requests[2].command(), fenris::RequestType::LIST_DIR);

    terminate_client(client1);
    terminate_client(client2);
    terminate_client(client3);
}

TEST_F(ServerConnectionManagerTest, ClientDisconnection)
{
    m_connection_manager->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int client_sock = connect_test_client();
    ASSERT_GE(client_sock, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(m_connection_manager->get_active_client_count(), 1);

    fenris::Request ping_request;
    ping_request.set_command(fenris::RequestType::PING);

    std::vector<uint8_t> serialized_request = serialize_request(ping_request);
    ASSERT_EQ(send_prefixed_data(client_sock, serialized_request),
              NetworkResult::SUCCESS);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    close(client_sock);

    m_client_sockets.erase(std::remove(m_client_sockets.begin(),
                                       m_client_sockets.end(),
                                       client_sock),
                           m_client_sockets.end());

    bool disconnected = false;
    for (int i = 0; i < 20; i++) {
        if (m_connection_manager->get_active_client_count() == 0) {
            disconnected = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    ASSERT_TRUE(disconnected) << "Server did not detect client disconnection";
    ASSERT_EQ(m_connection_manager->get_active_client_count(), 0);
}

TEST_F(ServerConnectionManagerTest, HandleDifferentRequestTypes)
{
    m_connection_manager->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int client_sock = connect_test_client();
    ASSERT_GE(client_sock, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    fenris::Request read_request;
    read_request.set_command(fenris::RequestType::READ_FILE);

    read_request.set_filename("example.dat");

    std::vector<uint8_t> serialized_read_req = serialize_request(read_request);
    ASSERT_EQ(send_prefixed_data(client_sock, serialized_read_req),
              NetworkResult::SUCCESS);

    std::vector<uint8_t> read_response_data;
    ASSERT_EQ(receive_prefixed_data(client_sock, read_response_data),
              NetworkResult::SUCCESS);
    fenris::Response read_response = deserialize_response(read_response_data);
    ASSERT_TRUE(read_response.success());
    ASSERT_EQ(read_response.data(), "READ_FILE");

    fenris::Request write_request;
    write_request.set_command(fenris::RequestType::WRITE_FILE);

    write_request.set_filename("output.log");

    std::vector<uint8_t> serialized_write_req =
        serialize_request(write_request);
    ASSERT_EQ(send_prefixed_data(client_sock, serialized_write_req),
              NetworkResult::SUCCESS);

    std::vector<uint8_t> write_response_data;
    ASSERT_EQ(receive_prefixed_data(client_sock, write_response_data),
              NetworkResult::SUCCESS);
    fenris::Response write_response = deserialize_response(write_response_data);
    ASSERT_TRUE(write_response.success());
    ASSERT_EQ(write_response.data(), "WRITE_FILE");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto received_requests = m_mock_handler_ptr->get_received_requests();
    ASSERT_EQ(received_requests.size(), 2);

    ASSERT_EQ(received_requests[0].command(), fenris::RequestType::READ_FILE);
    ASSERT_EQ(received_requests[0].filename(), "example.dat");

    ASSERT_EQ(received_requests[1].command(), fenris::RequestType::WRITE_FILE);

    ASSERT_EQ(received_requests[1].filename(), "output.log");

    fenris::Request terminate_request;
    terminate_request.set_command(fenris::RequestType::TERMINATE);
    std::vector<uint8_t> serialized_terminate_request =
        serialize_request(terminate_request);
    ASSERT_EQ(send_prefixed_data(client_sock, serialized_terminate_request),
              NetworkResult::SUCCESS);
    std::cout << "Sent terminate request: " << terminate_request.DebugString()
              << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::vector<uint8_t> terminate_received_data;
    ASSERT_EQ(receive_prefixed_data(client_sock, terminate_received_data),
              NetworkResult::SUCCESS);
    std::cout << "Received terminate data size: "
              << terminate_received_data.size() << std::endl;
}

} // namespace tests
} // namespace server
} // namespace fenris
