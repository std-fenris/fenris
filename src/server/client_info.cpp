#include "server/client_info.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>

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

bool FileSystemTree::add_node(const std::string &path, bool is_directory)
{
    {

        std::lock_guard<std::mutex> lock(tree_mutex);
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
    }
    return true;
}

bool FileSystemTree::remove_node(const std::string &path)
{
    std::lock_guard<std::mutex> lock(tree_mutex);
    auto node = traverse(path);
    if (!node || node->access_count > 0) {
        return false; // Cannot remove a node being accessed
    }

    auto parent = node->parent.lock();
    if (parent) {
        auto it = std::remove_if(parent->children.begin(),
                                 parent->children.end(),
                                 [&node](const std::shared_ptr<Node> &child) {
                                     return child == node;
                                 });
        parent->children.erase(it, parent->children.end());
    }

    return true;
}

std::shared_ptr<Node> FileSystemTree::find_node(const std::string &path)
{
    std::lock_guard<std::mutex> lock(tree_mutex);
    return traverse(path);
}

std::shared_ptr<Node>
FileSystemTree::find_file(const std::shared_ptr<Node> &current_node,
                          const std::string &file)
{
    std::lock_guard<std::mutex> lock(tree_mutex);
    auto it = std::find_if(current_node->children.begin(),
                           current_node->children.end(),
                           [&file](const std::shared_ptr<Node> &child) {
                               return ((child->name == file) &&
                                       (child->is_directory == false));
                           });
    return (it != current_node->children.end()) ? *it : nullptr;
}

std::shared_ptr<Node>
FileSystemTree::find_directory(const std::shared_ptr<Node> &current_node,
                               const std::string &dir)
{
    std::lock_guard<std::mutex> lock(tree_mutex);
    auto it = std::find_if(current_node->children.begin(),
                           current_node->children.end(),
                           [&dir](const std::shared_ptr<Node> &child) {
                               return ((child->name == dir) &&
                                       (child->is_directory == true));
                           });
    return (it != current_node->children.end()) ? *it : nullptr;
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
