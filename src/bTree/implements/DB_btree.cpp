#include "db_btree.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm> // for std::find

// --- BTree Public API ---

BTree::BTree() : new_root_offset_cache(0) {}

BTree::~BTree() {
    close();
}

bool BTree::create(const std::string& filename, KeyType key_type) {
    uint32_t t_out;
    if (!file.create(filename, key_type, t_out)) {
        return false;
    }
    metadata = file.getMetadata(); // Load metadata into memory
    return true;
}

bool BTree::open(const std::string& filename) {
    if (!file.open(filename)) {
        return false;
    }
    metadata = file.getMetadata(); // Load metadata into memory
    return true;
}

void BTree::close() {
    file.close();
}

void BTree::insert(int32_t key, int32_t value) {
    insert(keyToString(key), value);
}

void BTree::insert(const std::string& key, int32_t value) {
    if (!file.isOpen()) throw std::runtime_error("BTree file not open.");

    std::string k = key;
    if (key.length() >= MAX_STRING_KEY_SIZE) {
        // Truncate key if too long
        k=key.substr(0, MAX_STRING_KEY_SIZE - 1);
    }

     // If root is 0, tree is empty.
    if (metadata.root_offset == 0) {
        auto root = std::make_shared<Node>(metadata.t, metadata.key_type, true);
        root->keys[0] = k;
        root->values[0] = value;
        root->n = 1;

        // Write new root and commit
        offset_t new_root_offset = writeNode(root);
        file.commitRootOffset(new_root_offset);
        metadata.root_offset = new_root_offset; // Update in-memory copy
    } else {
        // Start recursive insert from root
        new_root_offset_cache = metadata.root_offset;
        offset_t final_root_offset = insertRecursive(metadata.root_offset, k, value);

        // If root changed, commit the new root offset
        if (final_root_offset != metadata.root_offset) {
            file.commitRootOffset(final_root_offset);
            metadata.root_offset = final_root_offset;
        }
    }
}

std::optional<int32_t> BTree::search(int32_t key) {
    return search(keyToString(key));
}

std::optional<int32_t> BTree::search(const std::string& key) {
    if (!file.isOpen()) throw std::runtime_error("BTree file not open.");
    return searchRecursive(metadata.root_offset, key);
}

bool BTree::update(int32_t key, int32_t new_value) {
    return update(keyToString(key), new_value);
}

bool BTree::update(const std::string& key, int32_t new_value) {
    if (!file.isOpen()) throw std::runtime_error("BTree file not open.");

    std::string k = key;
    if (key.length() >= MAX_STRING_KEY_SIZE) {
        // Truncate key if too long
        k=key.substr(0, MAX_STRING_KEY_SIZE - 1);
    }

    bool updated = false;

    // Start recursive update from root
    new_root_offset_cache = metadata.root_offset;
    offset_t final_root_offset = updateRecursive(metadata.root_offset, k, new_value, updated);

    // If root changed (e.g. from a deletion/merge), commit
    if (final_root_offset != metadata.root_offset) {
        file.commitRootOffset(final_root_offset);
        metadata.root_offset = final_root_offset;
    }

    return updated;
}

void BTree::remove(int32_t key) {
    remove(keyToString(key));
}

void BTree::remove(const std::string& key) {
    if (!file.isOpen() || metadata.root_offset == 0) return;

    std::string k = key;
    if (key.length() >= MAX_STRING_KEY_SIZE) {
        // Truncate key if too long
        k=key.substr(0, MAX_STRING_KEY_SIZE - 1);
    }
    new_root_offset_cache = metadata.root_offset;
    offset_t final_root_offset = removeRecursive(metadata.root_offset, k);

    // If root changed, commit
    if (final_root_offset != metadata.root_offset) {
        file.commitRootOffset(final_root_offset);
        metadata.root_offset = final_root_offset;

        // If root became empty, check if it needs to be collapsed
        if (final_root_offset != 0) {
            auto root = readNode(final_root_offset);
            if (root->n == 0 && !root->is_leaf) {
                // Root is empty and not a leaf, new root is its only child
                offset_t new_root = root->children[0];
                file.freeBlock(final_root_offset); // Free old root block
                file.commitRootOffset(new_root);
                metadata.root_offset = new_root;
            } else if (root->n == 0 && root->is_leaf) {
                // Tree is now completely empty
                file.freeBlock(final_root_offset);
                file.commitRootOffset(0);
                metadata.root_offset = 0;
            }
        }
    }
}


