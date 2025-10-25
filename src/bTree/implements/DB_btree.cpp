#include "db_btree.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm>

// --- BTree Public API ---

BTree::BTree() {}
BTree::~BTree() { close(); }

bool BTree::create(const std::string& filename, KeyType key_type) {
    uint32_t t_out;
    if (!file.create(filename, key_type, t_out)) {
        return false;
    }
    metadata = file.getMetadata();
    return true;
}

bool BTree::open(const std::string& filename) {
    if (!file.open(filename)) {
        return false;
    }
    metadata = file.getMetadata();
    return true;
}

void BTree::close() {
    file.close();
}



void BTree::insert(int32_t key, int32_t value) {
    if (!file.isOpen()) throw std::runtime_error("BTree file not open.");
    std::string k;
    try {
        k = keyToString(key);
    } catch (const std::exception& e) {
        std::cerr << "Insert Error: " << e.what() << std::endl;
        return;
    }
    offset_t old_root_offset = metadata.root_offset;

    if (old_root_offset == 0) {
        auto root = std::make_shared<Node>(metadata.t, metadata.key_type, true);
        root->keys[0] = k;
        root->values[0] = value;
        root->n = 1;
        offset_t new_root_offset = writeNode(root);
        //  Call  2-phase commit function ---
        if (file.commitRootAndGarbageCollect(new_root_offset)) {
            metadata.root_offset = new_root_offset; // Update in-memory copy
        }
    } else {
        offset_t final_root_offset = insertRecursive(old_root_offset, k, value);
        if (final_root_offset != old_root_offset) {
            // Call 2-phase commit function ---
            if (file.commitRootAndGarbageCollect(final_root_offset)) {
                metadata.root_offset = final_root_offset;
            }
        }
        // If no change, no commit is needed
    }
}

void BTree::insert(const std::string& key, int32_t value) {
    if (!file.isOpen()) throw std::runtime_error("BTree file not open.");
    std::string k;
    try {
        k = keyToString(key);
    } catch (const std::exception& e) {
        std::cerr << "Insert Error: " << e.what() << std::endl;
        return;
    }
    offset_t old_root_offset = metadata.root_offset;

    if (old_root_offset == 0) {
        auto root = std::make_shared<Node>(metadata.t, metadata.key_type, true);
        root->keys[0] = k;
        root->values[0] = value;
        root->n = 1;
        offset_t new_root_offset = writeNode(root);
        if (file.commitRootAndGarbageCollect(new_root_offset)) {
            metadata.root_offset = new_root_offset;
        }
    } else {
        offset_t final_root_offset = insertRecursive(old_root_offset, k, value);
        if (final_root_offset != old_root_offset) {
            if (file.commitRootAndGarbageCollect(final_root_offset)) {
                metadata.root_offset = final_root_offset;
            }
        }
    }
}

std::optional<int32_t> BTree::search(int32_t key) {
    if (!file.isOpen()) return std::nullopt;
    try {
        return searchRecursive(metadata.root_offset, keyToString(key));
    } catch (...) { return std::nullopt; }
}

std::optional<int32_t> BTree::search(const std::string& key) {
    if (!file.isOpen()) return std::nullopt;
    try {
        return searchRecursive(metadata.root_offset, keyToString(key));
    } catch (...) { return std::nullopt; }
}

bool BTree::update(int32_t key, int32_t new_value) {
    if (!file.isOpen()) return false;
    std::string k;
    try {
        k = keyToString(key);
    } catch (...) { return false; }

    bool updated = false;
    offset_t old_root_offset = metadata.root_offset;
    offset_t final_root_offset = updateRecursive(old_root_offset, k, new_value, updated);

    if (final_root_offset != old_root_offset) {
        if (file.commitRootAndGarbageCollect(final_root_offset)) {
            metadata.root_offset = final_root_offset;
        }
    }
    return updated;
}

bool BTree::update(const std::string& key, int32_t new_value) {
    if (!file.isOpen()) return false;
    std::string k;
    try {
        k = keyToString(key);
    } catch (...) { return false; }

    bool updated = false;
    offset_t old_root_offset = metadata.root_offset;
    offset_t final_root_offset = updateRecursive(old_root_offset, k, new_value, updated);

    if (final_root_offset != old_root_offset) {
        if (file.commitRootAndGarbageCollect(final_root_offset)) {
            metadata.root_offset = final_root_offset;
        }
    }
    return updated;
}

