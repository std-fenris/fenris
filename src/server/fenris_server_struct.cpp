#include "server/fenris_server_struct.hpp"
#include <algorithm> // for std::find_if
#include <sstream>   // for std::istringstream
#include <stdexcept> // for std::runtime_error

namespace fenris {
namespace server {

FileSystemTree::FileSystemTree()
{
    root = std::make_shared<Node>();
    root->name = "/";
    root->is_directory = true;
    root->access_count = 0;
    root->parent.reset(); // Use reset() to clear the weak_ptr
}

bool FileSystemTree::addNode(const std::string &path, bool is_directory)
{
    auto parent = traverse(path.substr(0, path.find_last_of('/')));
    if (!parent || !parent->is_directory) {
        return false;
    }

    auto new_node = std::make_shared<Node>();
    new_node->name = path.substr(path.find_last_of('/') + 1);
    new_node->is_directory = is_directory;
    new_node->access_count = 0;
    new_node->parent = parent;

    parent->children.push_back(new_node);
    return true;
}

bool FileSystemTree::removeNode(const std::string &path)
{
    auto node = traverse(path);
    if (!node || node->access_count > 0) {
        return false; // Cannot remove a node being accessed
    }

    auto parent = node->parent.lock();
    if (!parent) {
        return false;
    }

    parent->children.erase(
        std::remove_if(parent->children.begin(),
                       parent->children.end(),
                       [&node](const std::shared_ptr<Node> &child) {
                           return child == node;
                       }),
        parent->children.end());
    return true;
}

std::shared_ptr<Node> FileSystemTree::findNode(const std::string &path)
{
    return traverse(path);
}

std::shared_ptr<Node> FileSystemTree::traverse(const std::string &path)
{
    if (path == "/") {
        return root;
    }

    std::istringstream stream(path);
    std::string segment;
    auto current = root;

    while (std::getline(stream, segment, '/')) {
        if (segment.empty()) {
            continue;
        }

        auto it = std::find_if(current->children.begin(),
                               current->children.end(),
                               [&segment](const std::shared_ptr<Node> &child) {
                                   return child->name == segment;
                               });

        if (it == current->children.end()) {
            return nullptr;
        }

        current = *it;
    }

    return current;
}

} // namespace server
} // namespace fenris
