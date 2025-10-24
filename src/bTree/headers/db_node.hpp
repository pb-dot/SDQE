#pragma once

#include "db_constants.hpp"
#include <vector>
#include <string>
#include <memory>
#include <cstring> // For memcpy

/*
Calculating t
A full node contains 2t - 1 keys and 2t child pointers.

Let's do the math.

Total Block Size: 4096 bytes.

Node Header: We need space for is_leaf (1 byte) and n (number of keys actually present) (4 bytes).
Let's allocate 16 bytes for the header to be safe and for alignment.

Available Space: 4096 - 16 = 4080 bytes.

Data: We need to fit (2t - 1) keys, (2t - 1) values, and 2t child pointers.

Let max_keys = 2t - 1. This means max_children = 2t = max_keys + 1.

Our equation must be: Size(keys) + Size(values) + Size(children) <= 4080

Size(values) = max_keys * sizeof(int) = max_keys * 4 bytes.

Size(children) = max_children * sizeof(long long) = (max_keys + 1) * 8 bytes (for 64-bit file offsets).

So, Size(keys) + (max_keys * 4) + ((max_keys + 1) * 8) <= 4080 Size(keys) + 4*max_keys + 8*max_keys + 8 <= 4080 Size(keys) + 12*max_keys <= 4072

Now we solve for max_keys based on the two key types.

Case 1: int keys
Size(keys) = max_keys * sizeof(int) = max_keys * 4 bytes.

Our equation becomes: (max_keys * 4) + (12 * max_keys) <= 4072 16 * max_keys <= 4072 max_keys <= 4072 / 16 max_keys <= 254.5

The maximum number of keys must be an integer, so max_keys = 254.

But max_keys must be an odd number (to be 2t - 1). So, we must use the largest odd number less than or equal to 254, which is max_keys = 253.

Now we find t: 2t - 1 = 253 2t = 254 t = 127 (for int keys)

Case 2: string keys
Size(keys) = max_keys * 255 bytes (for char[255]).

Our equation becomes: (max_keys * 255) + (12 * max_keys) <= 4072 267 * max_keys <= 4072 max_keys <= 4072 / 267 max_keys <= 15.25...

The maximum number of keys must be an integer, so max_keys = 15.

This is already an odd number, so max_keys = 15.

Now we find t: 2t - 1 = 15 2t = 16 t = 8 (for string keys)

*/



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
    uint32_t n;
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
    // (Used by BTree logic)

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
