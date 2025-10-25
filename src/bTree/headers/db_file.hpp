#pragma once

#include "db_constants.hpp"
#include <fstream>
#include <string>
#include <mutex>
#include <vector>

// --- Metadata Header (Block 0) ---
// This struct represents the layout of the first block in the file.
struct MetadataHeader {
    char magic[16];          // Magic number to identify file type
    KeyType key_type;        // Type of keys stored in this tree (int/string)
    uint32_t t;              // [Each Node has 2t child 2t-1 max keys]
    offset_t root_offset;    // File offset of the root node
    offset_t free_list_head; // File offset of the first free block
    uint64_t block_count;    // Total number of blocks in the file
    //block_count is an optimization that lets us instantly calculate the offset for a new block, avoiding a slow "seek-to-end" system call.

    // Constructor to initialize default values
    MetadataHeader()
        : key_type(KeyType::INTEGER), t(0), root_offset(0), free_list_head(0), block_count(1) // Starts with 1 (itself)
    {
        std::fill(magic, magic + 16, 0);
        MAGIC_NUMBER.copy(magic, MAGIC_NUMBER.length());
    }
};

// --- DbFile Class ---
// Manages the physical database file, block allocation, and metadata.
class DbFile {
public:
    DbFile();
    ~DbFile();

    // --- File Lifecycle ---
    bool create(const std::string& filename, KeyType key_type, uint32_t& t_out);
    bool open(const std::string& filename);
    void close();
    bool isOpen() const;

    // --- Block I/O ---
    bool readBlock(offset_t offset, char* buffer);
    bool writeBlock(offset_t offset, const char* buffer);

    // --- Block Allocation ---
    // Get a new, unused block from the *committed* free list.
    offset_t allocateBlock();

    // --- freeBlock  just marks stale blocks as to be freed ---
    void freeBlock(offset_t offset);

    // --- Metadata Access ---
    MetadataHeader getMetadata() const;

    //  This is the two-phase commit function ---
    // Phase 1. Commits the new root (Data Commit).
    // Phase 2. Commits the garbage collection (Free List Commit).
    bool commitRootAndGarbageCollect(offset_t new_root_offset);

private:
    // Calculate the max 't' that fits in a block
    uint32_t calculateT(KeyType key_type);

    // This is Phase 2 of the commit.
    bool commitFreeList();

    // Read/Write metadata block (Block 0)
    bool readHeader();
    bool writeHeader();

    std::fstream file;       // File stream for I/O
    std::string filename;    // Name of the database file
    MetadataHeader header;   // In-memory copy of the *committed* metadata

    // In-memory list of blocks to be freed ---
    // Blocks are added here during a transaction, and processed
    // *after* the root commit (Phase 1) is successful.
    std::vector<offset_t> pending_free_list;
};


/*
Calculating t : uint32_t calculateT(KeyType key_type);

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