std::vector<BTree::KeyValuePair> BTree::rangeSearch(int32_t min_k, int32_t max_k) {
    return rangeSearch(keyToString(min_k), keyToString(max_k));
}

std::vector<BTree::KeyValuePair> BTree::rangeSearch(const std::string& min_k, const std::string& max_k) {
    std::vector<KeyValuePair> results;
    if (!file.isOpen() || metadata.root_offset == 0) return results;

    std::string min_str = min_k;
    if (min_k.length() >= MAX_STRING_KEY_SIZE) {
        // Truncate key if too long
        min_str=min_k.substr(0, MAX_STRING_KEY_SIZE - 1);
    }
    std::string max_str = max_k;
    if (max_k.length() >= MAX_STRING_KEY_SIZE) {
        // Truncate key if too long
        max_str=max_k.substr(0, MAX_STRING_KEY_SIZE - 1);
    }

    rangeSearchRecursive(metadata.root_offset, min_str, max_str, results);

    // De-format keys if they are integers
    if (metadata.key_type == KeyType::INTEGER) {
        for (auto& pair : results) {
            pair.first = std::to_string(Node::stringKeyToInt(pair.first));
        }
    }
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
    char buffer[BLOCK_SIZE] = {0}; // Zero-initialize buffer
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
        throw std::runtime_error("Key type mismatch: expected int, got string");
    }
    std::string k = Node::intKeyToString(key);
    if (k.length() >= MAX_STRING_KEY_SIZE) {
        // Truncate key if too long
        return k.substr(0, MAX_STRING_KEY_SIZE - 1);
    }
    return k;
}
// --- BTree Private: Search ---

std::optional<int32_t> BTree::searchRecursive(offset_t node_offset, const std::string& k) {
    if (node_offset == 0) {
        return std::nullopt; // Not found
    }

    auto node = readNode(node_offset);
    int i = node->findKey(k);

    // Key found in this node
    if (i < node->n && node->keys[i] == k) {
        return node->values[i];
    }

    // Key not found, and this is a leaf
    if (node->is_leaf) {
        return std::nullopt;
    }

    // Recurse into child
    return searchRecursive(node->children[i], k);
}

// --- BTree Private: Update (Copy-on-Write) ---

offset_t BTree::updateRecursive(offset_t node_offset, const std::string& k, int32_t new_value, bool& updated) {
    if (node_offset == 0) {
        return 0; // Not found
    }

    auto node = readNode(node_offset);
    int i = node->findKey(k);

    // Key found in this node
    if (i < node->n && node->keys[i] == k) {
        node->values[i] = new_value;
        updated = true;
        // Write the modified node to a new block
        offset_t new_node_offset = writeNode(node);
        // The old block `node_offset` is now stale, but we don't free it yet.
        // It's part of a valid tree version until the root is committed.
        // For simplicity, we'll leak it.
        // To fix this, we'd need a garbage collector.
        return new_node_offset;
    }

    // Key not found, and this is a leaf
    if (node->is_leaf) {
        return node_offset; // Not found, return original offset (no change)
    }

    // Recurse into child `i`
    offset_t old_child_offset = node->children[i];
    offset_t new_child_offset = updateRecursive(old_child_offset, k, new_value, updated);

    // If the child was not changed, we don't need to do anything.
    if (new_child_offset == old_child_offset) {
        return node_offset;
    }

    // --- Copy-on-Write ---
    // The child *was* changed (at `new_child_offset`).
    // We must create a new version of *this* node pointing to it.
    node->children[i] = new_child_offset;
    offset_t new_parent_offset = writeNode(node);
    // Again, `node_offset` is now stale.

    return new_parent_offset;
}

