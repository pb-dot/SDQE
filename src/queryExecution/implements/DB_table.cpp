#include "db_table.hpp"
#include <iostream>

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
    if (m_is_open) {
        std::cerr << "Error: Cannot create table, it is already open." << std::endl;
        return false;
    }

    // 1. Save the schema file
    if (!schema_to_create.save(m_schema_path)) {
        std::cerr << "Error: Failed to save schema file: " << m_schema_path << std::endl;
        return false;
    }

    // 2. Create the B-Tree index file
    BTree temp_tree;
    KeyType index_type = schema_to_create.get_index_key_type();
    if (!temp_tree.create(m_index_path, index_type)) {
        std::cerr << "Error: Failed to create index file: " << m_index_path << std::endl;
        return false;
    }
    temp_tree.close();

    // 3. Create the data file
    std::ofstream data_file_stream(m_data_path, std::ios::binary);
    if (!data_file_stream) {
        std::cerr << "Error: Failed to create data file: " << m_data_path << std::endl;
        return false;
    }
    data_file_stream.close();

    return true;
}

bool Table::open() {
    if (m_is_open) {
        return true; // Already open
    }

    // 1. Load schema
    if (!schema.load(m_schema_path)) {
        std::cerr << "Error: Failed to load schema: " << m_schema_path << std::endl;
        return false;
    }
    record_size = schema.get_record_size();

    // 2. Open B-Tree
    // We use the 'btree' member variable now
    if (!btree.open(m_index_path)) {
        std::cerr << "Error: Failed to open index: " << m_index_path << std::endl;
        return false;
    }

    // 3. Open data file
    data_file.open(m_data_path, std::ios::in | std::ios::out | std::ios::binary | std::ios::ate);
    if (!data_file.is_open()) {
        // Fallback for files that might exist but are empty/read-only issue
        data_file.open(m_data_path, std::ios::in | std::ios::out | std::ios::binary);
        if (!data_file.is_open()) {
            std::cerr << "Error: Failed to open data file: " << m_data_path << std::endl;
            btree.close(); // Clean up btree if data file fails
            return false;
        }
    }

    data_file.seekp(0, std::ios::end); // Default to appending
    m_is_open = true;
    return true;
}

void Table::close() {
    if (!m_is_open) {
        return; // Already closed
    }

    // Note: We don't check btree.isOpen() because it doesn't exist.
    // We just trust that our 'm_is_open' flag is the source of truth.
    btree.close();

    if (data_file.is_open()) {
        data_file.close();
    }

    m_is_open = false;
}

int64_t Table::insert_record(Record& record) {
    if (!m_is_open) {
        std::cerr << "Error: Cannot insert, table is not open." << std::endl;
        return -1;
    }

    // 1. Find the current end of the file
    data_file.seekp(0, std::ios::end);
    int64_t offset = data_file.tellp();

    // 2. Serialize
    std::vector<char> buffer = record.serialize(schema);

    // 3. Write to data file
    data_file.write(buffer.data(), buffer.size());
    if (data_file.fail()) {
        std::cerr << "Error: Failed to write to data file." << std::endl;
        return -1;
    }
    data_file.flush();

    // 4. Get index key
    Value index_value;
    try {
        index_value = record.get_index_value(schema);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        // This is tricky. We wrote data but can't index it.
        // A real DB would need to roll back the data file write.
        // For us, we'll just report the error.
        return -1;
    }

    // 5. Insert into B-Tree
    if (schema.get_index_key_type() == KeyType::INTEGER) {
        btree.insert(std::get<int64_t>(index_value), offset);
    } else {
        btree.insert(std::get<std::string>(index_value), offset);
    }

    return offset;
}

std::unique_ptr<Record> Table::find_record_by_key(const Value& key) {
    if (!m_is_open) {
        std::cerr << "Error: Cannot find, table is not open." << std::endl;
        return nullptr;
    }

    // 1. Search B-Tree
    std::optional<int64_t> offset_opt;
    if (std::holds_alternative<int64_t>(key)) {
        offset_opt = btree.search(std::get<int64_t>(key));
    } else {
        offset_opt = btree.search(std::get<std::string>(key));
    }

    if (!offset_opt) {
        return nullptr; // Key not found
    }
    int64_t offset = *offset_opt;

    // 2. Seek in data file
    data_file.seekg(offset);
    if (data_file.fail()) {
        std::cerr << "Error: Failed to seek in data file to offset " << offset << std::endl;
        return nullptr;
    }

    // 3. Read record
    std::vector<char> buffer(record_size);
    data_file.read(buffer.data(), record_size);
    if (data_file.fail() || data_file.gcount() != record_size) {
        std::cerr << "Error: Failed to read full record from data file." << std::endl;
        return nullptr;
    }

    // 4. Deserialize
    auto record = std::make_unique<Record>();
    record->deserialize(buffer.data(), schema);
    return record;
}