void BTree::remove(int32_t key) {
    if (!file.isOpen() || metadata.root_offset == 0) return;
    std::string k;
    try {
        k = keyToString(key);
    } catch (...) { return; }

    offset_t old_root_offset = metadata.root_offset;
    offset_t final_root_offset = removeRecursive(old_root_offset, k);

    if (final_root_offset != old_root_offset) {
        offset_t new_root_to_commit = final_root_offset;

        if (final_root_offset != 0) {
            auto root = readNode(final_root_offset);
            if (root->n == 0 && !root->is_leaf) {
                // Collapse root
                new_root_to_commit = root->children[0];
                file.freeBlock(final_root_offset); // Stage the empty root for deletion
            } else if (root->n == 0 && root->is_leaf) {
                // Tree is now empty
                new_root_to_commit = 0;
                file.freeBlock(final_root_offset); // Stage the empty root for deletion
            }
        }
        // Commit the new root (or 0) and process the free list
        if (file.commitRootAndGarbageCollect(new_root_to_commit)) {
            metadata.root_offset = new_root_to_commit;
        }
    }
}

void BTree::remove(const std::string& key) {
    if (!file.isOpen() || metadata.root_offset == 0) return;
    std::string k;
    try {
        k = keyToString(key);
    } catch (...) { return; }

    offset_t old_root_offset = metadata.root_offset;
    offset_t final_root_offset = removeRecursive(old_root_offset, k);

    if (final_root_offset != old_root_offset) {
        offset_t new_root_to_commit = final_root_offset;

        if (final_root_offset != 0) {
            auto root = readNode(final_root_offset);
            if (root->n == 0 && !root->is_leaf) {
                new_root_to_commit = root->children[0];
                file.freeBlock(final_root_offset);
            } else if (root->n == 0 && root->is_leaf) {
                new_root_to_commit = 0;
                file.freeBlock(final_root_offset);
            }
        }
        if (file.commitRootAndGarbageCollect(new_root_to_commit)) {
            metadata.root_offset = new_root_to_commit;
        }
    }
}

std::vector<BTree::KeyValuePair> BTree::rangeSearch(int32_t min_k, int32_t max_k) {
    std::vector<KeyValuePair> results;
    if (!file.isOpen() || metadata.root_offset == 0) return results;
    std::string min_str, max_str;
    try {
        min_str = keyToString(min_k);
        max_str = keyToString(max_k);
    } catch (...) { return results; }

    rangeSearchRecursive(metadata.root_offset, min_str, max_str, results);
    if (metadata.key_type == KeyType::INTEGER) {
        for (auto& pair : results) {
            pair.first = std::to_string(Node::stringKeyToInt(pair.first));
        }
    }
    return results;
}

std::vector<BTree::KeyValuePair> BTree::rangeSearch(const std::string& min_k, const std::string& max_k) {
    std::vector<KeyValuePair> results;
    if (!file.isOpen() || metadata.root_offset == 0) return results;
    std::string min_str, max_str;
    try {
        min_str = keyToString(min_k);
        max_str = keyToString(max_k);
    } catch (...) { return results; }

    rangeSearchRecursive(metadata.root_offset, min_str, max_str, results);
    return results;
}

void BTree::print() {
    if (!file.isOpen() || metadata.root_offset == 0) {
        std::cout << "(empty)" << std::endl;
        return;
    }
    printRecursive(metadata.root_offset, 0);
}

// --- BTree Private: Node I/O ---

std::shared_ptr<Node> BTree::readNode(offset_t offset) {
    if (offset == 0) return nullptr;
    char buffer[BLOCK_SIZE];
    if (!file.readBlock(offset, buffer)) {
        throw std::runtime_error("Failed to read block " + std::to_string(offset));
    }
    return std::make_shared<Node>(metadata.t, metadata.key_type, buffer);
}

offset_t BTree::writeNode(std::shared_ptr<Node> node) {
    char buffer[BLOCK_SIZE] = {0};
    node->serialize(buffer);
    offset_t new_offset = file.allocateBlock();
    if (new_offset == 0) {
        throw std::runtime_error("Failed to allocate block");
    }
    if (!file.writeBlock(new_offset, buffer)) {
        throw std::runtime_error("Failed to write block " + std::to_string(new_offset));
    }
    return new_offset;
}

// --- BTree Private: Key Helpers ---

std::string BTree::keyToString(int32_t key) {
    if (metadata.key_type != KeyType::INTEGER) {
        throw std::runtime_error("Key type mismatch: This is a STRING tree, but an INT key was provided.");
    }
    return Node::intKeyToString(key);
}

