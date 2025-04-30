#ifndef FENRIS_CLIENT_INFO_HPP
#define FENRIS_CLIENT_INFO_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace fenris {
namespace server {

const std::string DEFAULT_SERVER_DIR = "/fenris_server";

struct Node {
    std::string name;
    bool is_directory;
    std::vector<std::shared_ptr<Node>> children;
    std::weak_ptr<Node> parent;
    std::atomic<int> access_count{0};
    std::mutex node_mutex;
};

class FileSystemTree {
  public:
    FileSystemTree();

    // Adds a new node to the tree
    bool add_node(const std::string &path, bool is_directory);

    // Removes a node from the tree
    bool remove_node(const std::string &path);

    // Finds a node by its path
    std::shared_ptr<Node> find_node(const std::string &path);

    // Finds a file in the current node's children
    std::shared_ptr<Node> find_file(const std::shared_ptr<Node> &current_node,
                                    const std::string &file);

    std::shared_ptr<Node>
    find_directory(const std::shared_ptr<Node> &current_node,
                   const std::string &dir);

    std::shared_ptr<Node> root; // Root of the file system tree

  private:
    std::mutex tree_mutex; // Mutex for thread-safe access to the tree

    // Helper function to traverse the tree
    std::shared_ptr<Node> traverse(const std::string &path);
};

struct ClientInfo {
    uint32_t client_id;
    uint32_t socket;
    std::string address;
    std::string port;
    std::string current_directory = "/";
    uint32_t depth = 0; // Depth of the current directory in the tree
    bool keep_connection;

    std::vector<uint8_t> encryption_key;
    std::shared_ptr<Node>
        current_node; // Pointer to the current node in the file system tree

    ClientInfo(uint32_t client_id, uint32_t client_socket)
        : client_id(client_id), socket(client_socket), keep_connection(true),
          current_node(nullptr)
    {
    }

    // Function to set the current node
    void set_current_node(std::shared_ptr<Node> node)
    {
        if (current_node) {
            current_node->access_count++;
        }
    }

    ~ClientInfo()
    {
        // Ensure that current_node is root before deleting the client info
        if (current_node) {
            current_node->access_count--;
        }
    }
};

} // namespace server
} // namespace fenris

#endif // FENRIS_CLIENT_INFO_HPP
