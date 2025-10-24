#pragma once

#include <string>
#include <fstream>
#include <memory>
#include "db_btree.hpp"
#include "db_schema.hpp"
#include "db_record.hpp"

/**
 * @class Table
 * @brief Manages a single table (schema, index, data files) using RAII.
 * The table is automatically closed when it goes out of scope.
 */
class Table {
public:
    /**
     * @brief Constructs a Table manager. Does not open any files.
     */
    Table(const std::string& schema_path, const std::string& index_path,
          const std::string& data_path);

    /**
     * @brief Destructor. Automatically calls close().
     */
    ~Table();

    /**
     * @brief Creates the new set of table files (.schema, .idx, .data) on disk.
     * @param schema The schema to save.
     * @return true on success, false on failure.
     */
    bool create(const Schema& schema);

    /**
     * @brief Opens an existing table's files.
     * @return true on success, false on failure.
     */
    bool open();

    /**
     * @brief Closes all file handles.
     */
    void close();

    /**
     * @brief Inserts a record into the data file and its key into the B-Tree.
     * @param record The record to insert (must contain the index key).
     * @return The file offset (row pointer) where the record was inserted, or -1 on failure.
     */
    int64_t insert_record(Record& record);

    /**
     * @brief Finds a record by its index key.
     * @param key The index key (int64_t or string) to search for.
     * @return A unique_ptr to the Record if found, nullptr otherwise.
     */
    std::unique_ptr<Record> find_record_by_key(const Value& key);

    // ... find_by_range, delete_record, update_record will go here ...

    Schema& get_schema() { return schema; }

private:
    // File paths
    std::string m_schema_path;
    std::string m_index_path;
    std::string m_data_path;

    // State
    Schema schema;
    BTree btree;
    std::fstream data_file; // For reading/writing the .data heap file
    size_t record_size = 0;
    bool m_is_open = false;
};
