#include "db_table.hpp"
#include <iostream>
#include <vector>
#include <limits> // For std::numeric_limits

Table::Table(const std::string& schema_path, const std::string& index_path,
             const std::string& data_path)
    : m_schema_path(schema_path),
      m_index_path(index_path),
      m_data_path(data_path),
      record_size(0),
      m_is_open(false) {}

Table::~Table() {
    close();
}

bool Table::create(const Schema& schema_to_create) {
    if (m_is_open) return false;
    if (!schema_to_create.save(m_schema_path)) return false;

    BTree temp_tree;
    KeyType index_type = schema_to_create.get_index_key_type();
    if (!temp_tree.create(m_index_path, index_type)) return false;
    temp_tree.close();

    std::ofstream data_file_stream(m_data_path, std::ios::binary);
    if (!data_file_stream) return false;
    data_file_stream.close();

    return true;
}

bool Table::open() {
    if (m_is_open) return true;

    if (!schema.load(m_schema_path)) return false;
    record_size = schema.get_record_size();
    if (record_size == 0) return false;

    if (!btree.open(m_index_path)) return false;

    data_file.open(m_data_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!data_file.is_open()) {
        btree.close();
        return false;
    }

    m_is_open = true;
    return true;
}

void Table::close() {
    if (!m_is_open) return;
    btree.close();
    if (data_file.is_open()) data_file.close();
    m_is_open = false;
}

// --- Private Helper Methods (Corrected) ---

std::unique_ptr<Record> Table::read_record_at_offset(int32_t offset) {
    if (!m_is_open || offset < 0) return nullptr;

    // Cast 32-bit offset to 64-bit streamoff for file I/O
    data_file.seekg(static_cast<std::streamoff>(offset));
    if (data_file.fail()) {
        std::cerr << "Error: Failed to seek in data file to offset " << offset << std::endl;
        return nullptr;
    }

    std::vector<char> buffer(record_size);
    data_file.read(buffer.data(), record_size);
    if (data_file.fail() || data_file.gcount() != record_size) {
        return nullptr;
    }

    auto record = std::make_unique<Record>();
    record->deserialize(buffer.data(), schema);
    return record;
}

bool Table::write_record_at_offset(const Record& record, int32_t offset) {
    if (!m_is_open || offset < 0) return false;

    std::vector<char> buffer = record.serialize(schema);
    if (buffer.size() != record_size) return false;

    // Cast 32-bit offset to 64-bit streamoff for file I/O
    data_file.seekp(static_cast<std::streamoff>(offset));
    if (data_file.fail()) {
        std::cerr << "Error: Failed to seek (write) in data file to offset " << offset << std::endl;
        return false;
    }

    data_file.write(buffer.data(), record_size);
    if (data_file.fail()) return false;

    data_file.flush();
    return true;
}

// --- Public Data Manipulation Methods (Corrected) ---

int32_t Table::insert_record(Record& record) {
    if (!m_is_open) return -1;

    data_file.seekp(0, std::ios::end);
    // Get 64-bit offset from file
    std::streamoff offset64 = data_file.tellp();

    // *** CRITICAL CHECK ***
    // Check if 64-bit offset fits in 32-bit B-Tree value
    if (offset64 > std::numeric_limits<int32_t>::max()) {
        std::cerr << "Error: Data file exceeds 2GB limit. Cannot insert." << std::endl;
        return -1;
    }
    int32_t offset32 = static_cast<int32_t>(offset64);

    if (!write_record_at_offset(record, offset32)) {
        return -1;
    }

    Value index_value = record.get_index_value(schema);

    // Insert into B-Tree using 32-bit keys and 32-bit values
    if (schema.get_index_key_type() == KeyType::INTEGER) {
        // Cast 64-bit INT key to 32-bit B-Tree key
        int32_t key32 = static_cast<int32_t>(std::get<int64_t>(index_value));
        btree.insert(key32, offset32);
    } else {
        btree.insert(std::get<std::string>(index_value), offset32);
    }

    return offset32;
}

