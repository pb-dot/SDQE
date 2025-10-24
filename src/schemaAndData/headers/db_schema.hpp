#pragma once

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <cstdint>
#include <optional>
#include "db_btree.hpp"

// --- Constants ---
extern const size_t DB_INT_SIZE;
extern const size_t DB_STRING_SIZE;

// --- Data Types ---
enum class DataType {
    INT,
    STRING
};

// --- Column Definition ---
struct Column {
    std::string name;
    DataType type;
    size_t size;

    Column(std::string n, DataType t);
};

// --- Schema Class ---
/**
 * @class Schema
 * @brief Manages the table's metadata (schema).
 */
class Schema {
public:
    std::vector<Column> columns;
    std::string index_column_name;
    int64_t next_auto_increment_id = 1;

    // --- Public API ---

    /**
     * @brief Loads schema definition from a .schema file.
     */
    bool load(const std::string& filepath);

    /**
     * @brief Saves the current schema definition to a .schema file.
     */
    bool save(const std::string& filepath) const;

    /**
     * @brief Calculates the total fixed size of one record in bytes.
     */
    size_t get_record_size() const;

    /**
     * @brief Gets the byte offset of a column within a record.
     */
    size_t get_column_offset(const std::string& col_name) const;

    /**
     * @brief Gets the Column struct for the index.
     */
    Column get_index_column() const;

    /**
     * @brief Gets the B-Tree KeyType (INTEGER/STRING) for the index column.
     */
    KeyType get_index_key_type() const;

    /**
     * @brief Gets the Column struct for a given column name.
     */
    std::optional<Column> get_column(const std::string& col_name) const;
};