std::string BTree::keyToString(const std::string& key) {
    if (metadata.key_type != KeyType::STRING) {
        throw std::runtime_error("Key type mismatch: This is an INT tree, but a STRING key was provided.");
    }
    if (key.length() >= MAX_STRING_KEY_SIZE) {
        return key.substr(0, MAX_STRING_KEY_SIZE - 1);
    }
    return key;
}

// --- BTree Private: Search ---

std::optional<int32_t> BTree::searchRecursive(offset_t node_offset, const std::string& k) {
    if (node_offset == 0) return std::nullopt;
    auto node = readNode(node_offset);
    int i = node->findKey(k);
    if (i < node->n && node->keys[i] == k) {
        return node->values[i];
    }
    if (node->is_leaf) return std::nullopt;
    return searchRecursive(node->children[i], k);
}



offset_t BTree::updateRecursive(offset_t node_offset, const std::string& k, int32_t new_value, bool& updated) {
    if (node_offset == 0) return 0;
    auto node = readNode(node_offset);
    int i = node->findKey(k);

    if (i < node->n && node->keys[i] == k) {
        node->values[i] = new_value;
        updated = true;
        offset_t new_node_offset = writeNode(node);
        file.freeBlock(node_offset); // Stage old block for deletion
        return new_node_offset;
    }
    if (node->is_leaf) return node_offset;

    offset_t old_child_offset = node->children[i];
    offset_t new_child_offset = updateRecursive(old_child_offset, k, new_value, updated);

    if (new_child_offset == old_child_offset) return node_offset;

    file.freeBlock(old_child_offset); // Stage old child for deletion
    node->children[i] = new_child_offset;
    offset_t new_parent_offset = writeNode(node);
    file.freeBlock(node_offset); // Stage old parent for deletion
    return new_parent_offset;
}

// --- BTree Private: Insert (CoW -Copy On Write) ---

offset_t BTree::insertRecursive(offset_t node_offset, const std::string& k, int32_t v) {
    auto node = readNode(node_offset);

    if (node->n == 2 * metadata.t - 1) {
        auto s = std::make_shared<Node>(metadata.t, metadata.key_type, false);
        s->children[0] = node_offset;

        // splitChild writes 3 new nodes and *stages* the 2 old ones for deletion
        offset_t new_root_offset = splitChild(s, 0, 0);
        auto new_root_node = readNode(new_root_offset);
        int i = 0;
        if (new_root_node->keys[0] < k) i++;

        offset_t child_offset = new_root_node->children[i];
        auto child_node = readNode(child_offset);
        offset_t new_child_offset = insertNonFull(child_node, child_offset, k, v);

        if (new_child_offset != child_offset) {
            file.freeBlock(child_offset);
            new_root_node->children[i] = new_child_offset;
            offset_t final_root_offset = writeNode(new_root_node);
            file.freeBlock(new_root_offset);
            return final_root_offset;
        }
        return new_root_offset;
    }
    return insertNonFull(node, node_offset, k, v);
}

offset_t BTree::insertNonFull(std::shared_ptr<Node> node, offset_t node_offset, const std::string& k, int32_t v) {
    int i = node->n - 1;
    if (node->is_leaf) {
        while (i >= 0 && node->keys[i] > k) {
            node->keys[i + 1] = node->keys[i];
            node->values[i + 1] = node->values[i];
            i--;
        }
        node->keys[i + 1] = k;
        node->values[i + 1] = v;
        node->n = node->n + 1;
        offset_t new_leaf_offset = writeNode(node);
        file.freeBlock(node_offset);
        return new_leaf_offset;
    }

    while (i >= 0 && node->keys[i] > k) i--;
    i++;

    offset_t child_offset = node->children[i];
    auto child_node = readNode(child_offset);

    if (child_node->n == 2 * metadata.t - 1) {
        offset_t new_parent_offset = splitChild(node, node_offset, i);
        auto new_parent_node = readNode(new_parent_offset);
        if (new_parent_node->keys[i] < k) i++;

        offset_t child_to_descend_offset = new_parent_node->children[i];
        auto child_to_descend_node = readNode(child_to_descend_offset);
        offset_t new_child_offset = insertNonFull(child_to_descend_node, child_to_descend_offset, k, v);

        if (new_child_offset != child_to_descend_offset) {
            file.freeBlock(child_to_descend_offset);
            new_parent_node->children[i] = new_child_offset;
            offset_t final_parent_offset = writeNode(new_parent_node);
            file.freeBlock(new_parent_offset);
            return final_parent_offset;
        }
        return new_parent_offset;
    }

    offset_t new_child_offset = insertNonFull(child_node, child_offset, k, v);

    if (new_child_offset != child_offset) {
        file.freeBlock(child_offset);
        node->children[i] = new_child_offset;
        offset_t new_parent_offset = writeNode(node);
        file.freeBlock(node_offset);
        return new_parent_offset;
    }
    return node_offset;
}

