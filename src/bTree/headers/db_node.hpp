#pragma once

#include "db_constants.hpp"
#include <vector>
#include <string>
#include <memory>
#include <cstring>

// --- Node Serialized Header ---
// This header exists at the start of every node block.
struct NodeHeader {
    bool is_leaf;
    uint32_t n; // Current number of keys
    // 11 bytes padding for 16-byte alignment
    char padding[11];

    NodeHeader() : is_leaf(true), n(0), padding{0} {}
};

// --- Node Class ---
// In-memory representation of a B-Tree node.
// This class is generic and holds keys as std::string,
// but serializes them as either int or char[255] based on key_type.
class Node {
public:
    // --- Constructors ---

    // Create a new, empty node
    Node(uint32_t t, KeyType key_type, bool is_leaf);

    // Create a node by deserializing from a raw block
    Node(uint32_t t, KeyType key_type, const char* buffer);

    // --- Properties ---
    bool is_leaf;
    uint32_t n;  // current number of keys
    uint32_t t; // min degree
    KeyType key_type;

    // We use std::string for keys internally for simplicity,
    // even for integer keys (we'll just store them as strings).
    // This simplifies the generic logic.
    std::vector<std::string> keys;
    std::vector<int32_t> values;
    std::vector<offset_t> children; // File offsets of child nodes

    // --- Search ---

    // Find the first key >= k.
    // Returns index.
    int findKey(const std::string& k);

    // --- Serialization ---

    // Serialize this node's data into a 4096-byte buffer
    void serialize(char* buffer) const;

    // --- Key/Value Helpers ---

    // Splits this (full) node's child `i` into this node and `new_sibling`
    void splitChild(int i, std::shared_ptr<Node> child_to_split, std::shared_ptr<Node> new_sibling);

    // Get/Set for integer keys (converts to/from string)
    static std::string intKeyToString(int32_t key);
    static int32_t stringKeyToInt(const std::string& key);

private:
    uint32_t max_keys;
    uint32_t max_children;

    // Helpers for serialization
    void serializeIntKeys(char*& ptr) const;
    void serializeStringKeys(char*& ptr) const;

    // Helpers for deserialization
    void deserializeIntKeys(const char*& ptr);
    void deserializeStringKeys(const char*& ptr);
};
