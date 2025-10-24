#include "db_btree.hpp"
#include <iostream>
#include <vector>

void testIntTree() {
    const std::string filename = "int_tree.db";
    BTree tree;

    // --- Create ---
    std::cout << "--- Creating Integer B-Tree ---" << std::endl;
    if (!tree.create(filename, KeyType::INTEGER)) {
        std::cout << "Creation failed. Opening existing file." << std::endl;
        if (!tree.open(filename)) {
            std::cerr << "Failed to open file." << std::endl;
            return;
        }
    }

    // --- Insert ---
    std::cout << "\n--- Inserting Keys ---" << std::endl;
    std::vector<int> keys = {10, 20, 5, 6, 12, 30, 7, 17, 15, 25, 35, 40, 50, 1, 18};
    for (int k : keys) {
        std::cout << "Inserting " << k << std::endl;
        tree.insert(k, k * 10);
    }

    std::cout << "\n--- Tree Structure (Integers) ---" << std::endl;
    tree.print();

    // --- Search ---
    std::cout << "\n--- Point Search ---" << std::endl;
    int key_to_find = 17;
    auto val = tree.search(key_to_find);
    if (val) {
        std::cout << "Found key " << key_to_find << ", value = " << *val << std::endl;
    } else {
        std::cout << "Key " << key_to_find << " not found." << std::endl;
    }

    key_to_find = 99;
    val = tree.search(key_to_find);
    if (val) {
        std::cout << "Found key " << key_to_find << ", value = " << *val << std::endl;
    } else {
        std::cout << "Key " << key_to_find << " not found." << std::endl;
    }

    // --- Update ---
    std::cout << "\n--- Update ---" << std::endl;
    int key_to_update = 17;
    int new_val = 177;
    std::cout << "Updating key " << key_to_update << " to value " << new_val << std::endl;
    tree.update(key_to_update, new_val);
    val = tree.search(key_to_update);
    if (val) {
        std::cout << "Found key " << key_to_update << ", new value = " << *val << std::endl;
    }

    // --- Range Search ---
    std::cout << "\n--- Range Search [6, 25] ---" << std::endl;
    auto results = tree.rangeSearch(6, 25);
    for (const auto& pair : results) {
        std::cout << "(" << pair.first << ", " << pair.second << ") ";
    }
    std::cout << std::endl;

    // --- Delete ---
    std::cout << "\n--- Deleting Keys ---" << std::endl;
    std::cout << "Deleting 6 (leaf deletion)..." << std::endl;
    tree.remove(6);
    tree.print();

    std::cout << "\nDeleting 15 (internal merge/borrow)..." << std::endl;
    tree.remove(15);
    tree.print();

    std::cout << "\nDeleting 10 (root)..." << std::endl;
    tree.remove(10);
    tree.print();

    tree.close();

    // --- Re-open and test persistence ---
    std::cout << "\n--- Re-opening file to test persistence ---" << std::endl;
    BTree tree2;
    if (!tree2.open(filename)) {
        std::cerr << "Failed to re-open file." << std::endl;
        return;
    }

    std::cout << "Tree structure after re-opening:" << std::endl;
    tree2.print();

    val = tree2.search(17);
    std::cout << "Searching for 17... Value: " << (val ? std::to_string(*val) : "Not Found") << std::endl;
    val = tree2.search(6);
    std::cout << "Searching for 6 (deleted)... Value: " << (val ? std::to_string(*val) : "Not Found") << std::endl;

    tree2.close();
}

void testStringTree() {
    const std::string filename = "string_tree.db";
    BTree tree;

    // --- Create ---
    std::cout << "\n\n--- Creating String B-Tree ---" << std::endl;
    if (!tree.create(filename, KeyType::STRING)) {
        std::cout << "Creation failed. Opening existing file." << std::endl;
        if (!tree.open(filename)) {
            std::cerr << "Failed to open file." << std::endl;
            return;
        }
    }

    std::vector<std::string> keys = {"apple", "banana", "mango", "grape", "cherry", "date", "fig"};
    int val = 1;
    for (const auto& k : keys) {
        std::cout << "Inserting '" << k << "'" << std::endl;
        tree.insert(k, val++);
    }

    std::cout << "\n--- Tree Structure (Strings) ---" << std::endl;
    tree.print();

    std::cout << "\n--- Range Search ['banana', 'grape'] ---" << std::endl;
    auto results = tree.rangeSearch("banana", "grape");
    for (const auto& pair : results) {
        std::cout << "('" << pair.first << "', " << pair.second << ") ";
    }
    std::cout << std::endl;

    tree.close();
}

int main() {
    testIntTree();
    testStringTree();
    return 0;
}