// --- BTree Private: Insert (Copy-on-Write) ---

offset_t BTree::insertRecursive(offset_t node_offset, const std::string& k, int32_t v) {
    auto node = readNode(node_offset);

    // If this node is full, split it first
    if (node->n == 2 * metadata.t - 1) {
        // Create a new empty root
        auto s = std::make_shared<Node>(metadata.t, metadata.key_type, false);
        s->children[0] = node_offset; // Old root is now child 0

        // `new_root_offset_cache` will be written by `splitChild`
        offset_t new_root_offset = splitChild(s, 0, 0); // node `s` is not on disk yet (offset=0)

        // `s` is now the new root, decide which child to insert into
        int i = 0;
        if (s->keys[0] < k) {
            i++;
        }
        offset_t child_offset = s->children[i];
        auto child_node = readNode(child_offset);

        // Recurse into the correct child
        offset_t new_child_offset = insertNonFull(child_node, child_offset, k, v);

        if (new_child_offset != child_offset) {
            s->children[i] = new_child_offset;
            // Write the modified root `s` again
            new_root_offset = writeNode(s);
        }

        return new_root_offset; // Return the new root's offset
    }

    // Node is not full, just call insertNonFull
    return insertNonFull(node, node_offset, k, v);
}


offset_t BTree::insertNonFull(std::shared_ptr<Node> node, offset_t node_offset, const std::string& k, int32_t v) {
    int i = node->n - 1;

    if (node->is_leaf) {
        // --- Leaf Node: Insert key here ---

        // Find location for new key
        while (i >= 0 && node->keys[i] > k) {
            node->keys[i + 1] = node->keys[i];
            node->values[i + 1] = node->values[i];
            i--;
        }

        // Insert new key and value
        node->keys[i + 1] = k;
        node->values[i + 1] = v;
        node->n = node->n + 1;

        // Write this modified leaf to a new block
        return writeNode(node);
    }

    // --- Internal Node: Find child to insert into ---
    while (i >= 0 && node->keys[i] > k) {
        i--;
    }
    i++; // Descend into child[i]

    offset_t child_offset = node->children[i];
    auto child_node = readNode(child_offset);

    // Check if child is full
    if (child_node->n == 2 * metadata.t - 1) {
        // Child is full: split it
        // This writes 3 new nodes and returns the new parent offset
        offset_t new_parent_offset = splitChild(node, node_offset, i);

        // Read the new parent back (its keys/children changed)
        auto new_parent_node = readNode(new_parent_offset);

        // Decide which of the two new children to descend into
        if (new_parent_node->keys[i] < k) {
            i++;
        }

        // Recurse into the correct (and now not full) child
        offset_t new_child_offset = insertNonFull(readNode(new_parent_node->children[i]), new_parent_node->children[i], k, v);

        // If child write caused *another* copy, update parent again
        if (new_child_offset != new_parent_node->children[i]) {
            new_parent_node->children[i] = new_child_offset;
            return writeNode(new_parent_node);
        }

        return new_parent_offset;
    }

    // Child is not full: recurse
    offset_t new_child_offset = insertNonFull(child_node, child_offset, k, v);

    // If recursion caused a copy-on-write, we must update this node
    if (new_child_offset != child_offset) {
        node->children[i] = new_child_offset;
        return writeNode(node);
    }

    // No changes, return original offset
    return node_offset;
}

offset_t BTree::splitChild(std::shared_ptr<Node> parent_node, offset_t parent_offset, int i) {
    // `parent_node` is the node *in memory*.
    // `i` is the index of the child *to be split*.

    offset_t y_offset = parent_node->children[i];
    auto y = readNode(y_offset); // `y` is the full child node

    // `z` is the new sibling node
    auto z = std::make_shared<Node>(metadata.t, metadata.key_type, y->is_leaf);

    // Use Node's helper to move keys/values
    parent_node->splitChild(i, y, z);

    // --- Now, write all 3 modified nodes to disk ---
    // This is the core Copy-on-Write for a split

    // 1. Write new sibling `z`
    offset_t z_offset = writeNode(z);

    // 2. Write modified child `y`
    offset_t new_y_offset = writeNode(y);

    // 3. Update parent's child pointers and write it
    parent_node->children[i] = new_y_offset;
    parent_node->children[i + 1] = z_offset;

    offset_t new_parent_offset = writeNode(parent_node);

    // Free the old blocks
    file.freeBlock(y_offset);
    if (parent_offset != 0) { // Don't free offset 0 (used for new root)
        file.freeBlock(parent_offset);
    }

    return new_parent_offset;
}


