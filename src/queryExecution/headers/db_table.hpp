#pragma once

#include <string>
#include <fstream>
#include <memory>
#include <vector>
#include <map>
#include "db_btree.hpp"
#include "db_schema.hpp"
#include "db_record.hpp"

class Table {
public:
    Table(const std::string& schema_path, const std::string& index_path,
          const std::string& data_path);

    ~Table();

    bool create(const Schema& schema);
    bool open();
    void close();

    /**
     * @brief Inserts a record.
     * @return The 32-bit file offset (row pointer), or -1 on failure.
     */
    int32_t insert_record(Record& record);

    std::unique_ptr<Record> find_record_by_key(const Value& key);

    std::vector<std::unique_ptr<Record>> find_records_by_range(const Value& low, const Value& high);

    bool delete_record_by_key(const Value& key);

    bool update_record_by_key(const Value& key, const std::map<std::string, Value>& updates);

    Schema& get_schema() { return schema; }
    std::fstream& get_data_file_stream() { return data_file; }

private:
    /**
     * @brief Reads a record from a 32-bit offset.
     */
    std::unique_ptr<Record> read_record_at_offset(int32_t offset);

    /**
     * @brief Writes a record to a 32-bit offset.
     */
    bool write_record_at_offset(const Record& record, int32_t offset);

    // File paths
    std::string m_schema_path;
    std::string m_index_path;
    std::string m_data_path;

    // State
    Schema schema;
    BTree btree;
    std::fstream data_file;
    size_t record_size = 0;
    bool m_is_open = false;
};
