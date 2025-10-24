#include "db_engine.hpp"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <variant>

namespace fs = std::filesystem;

// --- HELPER FUNCTION ---
/**
 * @brief Converts a Value variant to a printable string.
 */
std::string value_to_string(const Value& val) {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int64_t>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return "'" + arg + "'";
        }
    }, val);
}

// --- Helper Function for Selective SELECT ---

/**
 * @brief Prints a record, but only the columns specified in sel_cols.
 */
static void print_record_selective(const Record& record, const Schema& schema, const std::vector<std::string>& sel_cols) {
    std::cout << "{ ";
    bool first = true;

    // If {*}, print all columns in schema order
    if (sel_cols.size() == 1 && sel_cols[0] == "*") {
        for (const auto& col : schema.columns) {
            auto it = record.values.find(col.name);
            if (it == record.values.end()) continue;

            if (!first) std::cout << ", ";
            std::cout << col.name << ": ";
            if (col.type == DataType::INT) {
                std::cout << std::get<int64_t>(it->second);
            } else {
                std::cout << "'" << std::get<std::string>(it->second) << "'";
            }
            first = false;
        }
    } else {
        // Print only specified columns
        for (const std::string& col_name : sel_cols) {
            auto it = record.values.find(col_name);
            auto col_schema = schema.get_column(col_name);

            if (it == record.values.end() || !col_schema) {
                std::cerr << "Warning: Column '" << col_name << "' not found. Skipping." << std::endl;
                continue;
            }

            if (!first) std::cout << ", ";
            std::cout << col_name << ": ";
            if (col_schema->type == DataType::INT) {
                std::cout << std::get<int64_t>(it->second);
            } else {
                std::cout << "'" << std::get<std::string>(it->second) << "'";
            }
            first = false;
        }
    }
    std::cout << " }" << std::endl;
}

// --- Engine Implementation ---

DatabaseEngine::DatabaseEngine() {
    std::cout << "Database Engine Initialized." << std::endl;
}

void DatabaseEngine::execute(const std::string& query) {

    std::cout << "Query Received :-> "<<query <<"\n";
    try {
        ParsedCommand parsed_command = parser.parse(query);

        // std::visit calls the correct overloaded lambda for the variant type
        std::visit([this](auto&& cmd) {
            using T = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<T, CreateDbCommand>) {
                exec_create_db(cmd);
            } else if constexpr (std::is_same_v<T, CreateTableCommand>) {
                // CreateTable needs to be modified for default index
                CreateTableCommand modifiable_cmd = cmd;
                exec_create_table(modifiable_cmd);
            } else if constexpr (std::is_same_v<T, InsertRowCommand>) {
                exec_insert_row(cmd);
            } else if constexpr (std::is_same_v<T, SelectCommand>) {
                exec_select(cmd);
            } else if constexpr (std::is_same_v<T, DeleteCommand>) {
                exec_delete_row(cmd);
            } else if constexpr (std::is_same_v<T, UpdateCommand>) {
                exec_update_row(cmd);
            }
        }, parsed_command);

    } catch (const ParseException& e) {
        std::cerr << "Parse Error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Execution Error: " << e.what() << std::endl;
    }
}

// --- Execution Handlers ---

void DatabaseEngine::exec_create_db(const CreateDbCommand& cmd) {
    if (fs::create_directory(cmd.db_name)) {
        std::cout << "Database '" << cmd.db_name << "' created." << std::endl;
    } else {
        std::cerr << "Error: Could not create database '" << cmd.db_name << "'." << std::endl;
    }
}

void DatabaseEngine::exec_create_table(CreateTableCommand& cmd) {
    Schema schema_to_create;

    // Handle default index
    if (cmd.index_column_name.empty()) {
        cmd.index_column_name = "row_num";
        cmd.columns.insert(cmd.columns.begin(), {"INT", "row_num"});
    }

    for (const auto& col_pair : cmd.columns) {
        DataType type = (col_pair.first == "INT") ? DataType::INT : DataType::STRING;
        schema_to_create.columns.emplace_back(col_pair.second, type);
    }
    schema_to_create.index_column_name = cmd.index_column_name;
    schema_to_create.next_auto_increment_id = 1;

    std::string base_path = get_base_path(cmd.db_name, cmd.table_name);
    Table table(base_path + ".schema", base_path + ".idx", base_path + ".data");

    if (table.create(schema_to_create)) {
        std::cout << "Table '" << cmd.table_name << "' created in '" << cmd.db_name << "'." << std::endl;
    } else {
        std::cerr << "Error: Failed to create table '" << cmd.table_name << "'." << std::endl;
    }
}

