#ifndef FENRIS_SERVER_STRUCT_HPP
#define FENRIS_SERVER_STRUCT_HPP

#include <atomic> // for std::atomic
#include <cstdint>
#include <memory> // for std::shared_ptr
#include <mutex>
#include <string>
#include <vector>

namespace fenris {
namespace server {

const std::string DEFAULT_SERVER_DIR = "/fenris_server";

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
          current_node(FST.root)
    {
        current_node->access_count++;
    }

    ~ClientInfo()
    {
        // Ensure that current_node is root before deleting the client info
        if (current_node) {
            current_node->access_count--;
        }
    }
};

struct Node {
    std::string name;  // Name of the file or directory
    bool is_directory; // Flag to indicate if it's a directory
    std::vector<std::shared_ptr<Node>> children; // Child nodes
    std::weak_ptr<Node> parent; // Weak pointer to the parent node
    std::atomic<int> access_count{0};
    // Atomic counter which works as a sub directory access counter if
    // (is_directory) or as a file readers counter if (!is_directory)
    std::mutex node_mutex; // Mutex for write (if !is_directory) or read/write
                           // (if is_directory)
};

class FileSystemTree {
  public:
    FileSystemTree();

    // Adds a new node to the tree
    bool addNode(const std::string &path, bool is_directory);

    // Removes a node from the tree
    bool removeNode(const std::string &path);

    // Finds a node by its path
    std::shared_ptr<Node> findNode(const std::string &path);

  private:
    std::shared_ptr<Node> root; // Root of the file system tree

    // Helper function to traverse the tree
    std::shared_ptr<Node> traverse(const std::string &path);
};

} // namespace server
} // namespace fenris

#endif // FENRIS_SERVER_HPP
