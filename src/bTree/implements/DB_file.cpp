#include "db_file.hpp"
#include "db_node.hpp" // For sizeof(NodeHeader)
#include <iostream>
#include <cstring> // For memcpy
#include <cstdio> // For std::remove

DbFile::DbFile() {}

DbFile::~DbFile() {
    close();
}

bool DbFile::create(const std::string& filename, KeyType key_type, uint32_t& t_out) {

    if (file.is_open()) return false;

    // Check if file already exists
    file.open(filename, std::ios::in | std::ios::binary);
    if (file.is_open()) {
        file.close();
        return false; // File exists
    }

    // Create new file
    file.open(filename, std::ios::out | std::ios::in | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false; // Creation failed

    this->filename = filename;

    // Initialize header
    header = MetadataHeader(); // Reset to defaults
    header.key_type = key_type;
    header.t = calculateT(key_type);


    if (header.t == 0) {
        std::cerr << "Error: BLOCK_SIZE " << BLOCK_SIZE
                  << " is too small to create a B-Tree with this key type." << std::endl;
        file.close();
        // Clean up the invalid file
        std::remove(filename.c_str());
        return false;
    }


    t_out = header.t;

    // Write the empty metadata block to disk
    if (!writeHeader()) {
        file.close();
        std::remove(filename.c_str());
        return false;
    }

    return true;
}

bool DbFile::open(const std::string& filename) {

    if (file.is_open()) return false;

    file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) return false;

    this->filename = filename;
    if (!readHeader()) {
        file.close();
        return false;
    }
    if (MAGIC_NUMBER.compare(header.magic) != 0) {
        std::cerr << "Error: Not a valid B-Tree file (magic number mismatch)." << std::endl;
        file.close();
        return false;
    }
    if (header.t < 2) {
         std::cerr << "Error: File is corrupt or has invalid B-Tree degree (t="
                   << header.t << ")." << std::endl;
        file.close();
        return false;
    }
    // On open, clear any pending blocks from a previous (failed) run
    pending_free_list.clear();
    return true;
}

void DbFile::close() {

    if (file.is_open()) {
        // Try to commit any pending free blocks on close ---
        // This is a "best effort" to clean up any leaks from a
        // previous crash *after* Phase 1 but *before* Phase 2.
        if (!pending_free_list.empty()) {
            commitFreeList();
        }

        // writeHeader() is not strictly needed here since commits do it,
        // but it's safe and ensures the final state is synced.
        writeHeader();
        file.flush();
        file.close();
        filename = "";
    }
}

bool DbFile::isOpen() const {
    return file.is_open();
}

bool DbFile::readBlock(offset_t offset, char* buffer) {

    if (!file.is_open()) return false;
    file.seekg(offset);
    file.read(buffer, BLOCK_SIZE);
    if (!file || file.gcount() != BLOCK_SIZE) {
        std::cerr << "Error: Failed to read block at offset " << offset << std::endl;
        file.clear();
        return false;
    }
    return true;
}

bool DbFile::writeBlock(offset_t offset, const char* buffer) {

    if (!file.is_open()) return false;
    file.seekp(offset);
    file.write(buffer, BLOCK_SIZE);
    return file.good();
}

offset_t DbFile::allocateBlock() {
    // This function is called by BTree methods. We assume the BTree's
    // public methods (insert, remove, etc.) are the transaction
    // boundary and are not called concurrently.
    // For a fully concurrent DB, this class would need better locking.

    if (!file.is_open()) return 0;

    offset_t new_offset = 0;

    // It *only* reads from the committed free list.
    // It can *never* see a block from the pending_free_list,
    // solving the "reuse-after-free-in-same-transaction" bug.
    if (header.free_list_head != 0) {
        new_offset = header.free_list_head;

        char buffer[BLOCK_SIZE];

        if (!readBlock(new_offset, buffer)) {
             std::cerr << "CRITICAL: Failed to read from free list block " << new_offset << std::endl;
             // This is bad. Fallback to extending the file.
             new_offset = 0;
             header.free_list_head = 0; // Mark list as corrupt/empty
        } else {
            // Read the "next" pointer from the block
            memcpy(&header.free_list_head, buffer, sizeof(offset_t));
        }

        // We *do not* write the header here. This change to
        // header.free_list_head is *in-memory only* for this
        // transaction. It will be committed by commitRootAndGarbageCollect.
    }

    // Fallback: If list was empty or read failed
    if (new_offset == 0) {
        // No free blocks, extend the file
        new_offset = header.block_count * BLOCK_SIZE;
        header.block_count++; // This is also an in-memory-only change

        // We must write an empty block to physically extend the file
        char empty_buffer[BLOCK_SIZE] = {0};
        if (!writeBlock(new_offset, empty_buffer)) {
            std::cerr << "CRITICAL: Failed to extend database file." << std::endl;
            header.block_count--; // Roll back
            return 0;
        }
    }

    return new_offset;
}

