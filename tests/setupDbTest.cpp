#include <iostream>
#include "db_engine.hpp"
#include <filesystem>

int main() {
    // --- THIS IS THE "RESET" PROGRAM ---
    // Clean up from previous runs
    std::filesystem::remove_all("test_db");
    std::filesystem::remove_all("string_db");

    DatabaseEngine engine;

    std::cout << "\n--- 1. Creating Databases ---" << std::endl;
    engine.execute("CREATE_DB {test_db}");
    engine.execute("CREATE_DB {string_db}");

    std::cout << "\n--- 2. Creating Tables ---" << std::endl;
    engine.execute("CREATE_TABLE {users} IN {test_db} WITH_COL {(INT,id),(STR,name),(STR,email)} WITH_INDEX {id}");
    engine.execute("CREATE_TABLE {logs} IN {test_db} WITH_COL { (STR,level), (STR,message) }");
    engine.execute("CREATE_TABLE {products} IN {string_db} WITH_COL {(STR,sku),(STR,name),(INT,price)} WITH_INDEX {sku}");

    std::cout << "\n--- 3. Inserting Rows (Users) ---" << std::endl;
    engine.execute("INSERT_ROW INTO {users} IN {test_db} VALUES { id = 10, name=\"Alice\", email=\"alice@a.com\" }");
    engine.execute("INSERT_ROW INTO {users} IN {test_db} VALUES {id=5, name = \"Bob\", email = \"bob@b.com\"}");
    engine.execute("INSERT_ROW INTO {users} IN {test_db} VALUES {id=12, name=\"Charlie\", email=\"charlie@c.com\"}");

    std::cout << "\n--- 4. Inserting Rows (Logs) ---" << std::endl;
    engine.execute("INSERT_ROW INTO {logs} IN {test_db} VALUES {level=\"INFO\", message=\"System start\"}");
    engine.execute("INSERT_ROW INTO {logs} IN {test_db} VALUES {level=\"WARN\", message=\"Disk low\"}");

    std::cout << "\n--- 5. Inserting Rows (Products) ---" << std::endl;
    engine.execute("INSERT_ROW INTO {products} IN {string_db} VALUES { sku=\"A-100\", name=\"Apple\", price=5 }");
    engine.execute("INSERT_ROW INTO {products} IN {string_db} VALUES { sku=\"B-200\", name=\"Banana\", price=2 }");

    std::cout << "\n--- Database Setup Complete. ---" << std::endl;
    std::cout << "--- You can now run the 'query_db' program. ---" << std::endl;

    return 0;
}