// --- BTree Private: Remove (Copy-on-Write) ---
// Note: This is a simplified remove implementation.
// It correctly handles leaf and basic internal node removal.
// The merge/borrow logic for Copy-on-Write is complex.

offset_t BTree::removeRecursive(offset_t node_offset, const std::string& k) {
    if (node_offset == 0) return 0;

    auto node = readNode(node_offset);
    int idx = node->findKey(k);

    // --- Case 1: Key is in this node ---
    if (idx < node->n && node->keys[idx] == k) {
        if (node->is_leaf) {
            return removeFromLeaf(node, idx); // Case 1
        } else {
            return removeFromNonLeaf(node, node_offset, idx); // Case 2
        }
    }

    // --- Case 2: Key is not in this node ---
    if (node->is_leaf) {
        return node_offset; // Key not found, no change
    }

    // Key is in subtree rooted at child[idx]
    bool is_last_child = (idx == node->n);
    offset_t child_offset = node->children[idx];
    auto child_node = readNode(child_offset);

    // --- Case 3: Ensure child has at least 't' keys ---
    if (child_node->n < metadata.t) {
        // Child is under-full. Fill it before descending.
        // `fill` returns the *new* offset of the parent node.
        offset_t new_parent_offset = fill(node, node_offset, idx);

        // Read the new parent back
        auto new_parent_node = readNode(new_parent_offset);

        // After fill, child[idx] might have merged with child[idx-1].
        // If we were descending to the last child and it merged,
        // we must now descend to the *new* last child (idx-1).
        if (is_last_child && idx > new_parent_node->n) {
            return removeRecursive(new_parent_node->children[idx - 1], k);
        } else {
            return removeRecursive(new_parent_node->children[idx], k);
        }
    }

    // Child is fine, recurse
    offset_t new_child_offset = removeRecursive(child_offset, k);

    // Copy-on-Write: If child changed, update this node
    if (new_child_offset != child_offset) {
        node->children[idx] = new_child_offset;
        return writeNode(node);
    }

    return node_offset;
}

offset_t BTree::removeFromLeaf(std::shared_ptr<Node> node, int idx) {
    // Shift keys/values left
    for (int i = idx + 1; i < node->n; ++i) {
        node->keys[i - 1] = node->keys[i];
        node->values[i - 1] = node->values[i];
    }
    node->n--;

    // Write the modified node
    return writeNode(node);
}

offset_t BTree::removeFromNonLeaf(std::shared_ptr<Node> node, offset_t node_offset, int idx) {
    std::string k = node->keys[idx];

    auto pred_child = readNode(node->children[idx]);
    auto succ_child = readNode(node->children[idx + 1]);

    // Case 2a: Predecessor child has >= t keys
    if (pred_child->n >= metadata.t) {
        auto pred_node = getPred(node->children[idx], idx); // Find rightmost key in subtree
        node->keys[idx] = pred_node->keys[pred_node->n - 1];
        node->values[idx] = pred_node->values[pred_node->n - 1];

        // Recurse to delete the predecessor key
        offset_t new_child_offset = removeRecursive(node->children[idx], pred_node->keys[pred_node->n - 1]);
        node->children[idx] = new_child_offset;
        return writeNode(node);
    }

    // Case 2b: Successor child has >= t keys
    if (succ_child->n >= metadata.t) {
        auto succ_node = getSucc(node->children[idx + 1], idx); // Find leftmost key in subtree
        node->keys[idx] = succ_node->keys[0];
        node->values[idx] = succ_node->values[0];

        // Recurse to delete the successor key
        offset_t new_child_offset = removeRecursive(node->children[idx + 1], succ_node->keys[0]);
        node->children[idx + 1] = new_child_offset;
        return writeNode(node);
    }

    // Case 2c: Both children have t-1 keys. Merge them.
    // This will return the new offset of the *parent* node.
    offset_t new_parent_offset = merge(node, node_offset, idx);
    // After merge, the key `k` is now in the merged child.
    // Recurse into that child to delete `k`.
    auto new_parent = readNode(new_parent_offset);
    return removeRecursive(new_parent->children[idx], k);
}