offset_t BTree::splitChild(std::shared_ptr<Node> parent_node, offset_t parent_offset, int i) {
    offset_t y_offset = parent_node->children[i];
    auto y = readNode(y_offset);
    auto z = std::make_shared<Node>(metadata.t, metadata.key_type, y->is_leaf);

    parent_node->splitChild(i, y, z);

    offset_t z_offset = writeNode(z);
    offset_t new_y_offset = writeNode(y);

    parent_node->children[i] = new_y_offset;
    parent_node->children[i + 1] = z_offset;
    offset_t new_parent_offset = writeNode(parent_node);

    // Stage old blocks for deletion
    file.freeBlock(y_offset);
    if (parent_offset != 0) { // Don't free offset 0 (used for new root)
        file.freeBlock(parent_offset);
    }
    return new_parent_offset;
}

// --- BTree Private: Remove (CoW) ---

offset_t BTree::removeRecursive(offset_t node_offset, const std::string& k) {
    if (node_offset == 0) return 0;

    auto node = readNode(node_offset);
    int idx = node->findKey(k);

    if (idx < node->n && node->keys[idx] == k) {
        if (node->is_leaf) {
            return removeFromLeaf(node, node_offset, idx);
        } else {
            return removeFromNonLeaf(node, node_offset, idx);
        }
    }
    if (node->is_leaf) return node_offset;

    bool is_last_child = (idx == node->n);
    offset_t child_offset = node->children[idx];
    auto child_node = readNode(child_offset);

    if (child_node->n < metadata.t) {
        // `fill` stages the old parent and child/sibling blocks
        offset_t new_parent_offset = fill(node, node_offset, idx);
        auto new_parent_node = readNode(new_parent_offset);
        offset_t new_child_to_descend_offset;

        if (is_last_child && idx > new_parent_node->n) {
            new_child_to_descend_offset = new_parent_node->children[idx - 1];
        } else {
            new_child_to_descend_offset = new_parent_node->children[idx];
        }
        offset_t final_child_offset = removeRecursive(new_child_to_descend_offset, k);

        if (final_child_offset != new_child_to_descend_offset) {
            file.freeBlock(new_child_to_descend_offset);
            if (is_last_child && idx > new_parent_node->n) {
                new_parent_node->children[idx - 1] = final_child_offset;
            } else {
                new_parent_node->children[idx] = final_child_offset;
            }
            offset_t final_parent_offset = writeNode(new_parent_node);
            file.freeBlock(new_parent_offset);
            return final_parent_offset;
        }
        return new_parent_offset;
    }

    offset_t new_child_offset = removeRecursive(child_offset, k);

    if (new_child_offset != child_offset) {
        file.freeBlock(child_offset);
        node->children[idx] = new_child_offset;
        offset_t new_parent_offset = writeNode(node);
        file.freeBlock(node_offset);
        return new_parent_offset;
    }
    return node_offset;
}

offset_t BTree::removeFromLeaf(std::shared_ptr<Node> node, offset_t node_offset, int idx) {
    for (int i = idx + 1; i < node->n; ++i) {
        node->keys[i - 1] = node->keys[i];
        node->values[i - 1] = node->values[i];
    }
    node->n--;
    offset_t new_node_offset = writeNode(node);
    file.freeBlock(node_offset);
    return new_node_offset;
}

