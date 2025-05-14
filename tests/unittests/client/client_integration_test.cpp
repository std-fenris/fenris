#include "client/client.hpp"
#include "client/colors.hpp"
#include "client/connection_manager.hpp"
#include "client/interface.hpp"
#include "common/crypto_manager.hpp"
#include "common/network_utils.hpp"
#include "common/request.hpp"
#include "common/response.hpp"
#include "fenris.pb.h"

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fenris {
namespace client {
namespace tests {

using namespace fenris::common;
using namespace fenris::common::network;

// Mock implementation of TUI for testing
class MockTUI : public ITUI {
  public:
    MockTUI()
        : m_next_command_index(0), m_connecting(false), m_port_number("7777")
    {
    }

    // Queue commands to be returned when get_command is called
    void queue_command(const std::vector<std::string> &command)
    {
        std::lock_guard<std::mutex> lock(m_command_mutex);
        m_commands.push_back(command);
    }

    // Get the results that were displayed
    std::vector<std::pair<bool, std::string>> get_displayed_results()
    {
        std::lock_guard<std::mutex> lock(m_results_mutex);
        return m_displayed_results;
    }

    // Clear the displayed results for the next test
    void clear_displayed_results()
    {
        std::lock_guard<std::mutex> lock(m_results_mutex);
        m_displayed_results.clear();
    }

    // ITUI interface implementation
    std::string get_server_IP() override
    {
        return "127.0.0.1"; // Always return localhost for testing
    }

    std::string get_port_number() override
    {
        return m_port_number; // Return the port number set by the test
    }

    void set_port_number(const std::string &port_number)
    {
        std::lock_guard<std::mutex> lock(m_command_mutex);
        m_port_number = port_number;
    }

    std::vector<std::string> get_command() override
    {
        std::lock_guard<std::mutex> lock(m_command_mutex);
        if (m_next_command_index < m_commands.size()) {
            return m_commands[m_next_command_index++];
        }

        // No more commands, return exit to stop the client
        return {"exit"};
    }

    void display_result(bool success, const std::string &result) override
    {
        std::lock_guard<std::mutex> lock(m_results_mutex);
        m_displayed_results.push_back({success, result});
    }

    void update_current_directory(const std::string &dir) override
    {
        // Not needed for testing
    }

    std::string get_current_directory() const override
    {
        return "/"; // Always return root for testing
    }

    void display_help() override
    {
        std::lock_guard<std::mutex> lock(m_results_mutex);
        m_displayed_results.push_back({true,
                                       "Available commands:\n"
                                       "1. help - Show this help message\n"
                                       "2. exit - Exit the client\n"});
    }

    ~MockTUI() override
    {
        std::lock_guard<std::mutex> lock(m_results_mutex);
        m_displayed_results.clear();
    }

  private:
    std::vector<std::vector<std::string>> m_commands;
    std::vector<std::pair<bool, std::string>> m_displayed_results;
    std::mutex m_command_mutex;
    std::mutex m_results_mutex;
    size_t m_next_command_index;
    bool m_connecting;
    std::string m_port_number;
};

// Mock Server implementation for testing
class MockServer {
  public:
    MockServer()
        : m_port(0), m_listen_socket(-1), m_client_socket(-1), m_running(false),
          m_current_dir("/")
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
        server_addr.sin_port = 0; // Let OS assign a port

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

    void clear_requests()
    {
        std::lock_guard<std::mutex> lock(m_requests_mutex);
        m_received_requests.clear();
    }

    // Virtual filesystem functions for the mock server
    void add_file(const std::string &path, const std::string &content)
    {
        std::lock_guard<std::mutex> lock(m_fs_mutex);
        m_files[path] = content;
    }

    void add_directory(const std::string &path)
    {
        std::lock_guard<std::mutex> lock(m_fs_mutex);
        m_directories.insert(path);
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

    void handle_client(uint32_t socket)
    {
        // First handle key exchange
        std::vector<uint8_t> encryption_key;

        if (!perform_key_exchange(socket, encryption_key)) {
            std::cerr << "MockServer: Key exchange failed" << std::endl;
            return;
        }

        std::cout << "MockServer: Key exchange successful" << std::endl;
        m_encryption_key = encryption_key;

        while (m_running) {
            auto request_opt = receive_request(socket, encryption_key);
            if (!request_opt.has_value()) {
                std::cerr << "MockServer: Failed to receive request."
                          << std::endl;
                break;
            }

            const fenris::Request &request = request_opt.value();

            {
                std::lock_guard<std::mutex> lock(m_requests_mutex);
                m_received_requests.push_back(request);
            }

            std::cout << "MockServer received request: " << request.command()
                      << std::endl;

            fenris::Response response_to_send;
            {
                std::lock_guard<std::mutex> lock(m_response_mutex);
                response_to_send = create_default_response(request);
            }

            if (!send_response(socket, encryption_key, response_to_send)) {
                std::cerr << "MockServer: Failed to send response."
                          << std::endl;
                break;
            }

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
    bool perform_key_exchange(uint32_t socket,
                              std::vector<uint8_t> &encryption_key)
    {
        crypto::CryptoManager crypto_manager;

        // Generate ECDH keypair for the server
        auto [private_key, public_key, keygen_result] =
            crypto_manager.generate_ecdh_keypair();
        if (keygen_result != crypto::ECDHResult::SUCCESS) {
            std::cerr << "Failed to generate server ECDH keypair: "
                      << crypto::ecdh_result_to_string(keygen_result)
                      << std::endl;
            return false;
        }

        // Receive the client's public key
        std::vector<uint8_t> client_public_key;
        NetworkResult recv_result =
            receive_prefixed_data(socket, client_public_key);
        if (recv_result != NetworkResult::SUCCESS) {
            std::cerr << "Failed to receive client public key: "
                      << network_result_to_string(recv_result) << std::endl;
            return false;
        }

        // Send our public key to the client
        NetworkResult send_result = send_prefixed_data(socket, public_key);
        if (send_result != NetworkResult::SUCCESS) {
            std::cerr << "Failed to send server public key: "
                      << network_result_to_string(send_result) << std::endl;
            return false;
        }

        // Compute the shared secret
        auto [shared_secret, ss_result] =
            crypto_manager.compute_ecdh_shared_secret(private_key,
                                                      client_public_key);
        if (ss_result != crypto::ECDHResult::SUCCESS) {
            std::cerr << "Failed to compute shared secret: "
                      << crypto::ecdh_result_to_string(ss_result) << std::endl;
            return false;
        }

        // Derive the encryption key
        auto [derived_key, key_derive_result] =
            crypto_manager.derive_key_from_shared_secret(
                shared_secret,
                crypto::AES_GCM_KEY_SIZE);
        if (key_derive_result != crypto::ECDHResult::SUCCESS) {
            std::cerr << "Failed to derive key from shared secret: "
                      << crypto::ecdh_result_to_string(key_derive_result)
                      << std::endl;
            return false;
        }

        encryption_key = derived_key;
        return true;
    }

    bool send_response(uint32_t socket,
                       const std::vector<uint8_t> &encryption_key,
                       const fenris::Response &response)
    {
        // Serialize the response
        std::vector<uint8_t> serialized_response = serialize_response(response);

        // Generate random IV
        auto [iv, iv_gen_result] = m_crypto_manager.generate_random_iv();
        if (iv_gen_result != crypto::EncryptionResult::SUCCESS) {
            return false;
        }

        // Encrypt the serialized response using client's key and generated IV
        auto [encrypted_response, encrypt_result] =
            m_crypto_manager.encrypt_data(serialized_response,
                                          encryption_key,
                                          iv);
        if (encrypt_result != crypto::EncryptionResult::SUCCESS) {
            return false;
        }

        // Create the final message with IV prefixed to encrypted data
        std::vector<uint8_t> message_with_iv;
        message_with_iv.reserve(iv.size() + encrypted_response.size());
        message_with_iv.insert(message_with_iv.end(), iv.begin(), iv.end());
        message_with_iv.insert(message_with_iv.end(),
                               encrypted_response.begin(),
                               encrypted_response.end());

        // Send the IV-prefixed encrypted response
        NetworkResult send_result = send_prefixed_data(socket, message_with_iv);
        if (send_result != NetworkResult::SUCCESS) {
            return false;
        }

        return true;
    }

    std::optional<fenris::Request>
    receive_request(uint32_t socket, const std::vector<uint8_t> &encryption_key)
    {

        // Receive encrypted data (includes IV + encrypted request)
        std::vector<uint8_t> encrypted_data;
        NetworkResult recv_result =
            receive_prefixed_data(socket, encrypted_data);

        if (recv_result != NetworkResult::SUCCESS) {
            return std::nullopt;
        }

        if (encrypted_data.size() < crypto::AES_GCM_IV_SIZE) {
            return std::nullopt;
        }

        // Extract IV from the beginning of the message
        std::vector<uint8_t> iv(encrypted_data.begin(),
                                encrypted_data.begin() +
                                    crypto::AES_GCM_IV_SIZE);

        // Extract the encrypted request data (after the IV)
        std::vector<uint8_t> encrypted_request(encrypted_data.begin() +
                                                   crypto::AES_GCM_IV_SIZE,
                                               encrypted_data.end());

        // Decrypt the request using client's key and extracted IV
        auto [decrypted_data, decrypt_result] =
            m_crypto_manager.decrypt_data(encrypted_request,
                                          encryption_key,
                                          iv);
        if (decrypt_result != crypto::EncryptionResult::SUCCESS) {
            return std::nullopt;
        }

        return deserialize_request(decrypted_data);
    }

    // Create a default response based on the request type
    fenris::Response create_default_response(const fenris::Request &request)
    {
        fenris::Response response;
        response.set_success(true);

        switch (request.command()) {
        case fenris::RequestType::PING:
            response.set_type(fenris::ResponseType::PONG);
            response.set_data("PONG");
            break;

        case fenris::RequestType::LIST_DIR:
            handle_list_dir_request(request, response);
            break;

        case fenris::RequestType::CHANGE_DIR:
            handle_change_dir_request(request, response);
            break;

        case fenris::RequestType::READ_FILE:
            handle_read_file_request(request, response);
            break;

        case fenris::RequestType::CREATE_FILE:
            handle_create_file_request(request, response);
            break;

        case fenris::RequestType::WRITE_FILE:
            handle_write_file_request(request, response);
            break;

        case fenris::RequestType::APPEND_FILE:
            handle_append_file_request(request, response);
            break;

        case fenris::RequestType::DELETE_FILE:
            handle_delete_file_request(request, response);
            break;

        case fenris::RequestType::CREATE_DIR:
            handle_create_dir_request(request, response);
            break;

        case fenris::RequestType::DELETE_DIR:
            handle_delete_dir_request(request, response);
            break;

        case fenris::RequestType::INFO_FILE:
            handle_info_file_request(request, response);
            break;

        case fenris::RequestType::TERMINATE:
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_data("Server terminating connection");
            break;

        default:
            response.set_success(false);
            response.set_type(fenris::ResponseType::ERROR);
            response.set_data("Unknown command");
            break;
        }

        return response;
    }

    // Handler methods for each command type
    void handle_list_dir_request(const fenris::Request &request,
                                 fenris::Response &response)
    {
        response.set_type(fenris::ResponseType::DIR_LISTING);
        std::string listing = "Directory contents:\n";

        std::lock_guard<std::mutex> lock(m_fs_mutex);
        for (const auto &file : m_files) {
            if (file.first.find(m_current_dir) == 0) {
                listing +=
                    "F: " + file.first.substr(m_current_dir.length()) + "\n";
            }
        }
        for (const auto &dir : m_directories) {
            if (dir.find(m_current_dir) == 0 && dir != m_current_dir) {
                listing += "D: " + dir.substr(m_current_dir.length()) + "\n";
            }
        }
        response.set_data(listing);
    }

    void handle_change_dir_request(const fenris::Request &request,
                                   fenris::Response &response)
    {
        std::string target_dir = request.filename();
        std::lock_guard<std::mutex> lock(m_fs_mutex);

        // Simple directory navigation logic
        if (target_dir == "..") {
            size_t last_slash =
                m_current_dir.rfind('/', m_current_dir.length() - 2);
            if (last_slash != std::string::npos) {
                m_current_dir = m_current_dir.substr(0, last_slash + 1);
            }
        } else if (target_dir.front() == '/') {
            // Check if target directory exists
            std::string dir_path = target_dir;
            if (dir_path.back() != '/') {
                dir_path += '/';
            }

            if (m_directories.find(dir_path) != m_directories.end() ||
                dir_path == "/") {
                m_current_dir = dir_path;
            } else {
                response.set_success(false);
                response.set_type(fenris::ResponseType::ERROR);
                response.set_data("Directory not found: " + target_dir);
                return;
            }
        } else {
            // Relative path navigation
            std::string dir_path = m_current_dir + target_dir;
            if (dir_path.back() != '/') {
                dir_path += '/';
            }

            if (m_directories.find(dir_path) != m_directories.end()) {
                m_current_dir = dir_path;
            } else {
                response.set_success(false);
                response.set_type(fenris::ResponseType::ERROR);
                response.set_data("Directory not found: " + target_dir);
                return;
            }
        }

        response.set_type(fenris::ResponseType::SUCCESS);
        response.set_data("Changed directory to: " + m_current_dir);
    }

    void handle_read_file_request(const fenris::Request &request,
                                  fenris::Response &response)
    {
        std::string file_path = request.filename();
        std::lock_guard<std::mutex> lock(m_fs_mutex);

        if (m_files.find(file_path) != m_files.end()) {
            response.set_type(fenris::ResponseType::FILE_CONTENT);
            response.set_data(m_files[file_path]);
        } else {
            response.set_success(false);
            response.set_type(fenris::ResponseType::ERROR);
            response.set_data("File not found: " + file_path);
        }
    }

    void handle_create_file_request(const fenris::Request &request,
                                    fenris::Response &response)
    {
        std::string file_path = request.filename();
        std::string content = request.data();

        std::lock_guard<std::mutex> lock(m_fs_mutex);
        m_files[file_path] = content;

        response.set_type(fenris::ResponseType::SUCCESS);
        response.set_data("File created: " + file_path);
    }

    void handle_write_file_request(const fenris::Request &request,
                                   fenris::Response &response)
    {
        std::string file_path = request.filename();
        std::string content = request.data();

        std::lock_guard<std::mutex> lock(m_fs_mutex);
        m_files[file_path] = content;

        response.set_type(fenris::ResponseType::SUCCESS);
        response.set_data("File written: " + file_path);
    }

    void handle_append_file_request(const fenris::Request &request,
                                    fenris::Response &response)
    {
        std::string file_path = request.filename();
        std::string content = request.data();

        std::lock_guard<std::mutex> lock(m_fs_mutex);
        if (m_files.find(file_path) != m_files.end()) {
            m_files[file_path] += content;
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_data("Content appended to file: " + file_path);
        } else {
            response.set_success(false);
            response.set_type(fenris::ResponseType::ERROR);
            response.set_data("File not found: " + file_path);
        }
    }

    void handle_delete_file_request(const fenris::Request &request,
                                    fenris::Response &response)
    {
        std::string file_path = request.filename();

        std::lock_guard<std::mutex> lock(m_fs_mutex);
        if (m_files.find(file_path) != m_files.end()) {
            m_files.erase(file_path);
            response.set_type(fenris::ResponseType::SUCCESS);
            response.set_data("File deleted: " + file_path);
        } else {
            response.set_success(false);
            response.set_type(fenris::ResponseType::ERROR);
            response.set_data("File not found: " + file_path);
        }
    }

    void handle_create_dir_request(const fenris::Request &request,
                                   fenris::Response &response)
    {
        std::string dir_path = request.filename();
        if (dir_path.back() != '/') {
            dir_path += '/';
        }

        std::lock_guard<std::mutex> lock(m_fs_mutex);
        m_directories.insert(dir_path);

        response.set_type(fenris::ResponseType::SUCCESS);
        response.set_data("Directory created: " + dir_path);
    }

    void handle_delete_dir_request(const fenris::Request &request,
                                   fenris::Response &response)
    {
        std::string dir_path = request.filename();
        if (dir_path.back() != '/') {
            dir_path += '/';
        }

        std::lock_guard<std::mutex> lock(m_fs_mutex);
        if (m_directories.find(dir_path) != m_directories.end()) {
            // Check if the directory is empty (no files or subdirectories)
            bool is_empty = true;
            for (const auto &file : m_files) {
                if (file.first.find(dir_path) == 0) {
                    is_empty = false;
                    break;
                }
            }

            for (const auto &dir : m_directories) {
                if (dir != dir_path && dir.find(dir_path) == 0) {
                    is_empty = false;
                    break;
                }
            }

            if (is_empty) {
                m_directories.erase(dir_path);
                response.set_type(fenris::ResponseType::SUCCESS);
                response.set_data("Directory deleted: " + dir_path);
            } else {
                response.set_success(false);
                response.set_type(fenris::ResponseType::ERROR);
                response.set_data("Directory not empty: " + dir_path);
            }
        } else {
            response.set_success(false);
            response.set_type(fenris::ResponseType::ERROR);
            response.set_data("Directory not found: " + dir_path);
        }
    }

    void handle_info_file_request(const fenris::Request &request,
                                  fenris::Response &response)
    {
        std::string file_path = request.filename();

        std::lock_guard<std::mutex> lock(m_fs_mutex);

        if (m_files.find(file_path) != m_files.end()) {
            response.set_success(true);
            std::string info = "File: " + file_path + "\n";
            info += "Size: " + std::to_string(m_files[file_path].size()) +
                    " bytes\n";
            info += "Permissions: rw-r--r--\n"; // Mock permissions
            info += "Modified: " + get_current_time_string() +
                    "\n"; // Add mock timestamp

            response.set_type(fenris::ResponseType::FILE_INFO);
            auto file_info = response.mutable_file_info();
            file_info->set_name(file_path);
            file_info->set_size(m_files[file_path].size());
            file_info->set_modified_time(163);
            file_info->set_is_directory(false);
        } else {
            response.set_success(false);
            response.set_type(fenris::ResponseType::ERROR);
            response.set_data("File not found: " + file_path);
        }
    }

    // Helper method to get current time as a string for file info
    std::string get_current_time_string()
    {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::string time_str = std::ctime(&time);

        // Remove trailing newline that ctime adds
        if (!time_str.empty() && time_str.back() == '\n') {
            time_str.pop_back();
        }

        return time_str;
    }

    int m_port;
    int m_listen_socket;
    int m_client_socket;
    std::atomic<bool> m_running;
    std::thread m_server_thread;
    std::mutex m_client_mutex;

    std::vector<fenris::Request> m_received_requests;
    std::mutex m_requests_mutex;

    std::mutex m_response_mutex;

    // Store encryption key derived from key exchange
    std::vector<uint8_t> m_encryption_key;
    crypto::CryptoManager m_crypto_manager;

    // Mock filesystem
    std::unordered_map<std::string, std::string> m_files;
    std::unordered_set<std::string> m_directories;
    std::mutex m_fs_mutex;
    std::string m_current_dir;
};

// Client test fixture
class ClientIntegrationTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        // Disable colors for testing to ensure assertions work with plain text
        colors::disable_colors();

        // Start mock server
        m_mock_server = std::make_unique<MockServer>();
        ASSERT_TRUE(m_mock_server->start());
        m_port = m_mock_server->get_port();
        m_port_str = std::to_string(m_port);

        // Create the client with our test port
        m_client = std::make_unique<Client>("TestClient");

        // Set up the mock TUI and keep a raw pointer to it
        auto mock_tui = std::make_unique<MockTUI>();
        m_mock_tui = mock_tui.get();

        // Set the port number in the mock TUI to match the server's port
        m_mock_tui->set_port_number(m_port_str);

        // Create a custom connection manager with the test port
        auto connection_manager =
            std::make_unique<ConnectionManager>("127.0.0.1",
                                                m_port_str,
                                                "TestConnectionManager");

        // Force blocking mode for test reliability
        connection_manager->set_non_blocking_mode(false);

        // Set the custom components on the client
        m_client->set_tui(std::move(mock_tui));
        m_client->set_connection_manager(std::move(connection_manager));

        // Initialize mock filesystem with some test data
        m_mock_server->add_directory("/");
        m_mock_server->add_directory("/dir1/");
        m_mock_server->add_directory("/dir2/");
        m_mock_server->add_file("/file1.txt", "This is file 1 content");
        m_mock_server->add_file("/file2.txt", "This is file 2 content");
        m_mock_server->add_file("/dir1/nested.txt", "Nested file content");

        // Queue the connect command to connect before running tests
        // m_mock_tui->queue_command({"connect"});
    }

    void TearDown() override
    {
        if (m_client_thread.joinable()) {
            m_client_thread.join();
        }

        if (m_mock_server) {
            m_mock_server->stop();
        }

        // Sleep to allow resources to clean up
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Re-enable colors for normal usage
        colors::enable_colors();
    }

    // Start the client in a separate thread and wait for it to process all
    // commands
    void runClient()
    {
        // Start the client in a separate thread
        m_client_thread = std::thread([this]() { m_client->run(); });

        // Give client time to connect and stabilize the connection
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Wait for client to process all commands and exit
        while (!m_client->is_exit_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        std::cout << "Client has exited" << std::endl;

        if (m_client_thread.joinable()) {
            m_client_thread.join();
        }
    }

    std::unique_ptr<Client> m_client;
    std::unique_ptr<MockServer> m_mock_server;
    MockTUI *m_mock_tui; // Raw pointer to TUI owned by the client
    int m_port;
    std::string m_port_str;
    std::thread m_client_thread;
};

// Basic connection test
TEST_F(ClientIntegrationTest, ConnectAndDisconnect)
{
    // Just queue exit command - we're already connected in SetUp
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    m_mock_tui->queue_command({"exit"});
    runClient();

    auto results = m_mock_tui->get_displayed_results();

    // Expect at least two results: connection success and disconnect
    ASSERT_GE(results.size(), 2);
    // Look for the connection success message (might not be the first result
    // now)
    bool found_connect = false;
    for (const auto &result : results) {
        // Check for a more general connection success message
        std::cout << "Result: " << result.second << std::endl;
        if (result.first && (result.second.find("Connected to server at") !=
                             std::string::npos)) {
            found_connect = true;
            break;
        }
    }
    EXPECT_TRUE(found_connect)
        << "No connection success message found in results";

    // Last result should be disconnect
    size_t last = results.size() - 1;
    EXPECT_TRUE(results[last].first);
    EXPECT_EQ(results[last].second, "Disconnected from server");
}

// Test ping command
TEST_F(ClientIntegrationTest, PingCommand)
{
    m_mock_tui->queue_command({"ping"});
    m_mock_tui->queue_command({"exit"});
    runClient();

    // Wait for server to process request
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto results = m_mock_tui->get_displayed_results();
    auto requests = m_mock_server->get_received_requests();

    // Verify at least one request was received
    ASSERT_FALSE(requests.empty()) << "No requests received by server";

    // Verify ping request was sent
    bool found_ping = false;
    for (const auto &request : requests) {
        if (request.command() == fenris::RequestType::PING) {
            found_ping = true;
            break;
        }
    }
    EXPECT_TRUE(found_ping) << "No PING request found among received requests";

    // Verify PONG response was displayed
    bool found_pong = false;
    for (const auto &result : results) {
        if (result.first && result.second == "Server is alive") {
            found_pong = true;
            break;
        }
    }
    EXPECT_TRUE(found_pong) << "No PONG response was displayed";
}

// Test ls (list directory) command
TEST_F(ClientIntegrationTest, ListDirectoryCommand)
{
    m_mock_tui->queue_command({"ls"});
    runClient();

    auto requests = m_mock_server->get_received_requests();

    // Verify request was sent
    ASSERT_GE(requests.size(), 1);
    EXPECT_EQ(requests[0].command(), fenris::RequestType::LIST_DIR);

    // Verify dir contents were displayed
    auto results = m_mock_tui->get_displayed_results();
    bool found_listing = false;
    for (const auto &result : results) {
        if (result.first &&
            result.second.find("Directory contents:") != std::string::npos) {
            found_listing = true;
            // Should contain our test files and directories
            EXPECT_NE(result.second.find("file1.txt"), std::string::npos);
            EXPECT_NE(result.second.find("file2.txt"), std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(found_listing);
}

// Test cd (change directory) command
TEST_F(ClientIntegrationTest, ChangeDirectoryCommand)
{
    m_mock_tui->queue_command({"cd", "dir1"});
    m_mock_tui->queue_command({"ls"});
    runClient();

    auto requests = m_mock_server->get_received_requests();

    // Should have at least 2 requests: cd and ls
    ASSERT_GE(requests.size(), 2);
    EXPECT_EQ(requests[0].command(), fenris::RequestType::CHANGE_DIR);
    EXPECT_EQ(requests[0].filename(), "dir1");
    EXPECT_EQ(requests[1].command(), fenris::RequestType::LIST_DIR);

    // Verify directory was changed and nested.txt is visible
    auto results = m_mock_tui->get_displayed_results();
    bool dir_changed = false;
    bool found_nested = false;

    for (const auto &result : results) {
        if (result.first &&
            result.second.find("Changed directory") != std::string::npos) {
            dir_changed = true;
        }
        if (result.first &&
            result.second.find("nested.txt") != std::string::npos) {
            found_nested = true;
        }
    }

    EXPECT_TRUE(dir_changed);
    EXPECT_TRUE(found_nested);
}

// Test cat (read file) command
TEST_F(ClientIntegrationTest, ReadFileCommand)
{
    m_mock_tui->queue_command({"cat", "/file1.txt"});
    runClient();

    auto requests = m_mock_server->get_received_requests();

    // Verify request was sent
    ASSERT_GE(requests.size(), 1);
    EXPECT_EQ(requests[0].command(), fenris::RequestType::READ_FILE);
    EXPECT_EQ(requests[0].filename(), "/file1.txt");

    // Verify file content was displayed
    auto results = m_mock_tui->get_displayed_results();
    bool found_content = false;

    for (const auto &result : results) {
        if (result.first &&
            result.second.find("This is file 1 content") != std::string::npos) {
            found_content = true;
            break;
        }
    }

    EXPECT_TRUE(found_content);
}

// Test write command
TEST_F(ClientIntegrationTest, WriteFileCommand)
{
    m_mock_tui->queue_command({"write", "/newfile.txt", "New file content"});
    m_mock_tui->queue_command({"cat", "/newfile.txt"});
    runClient();

    auto requests = m_mock_server->get_received_requests();

    // Should have 2 requests: write and cat
    ASSERT_GE(requests.size(), 2);
    EXPECT_EQ(requests[0].command(), fenris::RequestType::WRITE_FILE);
    EXPECT_EQ(requests[0].filename(), "/newfile.txt");
    EXPECT_EQ(requests[0].data(), "New file content");
    EXPECT_EQ(requests[1].command(), fenris::RequestType::READ_FILE);

    // Verify file was written and then read
    auto results = m_mock_tui->get_displayed_results();
    bool write_success = false;
    bool read_success = false;

    for (const auto &result : results) {
        if (result.first &&
            result.second.find("File written") != std::string::npos) {
            write_success = true;
        }
        if (result.first &&
            result.second.find("New file content") != std::string::npos) {
            read_success = true;
        }
    }

    EXPECT_TRUE(write_success);
    EXPECT_TRUE(read_success);
}

// Test append command
TEST_F(ClientIntegrationTest, AppendFileCommand)
{
    m_mock_tui->queue_command({"append", "/file1.txt", " - APPENDED TEXT"});
    m_mock_tui->queue_command({"cat", "/file1.txt"});
    runClient();

    auto requests = m_mock_server->get_received_requests();

    // Should have 2 requests: append and cat
    ASSERT_GE(requests.size(), 2);
    EXPECT_EQ(requests[0].command(), fenris::RequestType::APPEND_FILE);
    EXPECT_EQ(requests[0].filename(), "/file1.txt");
    EXPECT_EQ(requests[0].data(), " - APPENDED TEXT");
    EXPECT_EQ(requests[1].command(), fenris::RequestType::READ_FILE);

    // Verify content was appended and then read
    auto results = m_mock_tui->get_displayed_results();
    bool append_success = false;
    bool read_success = false;

    for (const auto &result : results) {
        if (result.first &&
            result.second.find("Content appended") != std::string::npos) {
            append_success = true;
        }
        if (result.first &&
            result.second.find("This is file 1 content - APPENDED TEXT") !=
                std::string::npos) {
            read_success = true;
        }
    }

    EXPECT_TRUE(append_success);
    EXPECT_TRUE(read_success);
}

// Test rm (remove file) command
TEST_F(ClientIntegrationTest, RemoveFileCommand)
{
    m_mock_tui->queue_command({"rm", "/file2.txt"});
    m_mock_tui->queue_command({"cat", "/file2.txt"}); // Should fail now
    runClient();

    auto requests = m_mock_server->get_received_requests();

    // Should have 2 requests: rm and cat
    ASSERT_GE(requests.size(), 2);
    EXPECT_EQ(requests[0].command(), fenris::RequestType::DELETE_FILE);
    EXPECT_EQ(requests[0].filename(), "/file2.txt");
    EXPECT_EQ(requests[1].command(), fenris::RequestType::READ_FILE);

    // Verify file was deleted and cat command fails
    auto results = m_mock_tui->get_displayed_results();
    bool delete_success = false;
    bool read_failure = false;

    for (const auto &result : results) {
        if (result.first &&
            result.second.find("File deleted") != std::string::npos) {
            delete_success = true;
        }
        if (!result.first &&
            result.second.find("File not found") != std::string::npos) {
            read_failure = true;
        }
    }

    EXPECT_TRUE(delete_success);
    EXPECT_TRUE(read_failure);
}

// Test mkdir (make directory) command
TEST_F(ClientIntegrationTest, MakeDirectoryCommand)
{
    m_mock_tui->queue_command({"mkdir", "/newdir"});
    m_mock_tui->queue_command({"cd", "/newdir"});
    m_mock_tui->queue_command({"ls"});
    runClient();

    auto requests = m_mock_server->get_received_requests();

    // Should have 3 requests: mkdir, cd, ls
    ASSERT_GE(requests.size(), 3);
    EXPECT_EQ(requests[0].command(), fenris::RequestType::CREATE_DIR);
    EXPECT_EQ(requests[0].filename(), "/newdir");
    EXPECT_EQ(requests[1].command(), fenris::RequestType::CHANGE_DIR);
    EXPECT_EQ(requests[2].command(), fenris::RequestType::LIST_DIR);

    // Verify directory was created and changed into
    auto results = m_mock_tui->get_displayed_results();
    bool mkdir_success = false;
    bool cd_success = false;

    for (const auto &result : results) {
        if (result.first &&
            result.second.find("Directory created") != std::string::npos) {
            mkdir_success = true;
        }
        if (result.first &&
            result.second.find("Changed directory") != std::string::npos &&
            result.second.find("newdir") != std::string::npos) {
            cd_success = true;
        }
    }

    EXPECT_TRUE(mkdir_success);
    EXPECT_TRUE(cd_success);
}

// Test rmdir (remove directory) command
TEST_F(ClientIntegrationTest, RemoveDirectoryCommand)
{
    m_mock_tui->queue_command({"rmdir", "/dir2"});
    m_mock_tui->queue_command({"cd", "/dir2"}); // Should fail
    runClient();

    auto requests = m_mock_server->get_received_requests();

    // Should have 2 requests: rmdir, cd
    ASSERT_GE(requests.size(), 2);
    EXPECT_EQ(requests[0].command(), fenris::RequestType::DELETE_DIR);
    EXPECT_EQ(requests[0].filename(), "/dir2");
    EXPECT_EQ(requests[1].command(), fenris::RequestType::CHANGE_DIR);

    // Verify directory was removed and cd fails
    auto results = m_mock_tui->get_displayed_results();
    bool rmdir_success = false;
    bool cd_failure = false;

    for (const auto &result : results) {
        if (result.first &&
            result.second.find("Directory deleted") != std::string::npos) {
            rmdir_success = true;
        }
        if (!result.first &&
            result.second.find("Directory not found") != std::string::npos) {
            cd_failure = true;
        }
    }

    EXPECT_TRUE(rmdir_success);
    EXPECT_TRUE(cd_failure);
}

// Test file info command
TEST_F(ClientIntegrationTest, FileInfoCommand)
{
    m_mock_tui->queue_command({"info", "/file1.txt"});
    runClient();

    auto requests = m_mock_server->get_received_requests();

    // Verify request was sent
    ASSERT_GE(requests.size(), 1);
    EXPECT_EQ(requests[0].command(), fenris::RequestType::INFO_FILE);
    EXPECT_EQ(requests[0].filename(), "/file1.txt");

    // Verify file info was displayed
    auto results = m_mock_tui->get_displayed_results();
    bool found_info = false;
    for (const auto &result : results) {
        if (result.first &&
            result.second.find("File: /file1.txt") != std::string::npos) {
            found_info = true;
            break;
        }
    }

    EXPECT_TRUE(found_info);
}

// Test invalid command
TEST_F(ClientIntegrationTest, InvalidCommand)
{
    m_mock_tui->queue_command({"invalid_command"});
    runClient();

    // Verify error was displayed
    auto results = m_mock_tui->get_displayed_results();
    bool found_error = false;
    for (const auto &result : results) {
        if (!result.first &&
            result.second.find("Invalid command") != std::string::npos) {
            found_error = true;
            break;
        }
    }

    EXPECT_TRUE(found_error);
}

// Test help command
TEST_F(ClientIntegrationTest, HelpCommand)
{
    m_mock_tui->queue_command({"help"});
    runClient();

    // Verify help was displayed
    auto results = m_mock_tui->get_displayed_results();
    bool found_help = false;

    for (const auto &result : results) {
        if (result.first &&
            result.second.find("Available commands") != std::string::npos) {
            found_help = true;
            break;
        }
    }

    EXPECT_TRUE(found_help);
}

// Test multiple commands in sequence
TEST_F(ClientIntegrationTest, MultipleCommandSequence)
{
    // Create a directory, create a file in it, read it, then delete both
    m_mock_tui->queue_command({"mkdir", "/test_seq"});
    m_mock_tui->queue_command({"cd", "/test_seq"});
    m_mock_tui->queue_command(
        {"write", "seq_file.txt", "Sequential test content"});
    m_mock_tui->queue_command({"cat", "seq_file.txt"});
    m_mock_tui->queue_command({"rm", "seq_file.txt"});
    m_mock_tui->queue_command({"cd", ".."});
    m_mock_tui->queue_command({"rmdir", "/test_seq"});

    runClient();

    auto requests = m_mock_server->get_received_requests();

    // Should have all our commands
    ASSERT_GE(requests.size(), 7);
    EXPECT_EQ(requests[0].command(), fenris::RequestType::CREATE_DIR);
    EXPECT_EQ(requests[1].command(), fenris::RequestType::CHANGE_DIR);
    EXPECT_EQ(requests[2].command(), fenris::RequestType::WRITE_FILE);
    EXPECT_EQ(requests[3].command(), fenris::RequestType::READ_FILE);
    EXPECT_EQ(requests[4].command(), fenris::RequestType::DELETE_FILE);
    EXPECT_EQ(requests[5].command(), fenris::RequestType::CHANGE_DIR);
    EXPECT_EQ(requests[6].command(), fenris::RequestType::DELETE_DIR);

    // Verify the file content was read correctly
    auto results = m_mock_tui->get_displayed_results();
    bool found_content = false;

    for (const auto &result : results) {
        if (result.first && result.second.find("Sequential test content") !=
                                std::string::npos) {
            found_content = true;
            break;
        }
    }

    EXPECT_TRUE(found_content);
}

// Edge case: test with empty responses
TEST_F(ClientIntegrationTest, EmptyResponsesFromServer)
{
    // Set up empty responses for multiple commands
    m_mock_tui->queue_command({"ls"});
    m_mock_tui->queue_command({"cat", "/file1.txt"});
    m_mock_tui->queue_command({"info", "/file1.txt"});

    runClient();

    // Verify client handled empty responses gracefully
    auto requests = m_mock_server->get_received_requests();
    ASSERT_GE(requests.size(), 3);
}

// Edge case: test with non-existent files/directories
TEST_F(ClientIntegrationTest, NonExistentResources)
{
    m_mock_tui->queue_command({"cat", "/nonexistent.txt"});
    m_mock_tui->queue_command({"cd", "/nonexistent/"});
    m_mock_tui->queue_command({"rm", "/nonexistent.txt"});
    m_mock_tui->queue_command({"rmdir", "/nonexistent/"});

    runClient();

    // Verify appropriate error messages were displayed
    auto results = m_mock_tui->get_displayed_results();
    int error_count = 0;

    for (const auto &result : results) {
        if (!result.first &&
            result.second.find("not found") != std::string::npos) {
            error_count++;
        }
    }

    EXPECT_GE(error_count, 4);
}

} // namespace tests
} // namespace client
} // namespace fenris
