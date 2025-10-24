#include "db_file.hpp"
#include "db_node.hpp"
#include <iostream>
#include <cstring> // For memcpy


DbFile::DbFile() {}

DbFile::~DbFile() {
    close();
}

bool DbFile::create(const std::string& filename, KeyType key_type, uint32_t& t_out) {
    std::lock_guard<std::mutex> lock(file_mutex);
    if (file.is_open()) {
        return false; // Already open
    }

    // Check if file already exists
    file.open(filename, std::ios::in | std::ios::binary);
    if (file.is_open()) {
        file.close();
        return false; // File exists
    }

    // Create new file
    file.open(filename, std::ios::out | std::ios::in | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false; // Creation failed
    }

    this->filename = filename;

    // Initialize header
    header = MetadataHeader(); // Reset to defaults
    header.key_type = key_type;
    header.t = calculateT(key_type);
    t_out = header.t;

    // Write the empty metadata block to disk
    if (!writeHeader()) {
        file.close();
        return false;
    }

    return true;
}

bool DbFile::open(const std::string& filename) {
    std::lock_guard<std::mutex> lock(file_mutex);
    if (file.is_open()) {
        return false;
    }

    file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        return false; // Failed to open
    }

    this->filename = filename;

    // Read and validate metadata header
    if (!readHeader()) {
        file.close();
        return false;
    }

    if (MAGIC_NUMBER.compare(header.magic) != 0) {
        std::cerr << "Error: Not a valid B-Tree file (magic number mismatch)." << std::endl;
        file.close();
        return false;
    }

    return true;
}

void DbFile::close() {
    std::lock_guard<std::mutex> lock(file_mutex);
    if (file.is_open()) {
        writeHeader(); // Sync metadata on close
        file.flush();
        file.close();
        filename = "";
    }
}

bool DbFile::isOpen() const {
    return file.is_open();
}

bool DbFile::readBlock(offset_t offset, char* buffer) {
    std::lock_guard<std::mutex> lock(file_mutex);
    if (!file.is_open()) return false;

    file.seekg(offset);
    file.read(buffer, BLOCK_SIZE);

    if (file.gcount() != BLOCK_SIZE) {
        // Handle read error or short read (e.g., end of file)
        std::cerr << "Error: Failed to read block at offset " << offset << std::endl;
        return false;
    }
    return true;
}

bool DbFile::writeBlock(offset_t offset, const char* buffer) {
    std::lock_guard<std::mutex> lock(file_mutex);
    if (!file.is_open()) return false;

    file.seekp(offset);
    file.write(buffer, BLOCK_SIZE);
    return file.good();
}

offset_t DbFile::allocateBlock() {
    std::lock_guard<std::mutex> lock(file_mutex);
    if (!file.is_open()) return 0;

    offset_t new_offset = 0;

    if (header.free_list_head != 0) {
        // Get block from free list
        new_offset = header.free_list_head;

        // Read the free block to find the *next* free block
        char buffer[BLOCK_SIZE];
        file.seekg(new_offset);
        file.read(buffer, BLOCK_SIZE);

        // The first 8 bytes of a free block store the offset of the next one
        memcpy(&header.free_list_head, buffer, sizeof(offset_t));

        // We don't need to write the header here, it will be synced
        // on the next commit.
    } else {
        // No free blocks, extend the file
        new_offset = header.block_count * BLOCK_SIZE;
        header.block_count++;

        // We must write an empty block to physically extend the file
        char empty_buffer[BLOCK_SIZE] = {0};
        file.seekp(new_offset);
        file.write(empty_buffer, BLOCK_SIZE);
    }

    return new_offset;
}

void DbFile::freeBlock(offset_t offset) {
    std::lock_guard<std::mutex> lock(file_mutex);
    if (!file.is_open() || offset == 0) return;

    // Write the current free list head to the start of this block
    char buffer[BLOCK_SIZE] = {0};
    memcpy(buffer, &header.free_list_head, sizeof(offset_t));

    file.seekp(offset);
    file.write(buffer, BLOCK_SIZE);

    // This block is now the new head
    header.free_list_head = offset;

    // Header will be synced on next commit or close.
}

MetadataHeader DbFile::getMetadata() const {
    return header;
}

bool DbFile::commitRootOffset(offset_t new_root_offset) {
    std::lock_guard<std::mutex> lock(file_mutex);
    if (!file.is_open()) return false;

    // 1. Update in-memory header
    header.root_offset = new_root_offset;

    // 2. Write the entire header to disk
    if (!writeHeader()) {
        return false;
    }

    // 3. Force sync to disk
    file.flush();
    return file.good();
}

uint32_t DbFile::calculateT(KeyType key_type) {
    // 16 bytes for NodeHeader
    int available_space = BLOCK_SIZE - sizeof(NodeHeader);

    // We need to solve for max_keys (which is 2t-1)
    // Size = (max_keys * key_size) + (max_keys * val_size) + (max_children * child_ptr_size)
    // Size = (max_keys * key_size) + (max_keys * 4) + ((max_keys + 1) * 8)
    // Size = (max_keys * key_size) + (max_keys * 4) + (max_keys * 8) + 8
    // Size = (max_keys * (key_size + 12)) + 8
    // available_space - 8 = max_keys * (key_size + 12)
    // max_keys = (available_space - 8) / (key_size + 12)

    int key_size = 0;
    if (key_type == KeyType::INTEGER) {
        key_size = sizeof(int32_t); // 4
    } else {
        key_size = MAX_STRING_KEY_SIZE; // 255
    }

    int max_keys = (available_space - 8) / (key_size + 12);

    // max_keys must be odd (2t-1), so find largest odd num <= max_keys
    if (max_keys % 2 == 0) {
        max_keys--;
    }

    // 2t - 1 = max_keys  =>  t = (max_keys + 1) / 2
    return (max_keys + 1) / 2;
}

bool DbFile::readHeader() {
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&header), sizeof(MetadataHeader));
    // Ensure we read *at least* this much
    return file.gcount() >= sizeof(MetadataHeader);
}

bool DbFile::writeHeader() {
    file.seekp(0);
    file.write(reinterpret_cast<const char*>(&header), sizeof(MetadataHeader));
    return file.good();
}
