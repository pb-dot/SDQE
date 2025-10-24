#include "db_engine.hpp"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <variant>

namespace fs = std::filesystem;

DatabaseEngine::DatabaseEngine() {
    std::cout << "Database Engine Initialized." << std::endl;
}

// --- Main Execution ---

void DatabaseEngine::execute(const std::string& query) {
    try {
        ParsedCommand parsed_command = parser.parse(query);

        std::visit([this](auto&& cmd) {
            using T = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<T, CreateDbCommand>) {
                exec_create_db(cmd);
            } else if constexpr (std::is_same_v<T, CreateTableCommand>) {
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

// --- Execution Handlers (Only exec_select is updated) ---

// ... exec_create_db, exec_create_table, exec_insert_row are unchanged ...
void DatabaseEngine::exec_create_db(const CreateDbCommand& cmd) {
    if (fs::create_directory(cmd.db_name)) {
        std::cout << "Database '" << cmd.db_name << "' created." << std::endl;
    } else {
        std::cerr << "Error: Could not create database '" << cmd.db_name << "'." << std::endl;
    }
}

void DatabaseEngine::exec_create_table(CreateTableCommand& cmd) {
    Schema schema_to_create;

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

    if (cmd.where_clause) {
        // We have a WHERE_INDEX_IS clause
        auto& clause = *cmd.where_clause;
        if (std::holds_alternative<Predicate>(clause)) {
            // Simple: WHERE id = 10
            Predicate p = std::get<Predicate>(clause);
            std::cout << "Point search: " << p.column_name << " " << p.op << " ..." << std::endl;
            auto record = table.find_record_by_key(p.value);
            if(record) {
                record->print(table.get_schema());
            } else {
                std::cout << "No record found." << std::endl;
            }
        } else {
            // Range: WHERE id > 5 AND id < 20
            RangePredicate p = std::get<RangePredicate>(clause);
            std::cout << "Range search: " << p.column_name << " " << p.op_low << " ... AND "
                      << p.op_high << " ..." << std::endl;
            // TODO: Implement this using btree.rangeSearch()
            std::cout << "Range search (Not Implemented Yet)" << std::endl;
        }
    } else {
        // No WHERE clause: Full table scan
        // TODO: Implement this by reading the .data file from start to finish
        std::cout << "Full table scan (Not Implemented Yet)" << std::endl;
    }
}

void DatabaseEngine::exec_delete_row(const DeleteCommand& cmd) {
    std::cout << "--- Executing DELETE ---" << std::endl;
    // TODO: Implement Delete
    std::cout << "Delete (Not Implemented Yet) for " << cmd.where_clause.column_name
              << " = ..." << std::endl;
}

void DatabaseEngine::exec_update_row(const UpdateCommand& cmd) {
    std::cout << "--- Executing UPDATE ---" << std::endl;
    // TODO: Implement Update
    std::cout << "Update (Not Implemented Yet) for " << cmd.where_clause.column_name
              << " = ..." << std::endl;
}


// --- Helpers ---

std::string DatabaseEngine::get_base_path(const std::string& db_name, const std::string& table_name) {
    if (db_name.find("..") != std::string::npos || table_name.find("..") != std::string::npos) {
        throw std::runtime_error("Invalid path: '..' is not allowed.");
    }
    return db_name + "/" + table_name;
}
