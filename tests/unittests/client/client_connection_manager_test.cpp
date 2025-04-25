#include "client/client.hpp"
#include "client/connection_manager.hpp"
#include "common/crypto_manager.hpp"
#include "common/network_utils.hpp"
#include "common/request.hpp"
#include "common/response.hpp"
#include "fenris.pb.h"

#include <arpa/inet.h>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fcntl.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fenris {
namespace client {
namespace tests {

using namespace fenris::common;
using namespace fenris::common::network;
using namespace google::protobuf::util;

// --- Mock Server Implementation ---
class MockServer {
  public:
    MockServer()
        : m_port(0), m_listen_socket(-1), m_client_socket(-1), m_running(false)
    {
    }

    ~MockServer()
    {
        stop();
    }

    bool start()
    {
        m_listen_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_listen_socket < 0) {
            perror("MockServer: socket creation failed");
            return false;
        }

        int yes = 1;
        setsockopt(m_listen_socket,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &yes,
                   sizeof(yes));

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = 0;

        if (bind(m_listen_socket,
                 (struct sockaddr *)&server_addr,
                 sizeof(server_addr)) < 0) {
            perror("MockServer: bind failed");
            close(m_listen_socket);
            m_listen_socket = -1;
            return false;
        }

        socklen_t len = sizeof(server_addr);
        if (getsockname(m_listen_socket,
                        (struct sockaddr *)&server_addr,
                        &len) == -1) {
            perror("MockServer: getsockname failed");
            close(m_listen_socket);
            m_listen_socket = -1;
            return false;
        }
        m_port = ntohs(server_addr.sin_port);

        if (listen(m_listen_socket, 1) < 0) {
            perror("MockServer: listen failed");
            close(m_listen_socket);
            m_listen_socket = -1;
            return false;
        }

        m_running = true;
        m_server_thread = std::thread(&MockServer::run, this);
        std::cout << "MockServer started on port " << m_port << std::endl;
        return true;
    }

    void stop()
    {
        if (!m_running.exchange(false)) {
            return;
        }

        if (m_listen_socket != -1) {
            int wake_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (wake_socket >= 0) {
                sockaddr_in addr{};
                addr.sin_family = AF_INET;
                addr.sin_port = htons(m_port);
                inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
                ::connect(wake_socket, (struct sockaddr *)&addr, sizeof(addr));
                close(wake_socket);
            }
            close(m_listen_socket);
            m_listen_socket = -1;
        }
        {
            std::lock_guard<std::mutex> lock(m_client_mutex);
            if (m_client_socket != -1) {
                shutdown(m_client_socket, SHUT_RDWR);
                close(m_client_socket);
                m_client_socket = -1;
            }
        }

        if (m_server_thread.joinable()) {
            m_server_thread.join();
        }
        std::cout << "MockServer stopped" << std::endl;
    }

    int get_port() const
    {
        return m_port;
    }

    std::vector<fenris::Request> get_received_requests()
    {
        std::lock_guard<std::mutex> lock(m_requests_mutex);
        return m_received_requests;
    }

    void set_next_response(const fenris::Response &response)
    {
        std::lock_guard<std::mutex> lock(m_response_mutex);
        m_next_response = response;
    }

    const std::vector<uint8_t> &get_encryption_key() const
    {
        return m_encryption_key;
    }

  private:
    void run()
    {
        std::cout << "MockServer thread running..." << std::endl;
        while (m_running) {
            struct sockaddr_storage client_addr;
            socklen_t sin_size = sizeof(client_addr);
            int temp_client_socket = accept(m_listen_socket,
                                            (struct sockaddr *)&client_addr,
                                            &sin_size);

            if (!m_running)
                break; // Check after accept returns

            if (temp_client_socket < 0) {
                if (errno != EBADF && errno != EINVAL) {
                    perror("MockServer: accept failed");
                }
                continue;
            }

            std::cout << "MockServer accepted connection" << std::endl;
            {
                std::lock_guard<std::mutex> lock(m_client_mutex);
                m_client_socket = temp_client_socket;
            }

            handle_client(m_client_socket);

            {
                std::lock_guard<std::mutex> lock(m_client_mutex);
                if (m_client_socket != -1) {
                    close(m_client_socket);
                    m_client_socket = -1;
                }
            }
            std::cout << "MockServer client disconnected" << std::endl;
        }
        std::cout << "MockServer thread exiting." << std::endl;
    }

