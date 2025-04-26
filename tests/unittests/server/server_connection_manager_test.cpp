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
        crypto_manager.derive_key_from_shared_secret(shared_secret,
                                                     crypto::AES_GCM_KEY_SIZE);
    if (key_derive_result != crypto::ECDHResult::SUCCESS) {
        std::cerr << "Failed to derive key from shared secret: "
                  << ecdh_result_to_string(key_derive_result) << std::endl;
        return false;
    }

    shared_key = derived_key;
    return true;
}

bool send_request(const ClientInfo &client_info, const fenris::Request &request)
{
    crypto::CryptoManager m_crypto_manager;

    std::vector<uint8_t> serialized_request = serialize_request(request);

    // Generate a random IV
    auto [iv, iv_gen_result] = m_crypto_manager.generate_random_iv();
    if (iv_gen_result != crypto::EncryptionResult::SUCCESS) {
        std::cerr << "failed to generate random IV" << std::endl;
        return false;
    }

    // Encrypt the serialized request
    auto [encrypted_request, encrypt_result] =
        m_crypto_manager.encrypt_data(serialized_request,
                                      client_info.encryption_key,
                                      iv);
    if (encrypt_result != crypto::EncryptionResult::SUCCESS) {
        std::cerr << "failed to encrypt request: "
                  << encryption_result_to_string(encrypt_result) << std::endl;
        return false;
    }

    // Create the final message with IV prefixed to encrypted data
    std::vector<uint8_t> message_with_iv;
    message_with_iv.reserve(iv.size() + encrypted_request.size());
    message_with_iv.insert(message_with_iv.end(), iv.begin(), iv.end());
    message_with_iv.insert(message_with_iv.end(),
                           encrypted_request.begin(),
                           encrypted_request.end());

    // Send the IV-prefixed encrypted request
    NetworkResult send_result =
        send_prefixed_data(client_info.socket, message_with_iv);
    if (send_result != NetworkResult::SUCCESS) {
        std::cerr << "failed to send request: "
                  << network_result_to_string(send_result) << std::endl;
        return false;
    }

    return true;
}

std::optional<fenris::Response> receive_response(const ClientInfo &client_info)
{
    crypto::CryptoManager m_crypto_manager;

    // Receive encrypted data (includes IV + encrypted response)
    std::vector<uint8_t> encrypted_data;
    NetworkResult recv_result =
        receive_prefixed_data(client_info.socket, encrypted_data);
    if (recv_result != NetworkResult::SUCCESS) {
        return std::nullopt;
    }

    if (encrypted_data.size() < crypto::AES_GCM_IV_SIZE) {
        return std::nullopt;
    }

    // Extract IV from the beginning of the message
    std::vector<uint8_t> iv(encrypted_data.begin(),
                            encrypted_data.begin() + crypto::AES_GCM_IV_SIZE);

    // Extract the encrypted response data (after the IV)
    std::vector<uint8_t> encrypted_response(encrypted_data.begin() +
                                                crypto::AES_GCM_IV_SIZE,
                                            encrypted_data.end());

    // Decrypt the response using the extracted IV
    auto [decrypted_data, decrypt_result] =
        m_crypto_manager.decrypt_data(encrypted_response,
                                      client_info.encryption_key,
                                      iv);

    if (decrypt_result != crypto::EncryptionResult::SUCCESS) {
        return std::nullopt;
    }

    // Deserialize the response
    return deserialize_response(decrypted_data);
}

