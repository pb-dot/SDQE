#pragma once

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <iostream>
#include "db_schema.hpp"

// A Value can be one of the types our database supports.
using Value = std::variant<int64_t, std::string>;

/**
 * @class Record
 * @brief Represents a single row of data in memory.
 */
class Record {
public:
    // Holds the data for this record, e.g., {"id" -> 10, "name" -> "Alice"}
    std::map<std::string, Value> values;

    /**
     * @brief Serializes the in-memory record data into a fixed-length byte buffer.
     */
    std::vector<char> serialize(const Schema& schema) const;

    /**
     * @brief Deserializes a fixed-length byte buffer into this record's map.
     */
    void deserialize(const char* buffer, const Schema& schema);

    /**
     * @brief Helper to print the record's content.
     */
    void print(const Schema& schema) const;

    /**
     * @brief Helper to extract the index key from the record.
     */
    Value get_index_value(const Schema& schema) const;
};