    void handle_client(int sock)
    {
        // First handle key exchange
        if (!perform_key_exchange(sock)) {
            std::cerr << "MockServer: Key exchange failed" << std::endl;
            return;
        }

        std::cout << "MockServer: Key exchange successful" << std::endl;

        while (m_running) {
            std::vector<uint8_t> request_data;
            NetworkResult receive_result =
                receive_prefixed_data(sock, request_data);
            if (receive_result != NetworkResult::SUCCESS) {
                if (m_running) {
                    std::cerr << "MockServer: Failed to receive request data "
                                 "or client disconnected."
                              << std::endl;
                }
                break;
            }

            fenris::Request request = deserialize_request(request_data);
            {
                std::lock_guard<std::mutex> lock(m_requests_mutex);
                m_received_requests.push_back(request);
            }
            std::cout << "MockServer received request: " << request.command()
                      << std::endl;

            fenris::Response response_to_send;
            {
                std::lock_guard<std::mutex> lock(m_response_mutex);
                if (m_next_response.IsInitialized()) {
                    response_to_send = m_next_response;
                    m_next_response.Clear();
                } else {

                    response_to_send.set_success(true);
                    response_to_send.set_type(fenris::ResponseType::PONG);
                    response_to_send.set_data("PONG");
                }
            }

            std::vector<uint8_t> response_data =
                serialize_response(response_to_send);

            NetworkResult send_result = send_prefixed_data(sock, response_data);
            if (send_result != NetworkResult::SUCCESS) {
                std::cerr << "MockServer: Failed to send response data."
                          << std::endl;
                break;
            }
            std::cout << "MockServer sent response." << std::endl;

            // Special handling for TERMINATE
            if (request.command() == fenris::RequestType::TERMINATE) {
                std::cout
                    << "MockServer received TERMINATE, closing connection."
                    << std::endl;
                break;
            }
        }
    }

    // Perform key exchange with the client
    bool perform_key_exchange(int sock)
    {
        common::crypto::CryptoManager crypto_manager;

        // Generate ECDH keypair for the server
        auto [private_key, public_key, keygen_result] =
            crypto_manager.generate_ecdh_keypair();
        if (keygen_result != common::crypto::ECDHResult::SUCCESS) {
            std::cerr << "Failed to generate server ECDH keypair" << std::endl;
            return false;
        }

        // Receive the client's public key size
        std::vector<uint8_t> client_public_key;
        NetworkResult recv_result =
            receive_prefixed_data(sock, client_public_key);
        if (recv_result != NetworkResult::SUCCESS) {
            std::cerr << "Failed to receive client public key" << std::endl;
            return false;
        }

        // Send our public key to the client
        NetworkResult send_result = send_prefixed_data(sock, public_key);
        if (send_result != NetworkResult::SUCCESS) {
            std::cerr << "Failed to send server public key" << std::endl;
            return false;
        }

        // Compute the shared secret
        auto [shared_secret, ss_result] =
            crypto_manager.compute_ecdh_shared_secret(private_key,
                                                      client_public_key);
        if (ss_result != common::crypto::ECDHResult::SUCCESS) {
            std::cerr << "Failed to compute shared secret" << std::endl;
            return false;
        }

        // Derive the encryption key
        auto [derived_key, key_derive_result] =
            crypto_manager.derive_key_from_shared_secret(shared_secret, 16);
        if (key_derive_result != common::crypto::ECDHResult::SUCCESS) {
            std::cerr << "Failed to derive key from shared secret" << std::endl;
            return false;
        }

        // Store the derived key
        m_encryption_key = derived_key;
        return true;
    }

    int m_port;
    int m_listen_socket;
    int m_client_socket;
    std::atomic<bool> m_running;
    std::thread m_server_thread;
    std::mutex m_client_mutex;

    std::vector<fenris::Request> m_received_requests;
    std::mutex m_requests_mutex;

    fenris::Response m_next_response;
    std::mutex m_response_mutex;

    // Store encryption key derived from key exchange
    std::vector<uint8_t> m_encryption_key;
};

class ClientConnectionManagerTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        m_mock_server = std::make_unique<MockServer>();
        ASSERT_TRUE(m_mock_server->start());
        m_port = m_mock_server->get_port();
        m_port_str = std::to_string(m_port);