offset_t BTree::removeFromNonLeaf(std::shared_ptr<Node> node, offset_t node_offset, int idx) {
    std::string k = node->keys[idx];
    offset_t pred_child_offset = node->children[idx];
    offset_t succ_child_offset = node->children[idx + 1];
    auto pred_child = readNode(pred_child_offset);

    if (pred_child->n >= metadata.t) {
        auto pred_node = getPred(pred_child_offset, idx);
        node->keys[idx] = pred_node->keys[pred_node->n - 1];
        node->values[idx] = pred_node->values[pred_node->n - 1];
        offset_t new_child_offset = removeRecursive(pred_child_offset, pred_node->keys[pred_node->n - 1]);
        file.freeBlock(pred_child_offset);
        node->children[idx] = new_child_offset;
        offset_t new_parent_offset = writeNode(node);
        file.freeBlock(node_offset);
        return new_parent_offset;
    }

    auto succ_child = readNode(succ_child_offset);
    if (succ_child->n >= metadata.t) {
        auto succ_node = getSucc(succ_child_offset, idx);
        node->keys[idx] = succ_node->keys[0];
        node->values[idx] = succ_node->values[0];
        offset_t new_child_offset = removeRecursive(succ_child_offset, succ_node->keys[0]);
        file.freeBlock(succ_child_offset);
        node->children[idx + 1] = new_child_offset;
        offset_t new_parent_offset = writeNode(node);
        file.freeBlock(node_offset);
        return new_parent_offset;
    }

    offset_t new_parent_offset = merge(node, node_offset, idx);
    auto new_parent = readNode(new_parent_offset);
    offset_t merged_child_offset = new_parent->children[idx];
    offset_t final_child_offset = removeRecursive(merged_child_offset, k);

    if (final_child_offset != merged_child_offset) {
        file.freeBlock(merged_child_offset);
        new_parent->children[idx] = final_child_offset;
        offset_t final_parent_offset = writeNode(new_parent);
        file.freeBlock(new_parent_offset);
        return final_parent_offset;
    }
    return new_parent_offset;
}

std::shared_ptr<Node> BTree::getPred(offset_t node_offset, int idx) {
    auto cur = readNode(node_offset);
    while (!cur->is_leaf) cur = readNode(cur->children[cur->n]);
    return cur;
}
std::shared_ptr<Node> BTree::getSucc(offset_t node_offset, int idx) {
    auto cur = readNode(node_offset);
    while (!cur->is_leaf) cur = readNode(cur->children[0]);
    return cur;
}

offset_t BTree::fill(std::shared_ptr<Node> node, offset_t node_offset, int idx) {
    if (idx != 0 && readNode(node->children[idx - 1])->n >= metadata.t) {
        return borrowFromPrev(node, node_offset, idx);
    }
    else if (idx != node->n && readNode(node->children[idx + 1])->n >= metadata.t) {
        return borrowFromNext(node, node_offset, idx);
    }
    else {
        if (idx != node->n) return merge(node, node_offset, idx);
        else return merge(node, node_offset, idx - 1);
    }
}

offset_t BTree::borrowFromPrev(std::shared_ptr<Node> node, offset_t node_offset, int idx) {
    offset_t child_offset = node->children[idx];
    offset_t sibling_offset = node->children[idx - 1];
    auto child = readNode(child_offset);
    auto sibling = readNode(sibling_offset);

    for (int i = child->n - 1; i >= 0; --i) {
        child->keys[i + 1] = child->keys[i];
        child->values[i + 1] = child->values[i];
    }
    if (!child->is_leaf) {
        for (int i = child->n; i >= 0; --i) child->children[i + 1] = child->children[i];
    }
    child->keys[0] = node->keys[idx - 1];
    child->values[0] = node->values[idx - 1];
    if (!child->is_leaf) child->children[0] = sibling->children[sibling->n];
    node->keys[idx - 1] = sibling->keys[sibling->n - 1];
    node->values[idx - 1] = sibling->values[sibling->n - 1];
    child->n += 1;
    sibling->n -= 1;

    offset_t new_child_offset = writeNode(child);
    offset_t new_sibling_offset = writeNode(sibling);
    node->children[idx] = new_child_offset;
    node->children[idx - 1] = new_sibling_offset;
    offset_t new_parent_offset = writeNode(node);

    file.freeBlock(child_offset);
    file.freeBlock(sibling_offset);
    file.freeBlock(node_offset);
    return new_parent_offset;
}

