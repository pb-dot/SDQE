#include "db_parser.hpp"
#include <sstream>
#include <algorithm>

// --- Whitespace Trim Helper ---
std::string trim(const std::string& str, const std::string& whitespace = " \t\n\r\f\v") {
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos) return ""; // no content

    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}

// --- Main Parse Function ---

ParsedCommand Parser::parse(std::string query) {
    std::stringstream ss(trim(query));
    std::string command = read_keyword(ss);

    if (command == "CREATE_DB")    return parse_create_db(ss);
    if (command == "CREATE_TABLE") return parse_create_table(ss);
    if (command == "INSERT_ROW")   return parse_insert_row(ss);
    if (command == "SELECT")       return parse_select(ss);
    if (command == "DELETE_ROW")   return parse_delete_row(ss);
    if (command == "UPDATE_ROW")   return parse_update_row(ss);

    throw ParseException("Unknown command: " + command);
}

// --- Helper Functions ---

std::string Parser::read_keyword(std::stringstream& ss) {
    std::string keyword;
    ss >> keyword;
    if (keyword.empty()) {
        throw ParseException("Expected keyword, found end of query.");
    }
    return keyword;
}

std::string Parser::read_value(std::stringstream& ss) {
    std::string value_content;
    char ch;

    // Eat whitespace until '{'
    ss >> std::ws;
    if (ss.peek() != '{') {
        throw ParseException("Expected '{' to start value.");
    }
    ss.get(ch); // Consume '{'

    int balance = 1;
    while (ss.get(ch)) {
        if (ch == '}') {
            balance--;
            if (balance == 0) {
                return trim(value_content);
            }
        } else if (ch == '{') {
            balance++;
        }
        value_content += ch;
    }

    throw ParseException("Expected matching '}' to end value.");
}

Value Parser::parse_string_to_value(const std::string& str) {
    std::string trimmed_str = trim(str);
    if (trimmed_str.empty()) throw ParseException("Empty value string");

    // String literal
    if (trimmed_str.front() == '"' && trimmed_str.back() == '"') {
        return trimmed_str.substr(1, trimmed_str.length() - 2);
    }

    // Integer
    try {
        return std::stoll(trimmed_str);
    } catch (...) {
        throw ParseException("Invalid value: not a valid integer or quoted string: " + trimmed_str);
    }
}

// Helper to read a value from a WHERE stream, respecting quotes
std::string read_where_value(std::stringstream& ss) {
    ss >> std::ws;
    if (ss.peek() == '"') {
        std::string val;
        ss.get(); // consume "
        std::getline(ss, val, '"'); // Read until next "
        return '"' + val + '"';
    } else {
        std::string val;
        ss >> val; // Read until space
        return val;
    }
}


// --- Sub-Parsers for Value Blocks ---

/**
 * @brief Parses column definitions from a string.
 * @param s Input string, e.g., "(INT,id), (STR,name)"
 * @return Vector of (type, name) pairs.
 */
std::vector<std::pair<std::string, std::string>> Parser::parse_col_defs(std::string s) {
    std::vector<std::pair<std::string, std::string>> defs;
    std::stringstream ss(s);
    char ch;

    while (ss.peek() != EOF) {
        ss >> std::ws; // eat whitespace
        if (ss.peek() == EOF) break;

        if (ss.get() != '(') { // Read '('
            throw ParseException("Expected '(' for column definition.");
        }

        std::string pair_str;
        std::getline(ss, pair_str, ')'); // Read until ')'

        if (ss.fail()) {
            throw ParseException("Expected ')' to close column definition.");
        }

        pair_str = trim(pair_str);

        size_t comma_pos = pair_str.find(',');
        if (comma_pos == std::string::npos) {
            throw ParseException("Invalid type,name pair: " + pair_str);
        }

        std::string type = trim(pair_str.substr(0, comma_pos));
        std::string name = trim(pair_str.substr(comma_pos + 1));

        if (type != "INT" && type != "STR") {
            throw ParseException("Unknown type: " + type);
        }
        if (name.empty()) {
            throw ParseException("Empty column name.");
        }
        defs.push_back({type, name});

        // After the ')', we expect a ',' or EOF
        ss >> std::ws;
        if (ss.peek() == ',') {
            ss.get(); // Consume the comma
        } else if (ss.peek() != EOF) {
            // Check for trailing characters that aren't commas
            std::string trailing;
            ss >> trailing;
            if (!trailing.empty()) {
                throw ParseException("Expected ',' between column definitions, found '" + trailing + "'");
            }
        }
    }
    return defs;
}


std::map<std::string, Value> Parser::parse_value_pairs(std::string s) {
    // Input: id=10, name = "Alice"
    std::map<std::string, Value> pairs;
    std::stringstream ss(s);
    std::string pair_str;

    while (std::getline(ss, pair_str, ',')) {
        pair_str = trim(pair_str);
        if (pair_str.empty()) continue;

        size_t eq_pos = pair_str.find('=');
        if (eq_pos == std::string::npos) {
            throw ParseException("Invalid key=value pair: " + pair_str);
        }

        std::string key = trim(pair_str.substr(0, eq_pos));
        std::string val_str = trim(pair_str.substr(eq_pos + 1));

        if (key.empty()) throw ParseException("Empty key in key=value pair.");

        pairs[key] = parse_string_to_value(val_str);
    }
    return pairs;
}

