#pragma once

#include "db_constants.hpp"
#include <fstream>
#include <string>
#include <mutex>

// --- Metadata Header (Block 0) ---
// This struct represents the layout of the first block in the file table.idx
struct MetadataHeader {
    char magic[16];          // Magic number to identify file type
    KeyType key_type;        // Type of keys stored in this tree
    uint32_t t;              // Minimum degree of the B-Tree
    offset_t root_offset;    // File offset of the root node
    offset_t free_list_head; // File offset of the first free block
    uint64_t block_count;    // Total number of blocks in the file

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

    // Create a new B-Tree file. Fails if file exists.
    // Calculates and returns 't' based on key type.
    bool create(const std::string& filename, KeyType key_type, uint32_t& t_out);

    // Open an existing B-Tree file. Fails if file doesn't exist.
    bool open(const std::string& filename);

    // Close the file
    void close();

    // Check if a file is currently open
    bool isOpen() const;

    // --- Block I/O ---

    // Read a block from a specific offset
    bool readBlock(offset_t offset, char* buffer);

    // Write a block to a specific offset
    bool writeBlock(offset_t offset, const char* buffer);

    // --- Block Allocation ---

    // Get a new, unused block. Either from free list or by extending the file.
    offset_t allocateBlock();

    // Add a block (and its subsequent chain) to the free list
    void freeBlock(offset_t offset);

    // --- Metadata Access ---

    // Get a copy of the current metadata
    MetadataHeader getMetadata() const;

    // Atomically update the root node offset and sync to disk.
    // This is the "commit" operation for shadow paging.
    bool commitRootOffset(offset_t new_root_offset);

private:
    // Calculate the max 't' that fits in a block
    uint32_t calculateT(KeyType key_type);

    // Read/Write metadata block (Block 0)
    bool readHeader();
    bool writeHeader();

    std::fstream file;       // File stream for I/O
    std::string filename;    // Name of the database file
    MetadataHeader header;   // In-memory copy of the metadata
    std::mutex file_mutex;   // Mutex to protect file access
};
