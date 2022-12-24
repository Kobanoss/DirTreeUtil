#include <vector>
#include <experimental/filesystem>
#include <cinttypes>


struct Node {
    std::experimental::filesystem::path path = "/";
    std::string name{};
    bool is_dir = false;
    bool is_other = false;
    uint64_t size = 0;
    uint64_t width = 1;
    uint64_t depth = 0;

    std::vector<Node *> children{};

    Node() = default;

    void addChild(Node *child) {
        children.push_back(child);
        width += 1;
    }
};