std::vector<std::string> Parser::parse_col_list(std::string s) {
    // Input: id, name, email or *
    s = trim(s);
    if (s == "*") return {"*"};

    std::vector<std::string> cols;
    std::stringstream ss(s);
    std::string col;
    while(std::getline(ss, col, ',')) {
        col = trim(col);
        if (!col.empty()) {
            cols.push_back(col);
        }
    }
    return cols;
}

WhereClause Parser::parse_where_index_is_clause(std::string s) {
    // Input: id = 10
    // Input: id > 5 AND id < 20
    // Input: name = "Alice Smith"

    std::stringstream ss(s);
    std::string col, op, val_str, and_str, col2, op2, val_str2;

    ss >> col >> op;
    val_str = read_where_value(ss); // Use quote-aware reader

    ss >> std::ws; // Eat whitespace
    if (ss.eof()) {
        // Simple predicate
        return Predicate{col, op, parse_string_to_value(val_str)};
    }

    ss >> and_str;
    if (and_str != "AND") throw ParseException("Expected 'AND' in range predicate.");

    ss >> col2 >> op2;
    val_str2 = read_where_value(ss); // Use quote-aware reader

    if (col != col2) throw ParseException("Range predicate must be on the same column.");

    return RangePredicate{col, op, parse_string_to_value(val_str), op2, parse_string_to_value(val_str2)};
}


// --- Main Command Parsers ---

ParsedCommand Parser::parse_create_db(std::stringstream& ss) {
    CreateDbCommand cmd;
    cmd.db_name = read_value(ss);
    return cmd;
}

ParsedCommand Parser::parse_create_table(std::stringstream& ss) {
    CreateTableCommand cmd;
    cmd.table_name = read_value(ss);

    if (read_keyword(ss) != "IN") throw ParseException("Expected 'IN'");
    cmd.db_name = read_value(ss);

    if (read_keyword(ss) != "WITH_COL") throw ParseException("Expected 'WITH_COL'");
    cmd.columns = parse_col_defs(read_value(ss));

    // Optional WITH_INDEX
    ss >> std::ws;
    if (ss.peek() == 'W') {
        if (read_keyword(ss) == "WITH_INDEX") {
            cmd.index_column_name = read_value(ss);
        } else {
            throw ParseException("Unknown keyword after WITH_COL");
        }
    }
    return cmd;
}

ParsedCommand Parser::parse_insert_row(std::stringstream& ss) {
    InsertRowCommand cmd;
    if (read_keyword(ss) != "INTO") throw ParseException("Expected 'INTO'");
    cmd.table_name = read_value(ss);

    if (read_keyword(ss) != "IN") throw ParseException("Expected 'IN'");
    cmd.db_name = read_value(ss);

    if (read_keyword(ss) != "VALUES") throw ParseException("Expected 'VALUES'");
    cmd.values = parse_value_pairs(read_value(ss));
    return cmd;
}

ParsedCommand Parser::parse_select(std::stringstream& ss) {
    SelectCommand cmd;
    cmd.columns = parse_col_list(read_value(ss));

    if (read_keyword(ss) != "FROM") throw ParseException("Expected 'FROM'");
    cmd.table_name = read_value(ss);

    if (read_keyword(ss) != "IN") throw ParseException("Expected 'IN'");
    cmd.db_name = read_value(ss);

    // Optional WHERE_INDEX_IS
    ss >> std::ws;
    if (ss.peek() == 'W') {
        if (read_keyword(ss) == "WHERE_INDEX_IS") {
            cmd.where_clause = parse_where_index_is_clause(read_value(ss));
        } else {
            throw ParseException("Unknown keyword after IN, expected 'WHERE_INDEX_IS'");
        }
    }
    return cmd;
}

ParsedCommand Parser::parse_delete_row(std::stringstream& ss) {
    DeleteCommand cmd;
    if (read_keyword(ss) != "FROM") throw ParseException("Expected 'FROM'");
    cmd.table_name = read_value(ss);

    if (read_keyword(ss) != "IN") throw ParseException("Expected 'IN'");
    cmd.db_name = read_value(ss);

    if (read_keyword(ss) != "WHERE_INDEX_IS") throw ParseException("Expected 'WHERE_INDEX_IS'");

    auto clause = parse_where_index_is_clause(read_value(ss));
    if (!std::holds_alternative<Predicate>(clause)) {
        throw ParseException("DELETE must use a simple WHERE_INDEX_IS clause (e.g., id = 10).");
    }
    cmd.where_clause = std::get<Predicate>(clause);
    return cmd;
}

ParsedCommand Parser::parse_update_row(std::stringstream& ss) {
    UpdateCommand cmd;
    cmd.table_name = read_value(ss);

    if (read_keyword(ss) != "IN") throw ParseException("Expected 'IN'");
    cmd.db_name = read_value(ss);

    if (read_keyword(ss) != "SET") throw ParseException("Expected 'SET'");
    cmd.set_values = parse_value_pairs(read_value(ss));

    if (read_keyword(ss) != "WHERE_INDEX_IS") throw ParseException("Expected 'WHERE_INDEX_IS'");

    auto clause = parse_where_index_is_clause(read_value(ss));
    if (!std::holds_alternative<Predicate>(clause)) {
        throw ParseException("UPDATE must use a simple WHERE_INDEX_IS clause (e.g., id = 10).");
    }
    cmd.where_clause = std::get<Predicate>(clause);
    return cmd;
}
