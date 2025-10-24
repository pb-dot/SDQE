#include "db_node.hpp"
#include <stdexcept>
#include <iostream>

Node::Node(uint32_t t, KeyType key_type, bool is_leaf)
    : is_leaf(is_leaf), n(0), t(t), key_type(key_type) {
    max_keys = 2 * t - 1;
    max_children = 2 * t;
    keys.resize(max_keys);
    values.resize(max_keys);
    children.resize(max_children);
}

Node::Node(uint32_t t, KeyType key_type, const char* buffer)
    : t(t), key_type(key_type) {
    max_keys = 2 * t - 1;
    max_children = 2 * t;
    keys.resize(max_keys);
    values.resize(max_keys);
    children.resize(max_children);

    const char* ptr = buffer;

    // 1. Deserialize Node Header
    NodeHeader header;
    memcpy(&header, ptr, sizeof(NodeHeader));
    ptr += sizeof(NodeHeader);
    this->is_leaf = header.is_leaf;
    this->n = header.n;

    // 2. Deserialize Children offsets
    memcpy(children.data(), ptr, sizeof(offset_t) * max_children);
    ptr += sizeof(offset_t) * max_children;

    // 3. Deserialize Values
    memcpy(values.data(), ptr, sizeof(int32_t) * max_keys);
    ptr += sizeof(int32_t) * max_keys;

    // 4. Deserialize Keys
    if (key_type == KeyType::INTEGER) {
        deserializeIntKeys(ptr);
    } else {
        deserializeStringKeys(ptr);
    }
}

void Node::serialize(char* buffer) const {
    char* ptr = buffer;

    // 1. Serialize Node Header
    NodeHeader header;
    header.is_leaf = this->is_leaf;
    header.n = this->n;
    memcpy(ptr, &header, sizeof(NodeHeader));
    ptr += sizeof(NodeHeader);

    // 2. Serialize Children offsets
    memcpy(ptr, children.data(), sizeof(offset_t) * max_children);
    ptr += sizeof(offset_t) * max_children;

    // 3. Serialize Values
    memcpy(ptr, values.data(), sizeof(int32_t) * max_keys);
    ptr += sizeof(int32_t) * max_keys;

    // 4. Serialize Keys
    if (key_type == KeyType::INTEGER) {
        serializeIntKeys(ptr);
    } else {
        serializeStringKeys(ptr);
    }
}

void Node::serializeIntKeys(char*& ptr) const {
    for (uint32_t i = 0; i < n; ++i) {
        int32_t key = stringKeyToInt(keys[i]);
        memcpy(ptr, &key, sizeof(int32_t));
        ptr += sizeof(int32_t);
    }
    // No need to zero out the rest, block is assumed to be
    // written in full.
}

void Node::serializeStringKeys(char*& ptr) const {
    for (uint32_t i = 0; i < n; ++i) {
        // Copy string, ensuring null termination and max size
        char key_buffer[MAX_STRING_KEY_SIZE] = {0};
        keys[i].copy(key_buffer, MAX_STRING_KEY_SIZE - 1);
        memcpy(ptr, key_buffer, MAX_STRING_KEY_SIZE);
        ptr += MAX_STRING_KEY_SIZE;
    }
}

void Node::deserializeIntKeys(const char*& ptr) {
    for (uint32_t i = 0; i < n; ++i) {
        int32_t key;
        memcpy(&key, ptr, sizeof(int32_t));
        ptr += sizeof(int32_t);
        keys[i] = intKeyToString(key);
    }
}

void Node::deserializeStringKeys(const char*& ptr) {
    for (uint32_t i = 0; i < n; ++i) {
        // Create string from fixed-size buffer
        keys[i] = std::string(ptr, strnlen(ptr, MAX_STRING_KEY_SIZE));
        ptr += MAX_STRING_KEY_SIZE;
    }
}

int Node::findKey(const std::string& k) {
    int idx = 0;
    // Simple linear search
    while (idx < n && keys[idx] < k) {
        ++idx;
    }
    return idx;
}

std::string Node::intKeyToString(int32_t key) {
    // A fixed-width, sortable string representation of an int
    // We add 0x80000000 to map to unsigned range for correct
    // lexicographical sorting (e.g. -1 becomes a large positive num)
    uint32_t unsigned_key = key + 0x80000000;
    std::string s(4, 0);
    s[0] = (unsigned_key >> 24) & 0xFF;
    s[1] = (unsigned_key >> 16) & 0xFF;
    s[2] = (unsigned_key >> 8) & 0xFF;
    s[3] = (unsigned_key) & 0xFF;
    return s;
}

int32_t Node::stringKeyToInt(const std::string& key) {
    if (key.length() != 4) {
        // This should not happen in normal operation
        throw std::runtime_error("Invalid int-string format");
    }
    uint32_t unsigned_key = (static_cast<uint8_t>(key[0]) << 24) |
                            (static_cast<uint8_t>(key[1]) << 16) |
                            (static_cast<uint8_t>(key[2]) << 8) |
                            (static_cast<uint8_t>(key[3]));
    return static_cast<int32_t>(unsigned_key - 0x80000000);
}


void Node::splitChild(int i, std::shared_ptr<Node> y, std::shared_ptr<Node> z) {
    // y is child[i], z is the new sibling
    // y is full (n = 2t - 1)

    // z will get the last (t-1) keys from y
    z->n = t - 1;
    for (int j = 0; j < t - 1; j++) {
        z->keys[j] = y->keys[j + t];
        z->values[j] = y->values[j + t];
    }

    // If y is not a leaf, copy its last t children to z
    if (!y->is_leaf) {
        for (int j = 0; j < t; j++) {
            z->children[j] = y->children[j + t];
        }
    }

    // Reduce number of keys in y
    y->n = t - 1;

    // --- Update this node (parent) ---
    // Make space for new child pointer
    for (int j = n; j >= i + 1; j--) {
        children[j + 1] = children[j];
    }
    // (z's offset will be set by the caller, BTree::splitChild)

    // Make space for new key/value
    for (int j = n - 1; j >= i; j--) {
        keys[j + 1] = keys[j];
        values[j + 1] = values[j];
    }

    // Copy middle key/value from y up to this node
    keys[i] = y->keys[t - 1];
    values[i] = y->values[t - 1];

    // Increment key count in this node
    n = n + 1;
}