// This is a lightweight, in-memory-only operation ---
void DbFile::freeBlock(offset_t offset) {
    if (offset == 0) return;
    // Just add the block to the in-memory pending list.
    // We will process this list *after* the next successful root commit.
    // This *solves* the "reuse-after-free-in-same-transaction" bug.
    pending_free_list.push_back(offset);
}

MetadataHeader DbFile::getMetadata() const {
    return header;
}

// --- The two-phase commit logic ---
bool DbFile::commitRootAndGarbageCollect(offset_t new_root_offset) {
    // This is the *only* function that should write the metadata header.
    // It acts as the "commit" point for the transaction.

    if (!file.is_open()) return false;

    // --- PHASE 1: Commit Data (The Root Flip) ---
    // This is the atomic data commit.
    // We commit the new root offset, *along with* any changes to
    // block_count and free_list_head from allocateBlock().
    header.root_offset = new_root_offset;
    if (!writeHeader()) {
        std::cerr << "CRITICAL: Failed to commit new root offset." << std::endl;
        // At this point, the in-memory state is out of sync.
        // A real DB would need to roll back. We'll just fail.
        return false;
    }
    file.flush(); // Force Block 0 to disk.

    // --- CRASH-POINT 1 ---
    // If we crash here:
    // 1. The new root is live and safe.
    // 2. The blocks we just allocated are correctly used.
    // 3. The old blocks (in pending_free_list) are now "leaked" (orphaned).
    // This is SAFE. No corruption, just a block leak.

    // --- PHASE 2: Commit Garbage Collection ---
    // Now that data is safe, we can *safely* modify the free list
    // by adding the blocks from the pending_free_list.
    if (!commitFreeList()) {
        // This is non-fatal. It just means the blocks will be
        // leaked until the *next* successful transaction.
        std::cerr << "Warning: Failed to commit free list. Blocks will be leaked." << std::endl;
    }

    // --- CRASH-POINT 2 ---
    // If we crash *during* commitFreeList (after it wrote to some
    // blocks but before it wrote the new header):
    // 1. The data is SAFE (from Phase 1).
    // 2. On restart, we'll read the *old* free_list_head.
    // 3. The blocks in pending_free_list are *still* leaked.
    // This is SAFE. No corruption.

    return true;
}

// --- This function *safely* adds pending blocks to the free list ---
bool DbFile::commitFreeList() {
    // This function must be called *after* the data commit.
    // It assumes it is already inside the file_mutex lock.
    if (pending_free_list.empty()) {
        return true; // Nothing to do
    }

    try {
        // We use the *current* in-memory header.free_list_head,
        // which was set by allocateBlock() during the transaction.
        offset_t current_head = header.free_list_head;
        char buffer[BLOCK_SIZE] = {0};

        for (offset_t offset : pending_free_list) {
            // Write the "next" pointer into the block
            memcpy(buffer, &current_head, sizeof(offset_t));
            if (!writeBlock(offset, buffer)) {
                std::cerr << "Warning: Failed to write to free block " << offset << std::endl;
                // Don't stop, just log and continue. We'll leak this block.
            } else {
                // Only update the head if the write was successful
                current_head = offset;
            }
        }

        // Now, commit the *new* head of the free list
        header.free_list_head = current_head;
        if (!writeHeader()) {
            std::cerr << "CRITICAL: Failed to write new free_list_head." << std::endl;
            return false;
        }
        file.flush(); // Force the header (and any buffered block writes)

        // All blocks are now safely in the free list. Clear the pending list.
        pending_free_list.clear();
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error during garbage collection: " << e.what() << std::endl;
        return false;
    }
}


uint32_t DbFile::calculateT(KeyType key_type) {
    // 16 bytes for NodeHeader
    int available_space = BLOCK_SIZE - sizeof(NodeHeader);

    int key_size = 0;
    if (key_type == KeyType::INTEGER) {
        key_size = sizeof(int32_t); // 4
    } else {
        key_size = MAX_STRING_KEY_SIZE; // 255
    }

    // Ensure available space is positive
    if (available_space <= 8) return 0; // Not enough space

    int max_keys = (available_space - 8) / (key_size + 12);

    // A B-Tree must have t >= 2, which means max_keys = 2t-1 >= 3.
    if (max_keys < 3) {
        return 0; // Signal failure: block size is too small
    }

    if (max_keys % 2 == 0) {
        max_keys--;
    }

    return (max_keys + 1) / 2;
}

bool DbFile::readHeader() {
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&header), sizeof(MetadataHeader));
    // Check if read failed or didn't read enough bytes
    if (!file || file.gcount() < sizeof(MetadataHeader)) {
        std::cerr << "Error: Failed to read full metadata header." << std::endl;
        return false;
    }
    return true;
}

bool DbFile::writeHeader() {
    file.seekp(0);
    file.write(reinterpret_cast<const char*>(&header), sizeof(MetadataHeader));
    return file.good();
}