offset_t BTree::borrowFromNext(std::shared_ptr<Node> node, offset_t node_offset, int idx) {
    offset_t child_offset = node->children[idx];
    offset_t sibling_offset = node->children[idx + 1];
    auto child = readNode(child_offset);
    auto sibling = readNode(sibling_offset);

    child->keys[child->n] = node->keys[idx];
    child->values[child->n] = node->values[idx];
    if (!child->is_leaf) child->children[child->n + 1] = sibling->children[0];
    node->keys[idx] = sibling->keys[0];
    node->values[idx] = sibling->values[0];
    for (int i = 1; i < sibling->n; ++i) {
        sibling->keys[i - 1] = sibling->keys[i];
        sibling->values[i - 1] = sibling->values[i];
    }
    if (!sibling->is_leaf) {
        for (int i = 1; i <= sibling->n; ++i) sibling->children[i - 1] = sibling->children[i];
    }
    child->n += 1;
    sibling->n -= 1;

    offset_t new_child_offset = writeNode(child);
    offset_t new_sibling_offset = writeNode(sibling);
    node->children[idx] = new_child_offset;
    node->children[idx + 1] = new_sibling_offset;
    offset_t new_parent_offset = writeNode(node);

    file.freeBlock(child_offset);
    file.freeBlock(sibling_offset);
    file.freeBlock(node_offset);
    return new_parent_offset;
}

offset_t BTree::merge(std::shared_ptr<Node> node, offset_t node_offset, int idx) {
    offset_t child_offset = node->children[idx];
    offset_t sibling_offset = node->children[idx + 1];
    auto child = readNode(child_offset);
    auto sibling = readNode(sibling_offset);

    child->keys[metadata.t - 1] = node->keys[idx];
    child->values[metadata.t - 1] = node->values[idx];
    for (int i = 0; i < sibling->n; ++i) {
        child->keys[i + metadata.t] = sibling->keys[i];
        child->values[i + metadata.t] = sibling->values[i];
    }
    if (!child->is_leaf) {
        for (int i = 0; i <= sibling->n; ++i) child->children[i + metadata.t] = sibling->children[i];
    }
    for (int i = idx + 1; i < node->n; ++i) {
        node->keys[i - 1] = node->keys[i];
        node->values[i - 1] = node->values[i];
    }
    for (int i = idx + 2; i <= node->n; ++i) node->children[i - 1] = node->children[i];
    child->n += sibling->n + 1;
    node->n--;

    offset_t new_child_offset = writeNode(child);
    node->children[idx] = new_child_offset;
    offset_t new_parent_offset = writeNode(node);

    file.freeBlock(child_offset);
    file.freeBlock(sibling_offset);
    file.freeBlock(node_offset);
    return new_parent_offset;
}

// --- BTree Private: Range Search & Print ---

void BTree::rangeSearchRecursive(offset_t node_offset, const std::string& min_k, const std::string& max_k, std::vector<KeyValuePair>& results) {
    if (node_offset == 0) return;
    auto node = readNode(node_offset);
    int i = 0;
    for (i = 0; i < node->n; i++) {
        if (node->keys[i] > max_k && !node->is_leaf) {
             rangeSearchRecursive(node->children[i], min_k, max_k, results);
             return;
        }
        if (!node->is_leaf && node->keys[i] >= min_k) {
            rangeSearchRecursive(node->children[i], min_k, max_k, results);
        }
        if (node->keys[i] >= min_k && node->keys[i] <= max_k) {
            results.push_back({node->keys[i], node->values[i]});
        }
        if (node->keys[i] > max_k) return;
    }
    if (!node->is_leaf) {
        rangeSearchRecursive(node->children[i], min_k, max_k, results);
    }
}

void BTree::printRecursive(offset_t node_offset, int level) {
    if (node_offset == 0) return;
    std::shared_ptr<Node> node;
    try {
        node = readNode(node_offset);
    } catch (const std::exception& e) {
        std::cout << std::string(level * 4, ' ') << "[Node " << node_offset
                  << "] (ERROR: " << e.what() << ")" << std::endl;
        return;
    }
    std::string indent(level * 4, ' ');
    std::cout << indent << "[Node " << node_offset << "] (n=" << node->n << ") "
              << (node->is_leaf ? "(LEAF)" : "") << std::endl;
    int i;
    for (i = 0; i < node->n; i++) {
        if (!node->is_leaf) printRecursive(node->children[i], level + 1);
        std::cout << indent << "  - Key: ";
        if (metadata.key_type == KeyType::INTEGER) {
            if(node->keys[i].length() == 4) {
                std::cout << Node::stringKeyToInt(node->keys[i]);
            } else {
                std::cout << "INVALID_INT_KEY_FORMAT";
            }
        } else {
            std::cout << "'" << node->keys[i] << "'";
        }
        std::cout << ", Val: " << node->values[i] << std::endl;
    }
    if (!node->is_leaf) printRecursive(node->children[i], level + 1);
}
