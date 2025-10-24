#pragma once

#include "db_constants.hpp"
#include "db_file.hpp"
#include "db_node.hpp"
#include <string>
#include <memory>
#include <optional>

class BTree {
public:
    BTree();
    ~BTree();

    // --- File Lifecycle ---
    bool create(const std::string& filename, KeyType key_type);
    bool open(const std::string& filename);
    void close();

    // --- Public API ---
    void insert(int32_t key, int32_t value);
    void insert(const std::string& key, int32_t value);

    std::optional<int32_t> search(int32_t key);
    std::optional<int32_t> search(const std::string& key);

    bool update(int32_t key, int32_t new_value);
    bool update(const std::string& key, int32_t new_value);

    void remove(int32_t key);
    void remove(const std::string& key);

    using KeyValuePair = std::pair<std::string, int32_t>;
    std::vector<KeyValuePair> rangeSearch(int32_t min_k, int32_t max_k);
    std::vector<KeyValuePair> rangeSearch(const std::string& min_k, const std::string& max_k);

    void print();

private:
    // --- Node I/O ---
    std::shared_ptr<Node> readNode(offset_t offset);
    offset_t writeNode(std::shared_ptr<Node> node);

    // --- Internal Helpers ---
    std::string keyToString(int32_t key);
    std::string keyToString(const std::string& key);

    // --- Recursive B-Tree Logic ---
    std::optional<int32_t> searchRecursive(offset_t node_offset, const std::string& k);
    offset_t updateRecursive(offset_t node_offset, const std::string& k, int32_t new_value, bool& updated);
    offset_t insertRecursive(offset_t node_offset, const std::string& k, int32_t v);
    offset_t removeRecursive(offset_t node_offset, const std::string& k);

    offset_t insertNonFull(std::shared_ptr<Node> node, offset_t node_offset, const std::string& k, int32_t v);
    offset_t splitChild(std::shared_ptr<Node> parent_node, offset_t parent_offset, int i);

    offset_t removeFromLeaf(std::shared_ptr<Node> node, offset_t node_offset, int idx);
    offset_t removeFromNonLeaf(std::shared_ptr<Node> node, offset_t node_offset, int idx);
    std::shared_ptr<Node> getPred(offset_t node_offset, int idx);
    std::shared_ptr<Node> getSucc(offset_t node_offset, int idx);

    offset_t fill(std::shared_ptr<Node> node, offset_t node_offset, int idx);
    offset_t borrowFromPrev(std::shared_ptr<Node> node, offset_t node_offset, int idx);
    offset_t borrowFromNext(std::shared_ptr<Node> node, offset_t node_offset, int idx);
    offset_t merge(std::shared_ptr<Node> node, offset_t node_offset, int idx);

    void rangeSearchRecursive(offset_t node_offset, const std::string& min_k, const std::string& max_k, std::vector<KeyValuePair>& results);
    void printRecursive(offset_t node_offset, int level);

    DbFile file;
    MetadataHeader metadata; // This is the BTree's in-memory copy of the header
};