std::unique_ptr<Record> Table::find_record_by_key(const Value& key) {
    if (!m_is_open) return nullptr;

    std::optional<int32_t> offset_opt; // <-- CHANGED to int32_t
    if (std::holds_alternative<int64_t>(key)) {
        // Cast 64-bit INT key to 32-bit for search
        int32_t key32 = static_cast<int32_t>(std::get<int64_t>(key));
        offset_opt = btree.search(key32);
    } else {
        offset_opt = btree.search(std::get<std::string>(key));
    }

    if (!offset_opt) {
        return nullptr; // Key not found in index
    }

    return read_record_at_offset(*offset_opt);
}

std::vector<std::unique_ptr<Record>> Table::find_records_by_range(const Value& low, const Value& high) {
    if (!m_is_open) {
        throw std::runtime_error("Cannot perform range search, table is not open.");
    }

    std::vector<std::unique_ptr<Record>> results;
    std::vector<int32_t> offsets; // Store offsets separately

    if (std::holds_alternative<int64_t>(low) && std::holds_alternative<int64_t>(high)) {
        // Cast 64-bit INT keys to 32-bit for search
        int32_t min_k = static_cast<int32_t>(std::get<int64_t>(low));
        int32_t max_k = static_cast<int32_t>(std::get<int64_t>(high));

        auto btree_results = btree.rangeSearch(min_k, max_k); // return vector < pair <key ,val>>
        for (const auto& pair : btree_results) {
            offsets.push_back(pair.second);
        }

    } else if (std::holds_alternative<std::string>(low) && std::holds_alternative<std::string>(high)) {

        auto btree_results = btree.rangeSearch(std::get<std::string>(low), std::get<std::string>(high));
        for (const auto& pair : btree_results) {
            offsets.push_back(pair.second);
        }

    } else {
        throw std::runtime_error("Range search type mismatch.");
    }

    for (int32_t offset : offsets) {
        auto record = read_record_at_offset(offset);
        if (record) {
            results.push_back(std::move(record));
        }
    }
    return results;
}

bool Table::delete_record_by_key(const Value& key) {
    if (!m_is_open) return false;

    std::optional<int32_t> offset_opt; // <-- CHANGED to int32_t
    if (std::holds_alternative<int64_t>(key)) {
        int32_t key32 = static_cast<int32_t>(std::get<int64_t>(key));
        offset_opt = btree.search(key32);
    } else {
        offset_opt = btree.search(std::get<std::string>(key));
    }

    if (!offset_opt) {
        std::cerr << "Error: Key not found, cannot delete." << std::endl;
        return false;
    }
    int32_t offset = *offset_opt;

    // Remove from B-Tree
    if (std::holds_alternative<int64_t>(key)) {
        btree.remove(static_cast<int32_t>(std::get<int64_t>(key)));
    } else {
        btree.remove(std::get<std::string>(key));
    }

    // Zero-out data
    std::vector<char> null_buffer(record_size, '\0');
    data_file.seekp(static_cast<std::streamoff>(offset));
    if (data_file.fail()) return false;

    data_file.write(null_buffer.data(), null_buffer.size());
    if (data_file.fail()) return false;

    data_file.flush();
    return true;
}

bool Table::update_record_by_key(const Value& key, const std::map<std::string, Value>& updates) {
    if (!m_is_open) return false;

    const std::string& index_col = schema.index_column_name;
    if (updates.find(index_col) != updates.end()) {
        std::cerr << "Error: Updating the index column ('" << index_col
                  << "') is not supported." << std::endl; // else have to update b-tree also
        return false;
    }

    std::optional<int32_t> offset_opt; // <-- CHANGED to int32_t
    if (std::holds_alternative<int64_t>(key)) {
        int32_t key32 = static_cast<int32_t>(std::get<int64_t>(key));
        offset_opt = btree.search(key32);
    } else {
        offset_opt = btree.search(std::get<std::string>(key));
    }

    if (!offset_opt) {
        std::cerr << "Error: Key not found, cannot update." << std::endl;
        return false;
    }
    int32_t offset = *offset_opt;

    auto record_ptr = read_record_at_offset(offset);
    if (!record_ptr) {
        std::cerr << "Error: Key found in index but record not found." << std::endl;
        return false;
    }

    Record record = *record_ptr;

    for (const auto& pair : updates) {
        if (record.values.find(pair.first) != record.values.end()) {
            record.values[pair.first] = pair.second;
        }
    }

    return write_record_at_offset(record, offset);
}
