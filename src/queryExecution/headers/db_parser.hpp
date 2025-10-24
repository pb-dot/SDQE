#pragma once

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <stdexcept>
#include "db_record.hpp"

struct Predicate {
    std::string column_name;
    std::string op; // e.g., "=", ">", "<"
    Value value;
};

struct RangePredicate {
    std::string column_name;
    std::string op_low;  // > or >=
    Value val_low;
    std::string op_high; // < or <=
    Value val_high;
};

// A WHERE clause can be a simple predicate or a range
using WhereClause = std::variant<Predicate, RangePredicate>;

// --- Command Structs (Full Set) ---

struct CreateDbCommand {
    std::string db_name;
};

struct CreateTableCommand {
    std::string table_name;
    std::string db_name;
    std::vector<std::pair<std::string, std::string>> columns;
    std::string index_column_name;
};

struct InsertRowCommand {
    std::string table_name;
    std::string db_name;
    std::map<std::string, Value> values;
};

struct SelectCommand {
    std::vector<std::string> columns; // {"*"} for all
    std::string table_name;
    std::string db_name;
    std::optional<WhereClause> where_clause;
};

struct DeleteCommand {
    std::string table_name;
    std::string db_name;
    Predicate where_clause; // Deletion must be by simple index
};

struct UpdateCommand {
    std::string table_name;
    std::string db_name;
    std::map<std::string, Value> set_values;
    Predicate where_clause; // Update must be by simple index
};

// The main command variant
using ParsedCommand = std::variant<
    CreateDbCommand,
    CreateTableCommand,
    InsertRowCommand,
    SelectCommand,
    DeleteCommand,
    UpdateCommand
>;

// --- Parser Exception ---
class ParseException : public std::runtime_error {
public:
    ParseException(const std::string& msg) : std::runtime_error(msg) {}
};

// --- Parser Class ---
class Parser {
public:
    ParsedCommand parse(std::string query);

private:
    // Main parsers
    ParsedCommand parse_create_db(std::stringstream& ss);
    ParsedCommand parse_create_table(std::stringstream& ss);
    ParsedCommand parse_insert_row(std::stringstream& ss);
    ParsedCommand parse_select(std::stringstream& ss);
    ParsedCommand parse_delete_row(std::stringstream& ss);
    ParsedCommand parse_update_row(std::stringstream& ss);

    // Helper functions
    std::string read_keyword(std::stringstream& ss);
    std::string read_value(std::stringstream& ss);
    Value parse_string_to_value(const std::string& str);

    // Sub-parsers for complex values
    std::vector<std::pair<std::string, std::string>> parse_col_defs(std::string s);
    std::map<std::string, Value> parse_value_pairs(std::string s);
    std::vector<std::string> parse_col_list(std::string s);
    WhereClause parse_where_index_is_clause(std::string s);
};
