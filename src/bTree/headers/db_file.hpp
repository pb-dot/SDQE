#pragma once

#include "db_constants.hpp"
#include <fstream>
#include <string>
#include <mutex>
#include <vector> // --- NEW: For pending free list ---

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
    // This is now safe from "reuse-after-free" bugs.
    offset_t allocateBlock();

    // --- MODIFIED: freeBlock now just stages blocks for collection ---
    // This is a lightweight, in-memory-only operation.
    void freeBlock(offset_t offset);

    // --- Metadata Access ---
    MetadataHeader getMetadata() const;

    // --- MODIFIED: This is now the two-phase commit function ---
    // 1. Commits the new root (Data Commit).
    // 2. Commits the garbage collection (Free List Commit).
    bool commitRootAndGarbageCollect(offset_t new_root_offset);

private:
    // Calculate the max 't' that fits in a block
    uint32_t calculateT(KeyType key_type);

    // --- NEW: Separate commit for free list ---
    // This is Phase 2 of the commit.
    bool commitFreeList();

    // Read/Write metadata block (Block 0)
    bool readHeader();
    bool writeHeader();

    std::fstream file;       // File stream for I/O
    std::string filename;    // Name of the database file
    MetadataHeader header;   // In-memory copy of the *committed* metadata

    // --- NEW: In-memory list of blocks to be freed ---
    // Blocks are added here during a transaction, and processed
    // *after* the root commit (Phase 1) is successful.
    std::vector<offset_t> pending_free_list;
};
