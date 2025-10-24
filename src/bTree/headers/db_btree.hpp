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

    // Create a new, empty B-Tree file
    bool create(const std::string& filename, KeyType key_type);

    // Open an existing B-Tree file
    bool open(const std::string& filename);

    // Close the file
    void close();

    // --- Public API ---

    // Insert a key-value pair.
    // Handles int and string keys automatically.
    void insert(int32_t key, int32_t value);
    void insert(const std::string& key, int32_t value);

    // Point Search. Find value for a key.
    // Returns std::nullopt if not found.
    std::optional<int32_t> search(int32_t key);
    std::optional<int32_t> search(const std::string& key);

    // Update value for an existing key.
    // Returns true on success, false if key not found.
    bool update(int32_t key, int32_t new_value);
    bool update(const std::string& key, int32_t new_value);

    // Delete a key.
    void remove(int32_t key);
    void remove(const std::string& key);

    // Range Search. Returns all key-value pairs in [min_k, max_k].
    // Note: This is less efficient than in-memory as it reads
    // many blocks, but demonstrates the traversal.
    using KeyValuePair = std::pair<std::string, int32_t>;
    std::vector<KeyValuePair> rangeSearch(int32_t min_k, int32_t max_k);
    std::vector<KeyValuePair> rangeSearch(const std::string& min_k, const std::string& max_k);

    // --- Utilities ---
    // Traverses tree and prints to stdout (for debugging)
    void print();

private:
    // --- Node I/O ---

    // Read a node from disk and deserialize it
    std::shared_ptr<Node> readNode(offset_t offset);

    // Serialize a node and write it to a *new* block
    // This is the core of Copy-on-Write (shadow paging)
    offset_t writeNode(std::shared_ptr<Node> node);

    // --- Internal Helpers ---

    // Convert keys to/from the internal string format
    std::string keyToString(int32_t key);

    // Recursive search helper
    std::optional<int32_t> searchRecursive(offset_t node_offset, const std::string& k);

    // Recursive update helper (uses Copy-on-Write)
    // Returns the offset of the *newly written* (or unchanged) node.
    offset_t updateRecursive(offset_t node_offset, const std::string& k, int32_t new_value, bool& updated);

    // Recursive insert helper
    offset_t insertRecursive(offset_t node_offset, const std::string& k, int32_t v);

    // Recursive remove helper
    offset_t removeRecursive(offset_t node_offset, const std::string& k);

    // --- B-Tree Logic (Copy-on-Write) ---
    // These functions modify a node *in memory* and return its
    // *new* disk offset after writing.

    // Recursive helper for `insertRecursive`
    offset_t insertNonFull(std::shared_ptr<Node> node, offset_t node_offset, const std::string& k, int32_t v);

    // Splits child `i` of `node`.
    // Writes all 3 modified nodes (parent, child, new sibling) to disk.
    // Returns the offset of the *new* parent node.
    offset_t splitChild(std::shared_ptr<Node> parent_node, offset_t parent_offset, int i);

    // Recursive remove logic
    offset_t removeFromLeaf(std::shared_ptr<Node> node, int idx);
    offset_t removeFromNonLeaf(std::shared_ptr<Node> node, offset_t node_offset, int idx);
    std::shared_ptr<Node> getPred(offset_t node_offset, int idx);
    std::shared_ptr<Node> getSucc(offset_t node_offset, int idx);

    // Fill/Merge logic for remove
    offset_t fill(std::shared_ptr<Node> node, offset_t node_offset, int idx);
    offset_t borrowFromPrev(std::shared_ptr<Node> node, offset_t node_offset, int idx);
    offset_t borrowFromNext(std::shared_ptr<Node> node, offset_t node_offset, int idx);
    offset_t merge(std::shared_ptr<Node> node, offset_t node_offset, int idx);

    // Recursive range search helper
    void rangeSearchRecursive(offset_t node_offset, const std::string& min_k, const std::string& max_k, std::vector<KeyValuePair>& results);

    // Recursive print helper
    void printRecursive(offset_t node_offset, int level);

    DbFile file;
    MetadataHeader metadata;

    // When an operation modifies the tree, it returns the
    // offset of the new root. This variable tracks it.
    offset_t new_root_offset_cache;
};
