#include "db_schema.hpp"
#include <fstream>
#include <sstream>

// --- Constants Definition ---
const size_t DB_INT_SIZE = 8;
const size_t DB_STRING_SIZE = 255;

// --- Column Implementation ---
Column::Column(std::string n, DataType t) : name(std::move(n)), type(t) {
    size = (type == DataType::INT) ? DB_INT_SIZE : DB_STRING_SIZE;
}

// --- Schema Implementation ---

bool Schema::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    columns.clear();
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string part;
        std::vector<std::string> parts;

        while (std::getline(ss, part, ':')) {
            parts.push_back(part);
        }

        if (parts.empty()) continue;

        if (parts[0] == "COL" && parts.size() == 3) {
            DataType type = (parts[2] == "INT") ? DataType::INT : DataType::STRING;
            columns.emplace_back(parts[1], type);
        } else if (parts[0] == "INDEX" && parts.size() == 2) {
            index_column_name = parts[1];
        } else if (parts[0] == "NEXT_ID" && parts.size() == 2) {
            try {
                next_auto_increment_id = std::stoll(parts[1]);
            } catch (...) {
                // Handle parse error if necessary
                next_auto_increment_id = 1;
            }
        }
    }
    file.close();
    return true;
}

bool Schema::save(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    for (const auto& col : columns) {
        file << "COL:" << col.name << ":" << (col.type == DataType::INT ? "INT" : "STR") << "\n";
    }

    if (!index_column_name.empty()) {
        file << "INDEX:" << index_column_name << "\n";
    }

    file << "NEXT_ID:" << next_auto_increment_id << "\n";

    file.close();
    return !file.fail();
}

size_t Schema::get_record_size() const {
    size_t total_size = 0;
    for (const auto& col : columns) {
        total_size += col.size;
    }
    return total_size;
}

size_t Schema::get_column_offset(const std::string& col_name) const {
    size_t current_offset = 0;
    for (const auto& col : columns) {
        if (col.name == col_name) {
            return current_offset;
        }
        current_offset += col.size;
    }
    throw std::runtime_error("Column not found in schema: " + col_name);
}

Column Schema::get_index_column() const {
    for (const auto& col : columns) {
        if (col.name == index_column_name) {
            return col;
        }
    }
    throw std::runtime_error("Index column not found in schema.");
}

KeyType Schema::get_index_key_type() const {
    return (get_index_column().type == DataType::INT) ? KeyType::INTEGER : KeyType::STRING;
}

std::optional<Column> Schema::get_column(const std::string& col_name) const {
    for (const auto& col : columns) {
        if (col.name == col_name) {
            return col;
        }
    }
    return std::nullopt; // Return an empty optional if not found
}