        m_connection_manager =
            std::make_unique<fenris::client::ConnectionManager>(
                "127.0.0.1",
                m_port_str,
                "TestClientConnectionManager");
    }

    void TearDown() override
    {
        if (m_connection_manager) {
            m_connection_manager->disconnect();
        }
        if (m_mock_server) {
            m_mock_server->stop();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::unique_ptr<fenris::client::ConnectionManager> m_connection_manager;
    std::unique_ptr<MockServer> m_mock_server;
    int m_port;
    std::string m_port_str;
};

TEST_F(ClientConnectionManagerTest, ConnectAndDisconnect)
{
    ASSERT_FALSE(m_connection_manager->is_connected());
    ASSERT_TRUE(m_connection_manager->connect());
    ASSERT_TRUE(m_connection_manager->is_connected());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    m_connection_manager->disconnect();
    ASSERT_FALSE(m_connection_manager->is_connected());
}

TEST_F(ClientConnectionManagerTest, ConnectionFailure)
{
    m_mock_server->stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_FALSE(m_connection_manager->is_connected());
    // Try connecting to the now-stopped server
    ASSERT_FALSE(m_connection_manager->connect());
    ASSERT_FALSE(m_connection_manager->is_connected());
}

TEST_F(ClientConnectionManagerTest, SendRequest)
{
    ASSERT_TRUE(m_connection_manager->connect());
    ASSERT_TRUE(m_connection_manager->is_connected());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    fenris::Request ping_request;
    ping_request.set_command(fenris::RequestType::PING);
    ping_request.set_data("TestPing");

    ASSERT_TRUE(m_connection_manager->send_request(ping_request));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto received_requests = m_mock_server->get_received_requests();
    ASSERT_EQ(received_requests.size(), 1);
    ASSERT_TRUE(MessageDifferencer::Equals(received_requests[0], ping_request));

    m_connection_manager->disconnect();
}

TEST_F(ClientConnectionManagerTest, ReceiveResponse)
{
    ASSERT_TRUE(m_connection_manager->connect());
    ASSERT_TRUE(m_connection_manager->is_connected());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    fenris::Response expected_response;
    expected_response.set_success(true);
    expected_response.set_type(fenris::ResponseType::PONG);
    expected_response.set_data("TestPong");
    m_mock_server->set_next_response(expected_response);

    fenris::Request dummy_request;
    dummy_request.set_command(fenris::RequestType::PING);
    dummy_request.set_data("DummyPingData");
    ASSERT_TRUE(m_connection_manager->send_request(dummy_request));

    auto received_response_opt = m_connection_manager->receive_response();
    ASSERT_TRUE(received_response_opt.has_value());

    ASSERT_TRUE(MessageDifferencer::Equals(received_response_opt.value(),
                                           expected_response));

    m_connection_manager->disconnect();
}

TEST_F(ClientConnectionManagerTest, SendAndReceiveMultiple)
{
    ASSERT_TRUE(m_connection_manager->connect());
    ASSERT_TRUE(m_connection_manager->is_connected());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    fenris::Request ping_request;
    ping_request.set_command(fenris::RequestType::PING);
    ping_request.set_data("Ping1");

    fenris::Response pong_response;
    pong_response.set_success(true);
    pong_response.set_type(fenris::ResponseType::PONG);
    pong_response.set_data("Pong1");
    m_mock_server->set_next_response(pong_response);

    ASSERT_TRUE(m_connection_manager->send_request(ping_request));
    auto received_pong_opt = m_connection_manager->receive_response();
    ASSERT_TRUE(received_pong_opt.has_value());
    ASSERT_TRUE(
        MessageDifferencer::Equals(received_pong_opt.value(), pong_response));

    fenris::Request read_request;
    read_request.set_command(fenris::RequestType::READ_FILE);
    read_request.set_filename("test.txt");

    fenris::Response file_response;
    file_response.set_success(true);
    file_response.set_type(fenris::ResponseType::FILE_CONTENT);
    file_response.set_data("File data");
    m_mock_server->set_next_response(file_response);

    ASSERT_TRUE(m_connection_manager->send_request(read_request));
    auto received_file_opt = m_connection_manager->receive_response();
    ASSERT_TRUE(received_file_opt.has_value());
    ASSERT_TRUE(
        MessageDifferencer::Equals(received_file_opt.value(), file_response));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto received_requests = m_mock_server->get_received_requests();
    ASSERT_EQ(received_requests.size(), 2);
    ASSERT_TRUE(MessageDifferencer::Equals(received_requests[0], ping_request));
    ASSERT_TRUE(MessageDifferencer::Equals(received_requests[1], read_request));

    m_connection_manager->disconnect();
}

TEST_F(ClientConnectionManagerTest, ECDHKeyExchangeProducesMatchingKeys)
{
    // Connect to the mock server
    ASSERT_TRUE(m_connection_manager->connect());
    ASSERT_TRUE(m_connection_manager->is_connected());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const std::vector<uint8_t> &client_key =
        m_connection_manager->get_encryption_key();
    ASSERT_FALSE(client_key.empty()) << "Client encryption key is empty";

    const std::vector<uint8_t> &server_key =
        m_mock_server->get_encryption_key();
    ASSERT_FALSE(server_key.empty()) << "Server encryption key is empty";

    ASSERT_EQ(client_key.size(), server_key.size()) << "Key sizes don't match";
    for (size_t i = 0; i < client_key.size(); i++) {
        ASSERT_EQ(client_key[i], server_key[i])
            << "Key mismatch at index " << i;
    }

    // Clean up
    m_connection_manager->disconnect();
}

} // namespace tests
} // namespace client
} // namespace fenris
