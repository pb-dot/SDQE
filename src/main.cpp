#include <iostream>
#include "db_engine.hpp"
#include <filesystem>

int main() {
    // Clean up from previous runs
    std::filesystem::remove_all("test_db");
    std::filesystem::remove_all("string_db");

    DatabaseEngine engine;

    std::cout << "\n--- 1. Creating Databases ---" << std::endl;
    engine.execute("CREATE_DB {test_db}");
    engine.execute("CREATE_DB {string_db}");

    std::cout << "\n--- 2. Creating Table (Users) with INT index---" << std::endl;
    engine.execute("CREATE_TABLE {users} IN {test_db} WITH_COL {(INT,id),(STR,name),(STR,email)} WITH_INDEX {id}");

    std::cout << "\n--- 3. Creating Table (Logs) with default index ---" << std::endl;
    engine.execute("CREATE_TABLE {logs} IN {test_db} WITH_COL { (STR,level), (STR,message) }");

    std::cout << "\n--- 4. Creating Table (Products) with STRING index ---" << std::endl;
    engine.execute("CREATE_TABLE {products} IN {string_db} WITH_COL {(STR,sku),(STR,name),(INT,price)} WITH_INDEX {sku}");

    std::cout << "\n--- 5. Inserting Rows (Users) ---" << std::endl;
    engine.execute("INSERT_ROW INTO {users} IN {test_db} VALUES { id = 10, name=\"Alice\", email=\"alice@a.com\" }");
    engine.execute("INSERT_ROW INTO {users} IN {test_db} VALUES {id=5, name = \"Bob\", email = \"bob@b.com\"}");
    engine.execute("INSERT_ROW INTO {users} IN {test_db} VALUES {id=12, name=\"Charlie\", email=\"charlie@c.com\"}");

    std::cout << "\n--- 6. Inserting Rows (Logs) ---" << std::endl;
    engine.execute("INSERT_ROW INTO {logs} IN {test_db} VALUES {level=\"INFO\", message=\"System start\"}");
    engine.execute("INSERT_ROW INTO {logs} IN {test_db} VALUES {level=\"WARN\", message=\"Disk low\"}");

    std::cout << "\n--- 7. Inserting Rows (Products) ---" << std::endl;
    engine.execute("INSERT_ROW INTO {products} IN {string_db} VALUES { sku=\"A-100\", name=\"Apple\", price=5 }");
    engine.execute("INSERT_ROW INTO {products} IN {string_db} VALUES { sku=\"B-200\", name=\"Banana\", price=2 }");

    std::cout << "\n--- 8. Testing SELECT (Full Scan) ---" << std::endl;
    engine.execute("SELECT {*} FROM {users} IN {test_db}");

    std::cout << "\n--- 9. Testing SELECT (Selective Columns) ---" << std::endl;
    engine.execute("SELECT {name, email} FROM {users} IN {test_db} WHERE_INDEX_IS {id = 10}");

    std::cout << "\n--- 10. Testing SELECT (Range Query) ---" << std::endl;
    engine.execute("SELECT {name} FROM {users} IN {test_db} WHERE_INDEX_IS {id > 5 AND id < 15}");

    std::cout << "\n--- 11. Testing SELECT (String Index) ---" << std::endl;
    engine.execute("SELECT {name, price} FROM {products} IN {string_db} WHERE_INDEX_IS {sku = \"A-100\"}");

    std::cout << "\n--- 12. Testing SELECT (Default Index) ---" << std::endl;
    engine.execute("SELECT {*} FROM {logs} IN {test_db} WHERE_INDEX_IS {row_num = 2}");

    std::cout << "\n--- 13. Testing UPDATE ---" << std::endl;
    engine.execute("UPDATE_ROW {users} IN {test_db} SET {name=\"Alice Smith\", email=\"alice.smith@new.com\"} WHERE_INDEX_IS {id = 10}");

    std::cout << "\n--- 14. Testing SELECT (Confirm Update) ---" << std::endl;
    engine.execute("SELECT {name, email} FROM {users} IN {test_db} WHERE_INDEX_IS {id = 10}");

    std::cout << "\n--- 15. Testing DELETE ---" << std::endl;
    engine.execute("DELETE_ROW FROM {users} IN {test_db} WHERE_INDEX_IS {id = 5}");

    std::cout << "\n--- 16. Testing SELECT (Confirm Delete) ---" << std::endl;
    engine.execute("SELECT {*} FROM {users} IN {test_db} WHERE_INDEX_IS {id = 5}");

    std::cout << "\n--- 17. Testing Full Scan (After Delete) ---" << std::endl;
    std::cout << "(Should only show 10 and 12, skipping the zeroed-out '5')" << std::endl;
    engine.execute("SELECT {*} FROM {users} IN {test_db}");

    std::cout << "\n--- 18. Testing Parse Error ---" << std::endl;
    engine.execute("SELECT {name} FROM {users} WHERE {id = 10}");

    return 0;
}
