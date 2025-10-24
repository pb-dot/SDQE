#include "db_record.hpp"
#include <cstring> // For memcpy
#include <algorithm> // For std::min

std::vector<char> Record::serialize(const Schema& schema) const {
    size_t record_size = schema.get_record_size();
    std::vector<char> buffer(record_size, '\0'); // Initialize with nulls (padding)

    size_t current_offset = 0;
    for (const auto& col : schema.columns) {
        auto it = values.find(col.name);
        if (it == values.end()) {
            current_offset += col.size;
            continue;
        }

        const Value& val = it->second;

        if (col.type == DataType::INT && std::holds_alternative<int64_t>(val)) {
            int64_t int_val = std::get<int64_t>(val);
            std::memcpy(&buffer[current_offset], &int_val, DB_INT_SIZE);
        }
        else if (col.type == DataType::STRING && std::holds_alternative<std::string>(val)) {
            const std::string& str_val = std::get<std::string>(val);
            std::memcpy(&buffer[current_offset], str_val.c_str(),
                        std::min(str_val.length(), DB_STRING_SIZE));
        }
        current_offset += col.size;
    }
    return buffer;
}

void Record::deserialize(const char* buffer, const Schema& schema) {
    values.clear();
    size_t current_offset = 0;

    for (const auto& col : schema.columns) {
        if (col.type == DataType::INT) {
            int64_t int_val;
            std::memcpy(&int_val, &buffer[current_offset], DB_INT_SIZE);
            values[col.name] = int_val;
        }
        else if (col.type == DataType::STRING) {
            // Create string from buffer, respect first null terminator
            std::string str_val(buffer + current_offset, DB_STRING_SIZE);
            values[col.name] = std::string(str_val.c_str());
        }
        current_offset += col.size;
    }
}

void Record::print(const Schema& schema) const {
    std::cout << "{ ";
    bool first = true;
    for (const auto& col : schema.columns) {
        auto it = values.find(col.name);
        if (it != values.end()) {
            if (!first) {
                std::cout << ", ";
            }
            std::cout << col.name << ": ";
            if (col.type == DataType::INT) {
                std::cout << std::get<int64_t>(it->second);
            } else {
                std::cout << "'" << std::get<std::string>(it->second) << "'";
            }
            first = false;
        }
    }
    std::cout << " }" << std::endl;
}

Value Record::get_index_value(const Schema& schema) const {
    auto it = values.find(schema.index_column_name);
    if (it == values.end()) {
        throw std::runtime_error("Index value not found in record.");
    }
    return it->second;
}