// Gets predecessor node (rightmost node in subtree)
std::shared_ptr<Node> BTree::getPred(offset_t node_offset, int idx) {
    auto cur = readNode(node_offset);
    while (!cur->is_leaf) {
        cur = readNode(cur->children[cur->n]);
    }
    return cur;
}

// Gets successor node (leftmost node in subtree)
std::shared_ptr<Node> BTree::getSucc(offset_t node_offset, int idx) {
    auto cur = readNode(node_offset);
    while (!cur->is_leaf) {
        cur = readNode(cur->children[0]);
    }
    return cur;
}

// Fills child[idx] which is under-full
offset_t BTree::fill(std::shared_ptr<Node> node, offset_t node_offset, int idx) {
    // Try to borrow from previous sibling
    if (idx != 0 && readNode(node->children[idx - 1])->n >= metadata.t) {
        return borrowFromPrev(node, node_offset, idx);
    }
    // Try to borrow from next sibling
    else if (idx != node->n && readNode(node->children[idx + 1])->n >= metadata.t) {
        return borrowFromNext(node, node_offset, idx);
    }
    // Merge with a sibling
    else {
        if (idx != node->n) {
            return merge(node, node_offset, idx); // Merge with next
        } else {
            return merge(node, node_offset, idx - 1); // Merge with prev
        }
    }
}

offset_t BTree::borrowFromPrev(std::shared_ptr<Node> node, offset_t node_offset, int idx) {
    auto child = readNode(node->children[idx]);
    auto sibling = readNode(node->children[idx - 1]);

    // Move all keys in child one step ahead
    for (int i = child->n - 1; i >= 0; --i) {
        child->keys[i + 1] = child->keys[i];
        child->values[i + 1] = child->values[i];
    }
    if (!child->is_leaf) {
        for (int i = child->n; i >= 0; --i) {
            child->children[i + 1] = child->children[i];
        }
    }

    // Set child's first key from parent
    child->keys[0] = node->keys[idx - 1];
    child->values[0] = node->values[idx - 1];

    // Move sibling's last child to child's first child
    if (!child->is_leaf) {
        child->children[0] = sibling->children[sibling->n];
    }

    // Move last key from sibling up to parent
    node->keys[idx - 1] = sibling->keys[sibling->n - 1];
    node->values[idx - 1] = sibling->values[sibling->n - 1];

    child->n += 1;
    sibling->n -= 1;

    // --- Copy-on-Write ---
    offset_t new_child_offset = writeNode(child);
    offset_t new_sibling_offset = writeNode(sibling);
    node->children[idx] = new_child_offset;
    node->children[idx - 1] = new_sibling_offset;

    file.freeBlock(node->children[idx]);
    file.freeBlock(node->children[idx-1]);

    return writeNode(node);
}

