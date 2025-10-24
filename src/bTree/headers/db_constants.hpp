#pragma once

#include <cstdint>
#include <string>

// Use a fixed 4KB block size, standard for most OS/DB pages
constexpr size_t BLOCK_SIZE = 4096; //4096 bytes

// Maximum size for string keys
constexpr size_t MAX_STRING_KEY_SIZE = 255; //255 bytes

// Magic number to identify our B-Tree files
const std::string MAGIC_NUMBER = "V_BTR_v1";

// Type alias for file offsets
using offset_t = int64_t;

// Enum for key types
enum class KeyType : uint8_t {
    INTEGER = 0x00,
    STRING = 0x01
};
