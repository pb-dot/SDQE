
//REPL mode -->

#include <iostream>
#include <string>
#include <sstream>
#include <chrono>       // For query timing
#include <filesystem>   // For .tables command
#include <fstream>      // For .schema command
#include "db_engine.hpp"

namespace fs = std::filesystem;

// --- Helper: String Trimmer ---
// Trims whitespace from both ends of a string
std::string trim(const std::string& str, const std::string& whitespace = " \t\n\r\f\v") {
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos) return ""; // no content
    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;
    return str.substr(strBegin, strRange);
}

// --- Helper: Welcome Banner ---
void print_welcome_banner() {
    std::cout << "**********************************************" << std::endl;
    std::cout << "* *" << std::endl;
    std::cout << "* Welcome to Structured_Data_Query_Engine !" << std::endl;
    std::cout << "* *" << std::endl;
    std::cout << "* Type .help for command syntax.     *" << std::endl;
    std::cout << "* Type .quit to exit.                *" << std::endl;
    std::cout << "* *" << std::endl;
    std::cout << "**********************************************" << std::endl;
    std::cout << std::endl;
}

// --- Helper: Help Text ---
void print_help() {
    std::cout << "--- SQL Commands ---\n" << std::endl;
    std::cout << "  Create DB: CREATE_DB {database_name}\n" << std::endl;
    std::cout << "  Create Table: CREATE_TABLE {table_name} IN {db_name} "
              << "  WITH_COL {(INT,id),(STR,name)} "
              << "  WITH_INDEX {id}\n" << std::endl;
    std::cout << "  Insert: INSERT_ROW INTO {table_name} IN {db_name} "
              << "  VALUES {id=10, name=\"Alice\"}\n" << std::endl;
    std::cout << "  Select (Point): SELECT {*} FROM {tableName} IN {db_name} "
              << "  WHERE_INDEX_IS {id = 10}\n" << std::endl;
    std::cout << "  Select (Range): SELECT {col_name} FROM {tableName} IN {db_name} "
              << "  WHERE_INDEX_IS {id > 5 AND id < 20}\n" << std::endl;
    std::cout << "  Delete: DELETE_ROW FROM {tableName} IN {db_name} "
              << "  WHERE_INDEX_IS {id = 5}\n" << std::endl;
    std::cout << "  Update: UPDATE_ROW {tableName} IN {db_name} "
              << "  SET {name=\"Robert\"} WHERE_INDEX_IS {id = 10}\n" << std::endl;

    std::cout << "--- Meta-Commands ---\n" << std::endl;
    std::cout << "  .help                       Show this help message." << std::endl;
    std::cout << "  .quit                       Exit the REPL." << std::endl;
    std::cout << "  .clear                      Clear the terminal screen." << std::endl;
    std::cout << "  .tables db_name           List all tables in a database." << std::endl;
    std::cout << "  .schema db_name tbl_name  Show the schema for a table." << std::endl;
    std::cout << std::endl;
}

// --- Helper: .tables Command ---
void handle_list_tables(const std::string& db_name) {
    if (db_name.empty()) {
        std::cerr << "Usage: .tables database_name" << std::endl;
        return;
    }

    fs::path db_path(db_name);
    if (!fs::exists(db_path) || !fs::is_directory(db_path)) {
        std::cerr << "Error: Database '" << db_name << "' not found." << std::endl;
        return;
    }

    std::cout << "Tables in " << db_name << ":" << std::endl;
    int count = 0;
    for (const auto& entry : fs::directory_iterator(db_path)) {
        if (entry.path().extension() == ".schema") {
            std::cout << "  " << entry.path().stem().string() << std::endl;
            count++;
        }
    }
    if (count == 0) {
        std::cout << "  (No tables found)" << std::endl;
    }
}

// --- Helper: .schema Command ---
void handle_show_schema(const std::string& db_name, const std::string& table_name) {
    if (db_name.empty() || table_name.empty()) {
        std::cerr << "Usage: .schema database_name table_name" << std::endl;
        return;
    }

    std::string schema_path = db_name + "/" + table_name + ".schema";
    std::ifstream file(schema_path);
    if (!file.is_open()) {
        std::cerr << "Error: Table '" << table_name << "' in database '" << db_name << "' not found." << std::endl;
        return;
    }

    std::cout << "Schema for " << table_name << ":" << std::endl;
    std::string line;
    while (std::getline(file, line)) {
        std::cout << "  " << line << std::endl;
    }
    file.close();
}


// --- The Main REPL ---
int main() {
    print_welcome_banner();
    DatabaseEngine engine;
    std::string line;

    while (true) {
        std::cout << "_prompt > ";
        if (!std::getline(std::cin, line)) {
            break; // EOF (e.g., Ctrl+D)
        }

        std::string query = trim(line);
        if (query.empty()) {
            continue;
        }

        // --- Handle Meta-Commands ---
        if (query[0] == '.') {
            std::stringstream ss(query);
            std::string cmd;
            ss >> cmd;

            if (cmd == ".quit") {
                break;
            } else if (cmd == ".help") {
                print_help();
            } else if (cmd == ".clear") {
                #if defined(_WIN32)
                    system("cls");
                #else
                    system("clear");
                #endif
            } else if (cmd == ".tables") {
                std::string db_name;
                ss >> db_name;
                handle_list_tables(trim(db_name));
            } else if (cmd == ".schema") {
                std::string db_name, table_name;
                ss >> db_name >> table_name;
                handle_show_schema(trim(db_name), trim(table_name));
            } else {
                std::cerr << "Error: Unknown command '" << cmd << "'. Type .help for a list." << std::endl;
            }
            continue; // Go back to prompt
        }

        // --- Handle SQL Queries ---
        try {
            // Start Timer
            auto start = std::chrono::high_resolution_clock::now();

            engine.execute(query);

            // Stop Timer
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> ms = end - start;

            std::cout << "\n(Query took " << ms.count() << " ms)" << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
        std::cout << std::endl;
    }

    std::cout << "Goodbye!" << std::endl;
    return 0;
}
