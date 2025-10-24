#pragma once

#include <string>
#include <map>
#include <memory>
#include "db_table.hpp"
#include "db_parser.hpp" // Our robust parser

/**
 * @class DatabaseEngine
 * @brief The main engine. Parses queries and dispatches to table managers.
 */
class DatabaseEngine {
public:
    DatabaseEngine();

    /**
     * @brief Executes a query from your custom language.
     * @param query The raw query string.
     */
    void execute(const std::string& query);

private:
    // --- Execution Handlers ---
    void exec_create_db(const CreateDbCommand& cmd);
    void exec_create_table(CreateTableCommand& cmd); // non-const for default index
    void exec_insert_row(const InsertRowCommand& cmd);
    void exec_select(const SelectCommand& cmd);
    void exec_delete_row(const DeleteCommand& cmd);
    void exec_update_row(const UpdateCommand& cmd);

    // --- Helper ---
    std::string get_base_path(const std::string& db_name, const std::string& table_name);

    // --- State ---
    Parser parser; // The parser is a member
};