class ServerConnectionManagerTest : public ::testing::Test {
  protected:
    void SetUp() override
    {

        m_port = 12345 + (rand() % 1000);
        m_port_str = std::to_string(m_port);

        auto handler_ptr = std::make_unique<MockClientHandler>(true, 10);
        m_mock_handler_ptr = handler_ptr.get();

        LoggingConfig config;
        config.level = LogLevel::DEBUG;
        config.log_file_path = "test_server_connection_manager.log";
        config.console_logging = true;
        config.file_logging = true;
        initialize_logging(config, "TestServerConnectionManager");

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

    ClientInfo connect_test_client()
    {
        int client_sock = create_and_connect_client_socket("127.0.0.1", m_port);
        std::vector<uint8_t> shared_key;

        if (client_sock >= 0) {
            m_client_sockets.push_back(client_sock);

            if (!perform_client_key_exchange(client_sock, shared_key)) {
                std::cerr << "Key exchange failed during test client connection"
                          << std::endl;
                close(client_sock);
                m_client_sockets.pop_back();
                return ClientInfo{};
            }
            std::cout << "Key exchange successful for client socket: "
                      << client_sock << std::endl;
        }

        ClientInfo client_info;
        client_info.client_id = m_client_sockets.size();
        client_info.socket = client_sock;
        client_info.encryption_key = std::vector<uint8_t>(shared_key);
        client_info.address = "127.0.0.1";
        client_info.port = m_port_str;

        std::cout << "Client connected with ID: " << client_info.client_id
                  << std::endl;

        return client_info;
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

    ASSERT_EQ(m_connection_manager->get_active_client_count(), 0);
}

TEST_F(ServerConnectionManagerTest, AcceptClientConnection)
{
    m_connection_manager->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ClientInfo client = connect_test_client();
    ASSERT_GE(client.socket, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(m_connection_manager->get_active_client_count(), 1);

    fenris::Request ping_request;
    ping_request.set_command(fenris::RequestType::PING);

    ASSERT_TRUE(send_request(client, ping_request));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto response_opt = receive_response(client);
    ASSERT_TRUE(response_opt.has_value());
    ASSERT_TRUE(response_opt->success());
    ASSERT_EQ(response_opt->data(), "PING");

    auto received_requests = m_mock_handler_ptr->get_received_requests();
    ASSERT_EQ(received_requests.size(), 1);
    ASSERT_TRUE(MessageDifferencer::Equals(received_requests[0], ping_request));

    fenris::Request terminate_request;
    terminate_request.set_command(fenris::RequestType::TERMINATE);
    ASSERT_TRUE(send_request(client, terminate_request));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto terminate_response_opt = receive_response(client);
    ASSERT_TRUE(terminate_response_opt.has_value());
    ASSERT_TRUE(terminate_response_opt->success());
    ASSERT_EQ(terminate_response_opt->data(), "TERMINATE");

    // Close client socket (will be handled by TearDown)
}

TEST_F(ServerConnectionManagerTest, MultipleClientConnections)
{
    m_connection_manager->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto perform_client_exchange = [&](const ClientInfo &client_info,
                                       fenris::RequestType cmd_type) {
        fenris::Request request;
        request.set_command(cmd_type);
        if (cmd_type == fenris::RequestType::READ_FILE) {
            request.set_filename("test.txt");
        }

        ASSERT_TRUE(send_request(client_info, request));

        auto response_opt = receive_response(client_info);
        ASSERT_TRUE(response_opt.has_value());
        ASSERT_TRUE(response_opt->success());

        ASSERT_EQ(response_opt->data(),
                  request.command() == fenris::RequestType::READ_FILE
                      ? "READ_FILE"
                      : (request.command() == fenris::RequestType::LIST_DIR
                             ? "LIST_DIR"
                             : "PING"));
    };

    auto terminate_client = [&](const ClientInfo &client_info) {
        fenris::Request terminate_request;
        terminate_request.set_command(fenris::RequestType::TERMINATE);
        ASSERT_TRUE(send_request(client_info, terminate_request));

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        auto response_opt = receive_response(client_info);
        ASSERT_TRUE(response_opt.has_value());
        ASSERT_TRUE(response_opt->success());
        ASSERT_EQ(response_opt->data(), "TERMINATE");
    };

    ClientInfo client1 = connect_test_client();
    ClientInfo client2 = connect_test_client();
    ClientInfo client3 = connect_test_client();

    ASSERT_GE(client1.socket, 0);
    ASSERT_GE(client2.socket, 0);
    ASSERT_GE(client3.socket, 0);

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

    ClientInfo client = connect_test_client();
    ASSERT_GE(client.socket, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(m_connection_manager->get_active_client_count(), 1);

    fenris::Request ping_request;
    ping_request.set_command(fenris::RequestType::PING);

    ASSERT_TRUE(send_request(client, ping_request));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    close(client.socket);

    m_client_sockets.erase(std::remove(m_client_sockets.begin(),
                                       m_client_sockets.end(),
                                       client.socket),
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

    ClientInfo client = connect_test_client();
    ASSERT_GE(client.socket, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    fenris::Request read_request;
    read_request.set_command(fenris::RequestType::READ_FILE);
    read_request.set_filename("example.dat");

    ASSERT_TRUE(send_request(client, read_request));

    auto read_response_opt = receive_response(client);
    ASSERT_TRUE(read_response_opt.has_value());
    ASSERT_TRUE(read_response_opt->success());
    ASSERT_EQ(read_response_opt->data(), "READ_FILE");

    fenris::Request write_request;
    write_request.set_command(fenris::RequestType::WRITE_FILE);
    write_request.set_filename("output.log");

    ASSERT_TRUE(send_request(client, write_request));

    auto write_response_opt = receive_response(client);
    ASSERT_TRUE(write_response_opt.has_value());
    ASSERT_TRUE(write_response_opt->success());
    ASSERT_EQ(write_response_opt->data(), "WRITE_FILE");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto received_requests = m_mock_handler_ptr->get_received_requests();
    ASSERT_EQ(received_requests.size(), 2);

    ASSERT_EQ(received_requests[0].command(), fenris::RequestType::READ_FILE);
    ASSERT_EQ(received_requests[0].filename(), "example.dat");

    ASSERT_EQ(received_requests[1].command(), fenris::RequestType::WRITE_FILE);
    ASSERT_EQ(received_requests[1].filename(), "output.log");

    fenris::Request terminate_request;
    terminate_request.set_command(fenris::RequestType::TERMINATE);
    ASSERT_TRUE(send_request(client, terminate_request));
    std::cout << "Sent terminate request: " << terminate_request.DebugString()
              << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto terminate_response_opt = receive_response(client);
    ASSERT_TRUE(terminate_response_opt.has_value());
    ASSERT_TRUE(terminate_response_opt->success());
    ASSERT_EQ(terminate_response_opt->data(), "TERMINATE");
    std::cout << "Received terminate response: "
              << terminate_response_opt->DebugString() << std::endl;
}

} // namespace tests
} // namespace server
} // namespace fenris