void DatabaseEngine::exec_insert_row(const InsertRowCommand& cmd) {
    std::string base_path = get_base_path(cmd.db_name, cmd.table_name);
    Table table(base_path + ".schema", base_path + ".idx", base_path + ".data");

    if (!table.open()) {
        throw std::runtime_error("Table '" + cmd.table_name + "' not found.");
    }

    Record record;
    record.values = cmd.values;

    Schema& schema = table.get_schema();

    // Handle default index (auto-increment)
    if (schema.index_column_name == "row_num" && record.values.find("row_num") == record.values.end()) {
        int64_t next_id = schema.next_auto_increment_id++;
        record.values["row_num"] = next_id;
        schema.save(base_path + ".schema");
    }

    if (table.insert_record(record) != -1) {
        std::cout << "Row inserted into '" << cmd.table_name << "'." << std::endl;
    } else {
        std::cerr << "Error: Failed to insert row." << std::endl;
    }
}

void DatabaseEngine::exec_select(const SelectCommand& cmd) {
    std::cout << "--- Executing SELECT ---" << std::endl;
    std::string base_path = get_base_path(cmd.db_name, cmd.table_name);
    Table table(base_path + ".schema", base_path + ".idx", base_path + ".data");

    if (!table.open()) {
        throw std::runtime_error("Table '" + cmd.table_name + "' not found.");
    }

    Schema& schema = table.get_schema();

    if (cmd.where_clause) {
        // --- 1. Index Search (Point or Range) ---
        auto& clause = *cmd.where_clause;
        if (std::holds_alternative<Predicate>(clause)) {
            // Point search
            Predicate p = std::get<Predicate>(clause);
            std::cout << "Point search: " << p.column_name << " " << p.op << "  "<< value_to_string(p.value) << std::endl;
            auto record = table.find_record_by_key(p.value);
            if(record) {
                print_record_selective(*record, schema, cmd.columns);
            } else {
                std::cout << "No record found." << std::endl;
            }
        } else {
            // Range search
            RangePredicate p = std::get<RangePredicate>(clause);
            std::cout << "Range search: " << p.column_name << " " << p.op_low << " "<< value_to_string(p.val_low)<<" AND "
                      << p.op_high << "  "<< value_to_string(p.val_high) << std::endl;
            auto records = table.find_records_by_range(p.val_low, p.val_high);
            if (records.empty()) {
                std::cout << "No records found in range." << std::endl;
            }
            for (const auto& record : records) {
                print_record_selective(*record, schema, cmd.columns);
            }
        }
    } else {
        // --- 2. Full Table Scan (No WHERE_INDEX_IS) ---
        std::cout << "Full table scan:" << std::endl;
        size_t record_size = schema.get_record_size();
        std::fstream& data_file = table.get_data_file_stream();

        data_file.seekg(0, std::ios::beg);

        std::vector<char> buffer(record_size);
        int count = 0;
        while (data_file.read(buffer.data(), record_size)) {
            // Check for deleted record (tombstone)
            if (buffer[0] == '\0') {
                continue;
            }

            Record record;
            record.deserialize(buffer.data(), schema);
            print_record_selective(record, schema, cmd.columns);
            count++;
        }
        if (count == 0) {
            std::cout << "Table is empty." << std::endl;
        }
    }
}

void DatabaseEngine::exec_delete_row(const DeleteCommand& cmd) {
    std::cout << "--- Executing DELETE ---" << std::endl;
    std::string base_path = get_base_path(cmd.db_name, cmd.table_name);
    Table table(base_path + ".schema", base_path + ".idx", base_path + ".data");

    if (!table.open()) {
        throw std::runtime_error("Table '" + cmd.table_name + "' not found.");
    }

    if (table.delete_record_by_key(cmd.where_clause.value)) {
        std::cout << "Record deleted successfully." << std::endl;
    } else {
        std::cout << "Failed to delete record." << std::endl;
    }
}

void DatabaseEngine::exec_update_row(const UpdateCommand& cmd) {
    std::cout << "--- Executing UPDATE ---" << std::endl;
    std::string base_path = get_base_path(cmd.db_name, cmd.table_name);
    Table table(base_path + ".schema", base_path + ".idx", base_path + ".data");

    if (!table.open()) {
        throw std::runtime_error("Table '" + cmd.table_name + "' not found.");
    }

    if (table.update_record_by_key(cmd.where_clause.value, cmd.set_values)) {
        std::cout << "Record updated successfully." << std::endl;
    } else {
        std::cout << "Failed to update record." << std::endl;
    }
}

// --- Helper ---

std::string DatabaseEngine::get_base_path(const std::string& db_name, const std::string& table_name) {
    // Basic path sanitization
    if (db_name.find("..") != std::string::npos || table_name.find("..") != std::string::npos) {
        throw std::runtime_error("Invalid path: '..' is not allowed.");
    }
    return db_name + "/" + table_name;
}