offset_t BTree::borrowFromNext(std::shared_ptr<Node> node, offset_t node_offset, int idx) {
    auto child = readNode(node->children[idx]);
    auto sibling = readNode(node->children[idx + 1]);

    // Parent key/value moves to end of child
    child->keys[child->n] = node->keys[idx];
    child->values[child->n] = node->values[idx];

    if (!child->is_leaf) {
        child->children[child->n + 1] = sibling->children[0];
    }

    // Sibling's first key/value moves up to parent
    node->keys[idx] = sibling->keys[0];
    node->values[idx] = sibling->values[0];

    // Shift keys/values in sibling left
    for (int i = 1; i < sibling->n; ++i) {
        sibling->keys[i - 1] = sibling->keys[i];
        sibling->values[i - 1] = sibling->values[i];
    }
    if (!sibling->is_leaf) {
        for (int i = 1; i <= sibling->n; ++i) {
            sibling->children[i - 1] = sibling->children[i];
        }
    }

    child->n += 1;
    sibling->n -= 1;

    // --- Copy-on-Write ---
    offset_t new_child_offset = writeNode(child);
    offset_t new_sibling_offset = writeNode(sibling);
    node->children[idx] = new_child_offset;
    node->children[idx + 1] = new_sibling_offset;

    file.freeBlock(node->children[idx]);
    file.freeBlock(node->children[idx + 1]);

    return writeNode(node);
}

offset_t BTree::merge(std::shared_ptr<Node> node, offset_t node_offset, int idx) {
    auto child = readNode(node->children[idx]);
    auto sibling = readNode(node->children[idx + 1]);

    // Pull down key from parent
    child->keys[metadata.t - 1] = node->keys[idx];
    child->values[metadata.t - 1] = node->values[idx];

    // Copy keys/values from sibling to child
    for (int i = 0; i < sibling->n; ++i) {
        child->keys[i + metadata.t] = sibling->keys[i];
        child->values[i + metadata.t] = sibling->values[i];
    }
    if (!child->is_leaf) {
        for (int i = 0; i <= sibling->n; ++i) {
            child->children[i + metadata.t] = sibling->children[i];
        }
    }

    // Update parent: shift keys/children left
    for (int i = idx + 1; i < node->n; ++i) {
        node->keys[i - 1] = node->keys[i];
        node->values[i - 1] = node->values[i];
    }
    for (int i = idx + 2; i <= node->n; ++i) {
        node->children[i - 1] = node->children[i];
    }

    child->n += sibling->n + 1;
    node->n--;

    // --- Copy-on-Write ---
    offset_t new_child_offset = writeNode(child);
    node->children[idx] = new_child_offset;

    file.freeBlock(node->children[idx]); // Free old child
    file.freeBlock(node->children[idx+1]); // Free old sibling

    return writeNode(node);
}


// --- BTree Private: Range Search & Print ---

void BTree::rangeSearchRecursive(offset_t node_offset, const std::string& min_k, const std::string& max_k, std::vector<KeyValuePair>& results) {
    if (node_offset == 0) return;

    auto node = readNode(node_offset);
    int i = 0;
    for (i = 0; i < node->n; i++) {
        // 1. If not leaf, traverse subtree before key[i]
        if (!node->is_leaf) {
            rangeSearchRecursive(node->children[i], min_k, max_k, results);
        }

        // 2. Check if key[i] is in range
        if (node->keys[i] >= min_k && node->keys[i] <= max_k) {
            results.push_back({node->keys[i], node->values[i]});
        }

        // 3. If key[i] > max_k, we can stop
        if (node->keys[i] > max_k) {
            return;
        }
    }

    // 4. Traverse the last child (subtree after key[n-1])
    if (!node->is_leaf) {
        rangeSearchRecursive(node->children[i], min_k, max_k, results);
    }
}

void BTree::printRecursive(offset_t node_offset, int level) {
    if (node_offset == 0) return;

    auto node = readNode(node_offset);
    std::string indent(level * 4, ' ');

    std::cout << indent << "[Node " << node_offset << "] (n=" << node->n << ") "
              << (node->is_leaf ? "(LEAF)" : "") << std::endl;

    int i;
    for (i = 0; i < node->n; i++) {
        if (!node->is_leaf) {
            printRecursive(node->children[i], level + 1);
        }

        std::cout << indent << "  - Key: ";
        if (metadata.key_type == KeyType::INTEGER) {
            std::cout << Node::stringKeyToInt(node->keys[i]);
        } else {
            std::cout << "'" << node->keys[i] << "'";
        }
        std::cout << ", Val: " << node->values[i] << std::endl;
    }

    if (!node->is_leaf) {
        printRecursive(node->children[i], level + 1);
    }
}
