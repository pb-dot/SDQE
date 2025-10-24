#include <iostream>
#include "db_engine.hpp"
#include <filesystem>


int main() {
    std::filesystem::remove_all("test_db");

    DatabaseEngine engine;

    std::cout << "\n--- 1. Creating Database ---" << std::endl;
    engine.execute("CREATE_DB {test_db}");

    std::cout << "\n--- 2. Creating Table (Users) ---" << std::endl;
    engine.execute("CREATE_TABLE {users} IN {test_db} WITH_COL {(INT,id),(STR,name),(STR,email)} WITH_INDEX {id}");

    std::cout << "\n--- 3. Creating Table (Logs) with default index ---" << std::endl;
    engine.execute("CREATE_TABLE {logs} IN {test_db} WITH_COL { (STR,level), (STR,message) }");

    std::cout << "\n--- 4. Inserting Rows (Users) with extra spaces ---" << std::endl;
    engine.execute("INSERT_ROW INTO {users} IN {test_db} VALUES { id = 10, name=\"Alice\", email=\"alice@a.com\" }");
    engine.execute("INSERT_ROW INTO {users} IN {test_db} VALUES {id=5, name = \"Bob\", email = \"bob@b.com\"}");
    engine.execute("INSERT_ROW INTO {users} IN {test_db} VALUES {id=12, name=\"Charlie\", email=\"charlie@c.com\"}");

    std::cout << "\n--- 5. Inserting Rows (Logs) ---" << std::endl;
    engine.execute("INSERT_ROW INTO {logs} IN {test_db} VALUES {level=\"INFO\", message=\"System start\"}");
    engine.execute("INSERT_ROW INTO {logs} IN {test_db} VALUES {level=\"WARN\", message=\"Disk low\"}");
    engine.execute("INSERT_ROW INTO {logs} IN {test_db} VALUES {level=\"INFO\", message=\"User logged in\"}");

    std::cout << "\n--- 6. Testing SELECT (Point Query) ---" << std::endl;
    engine.execute("SELECT {*} FROM {users} IN {test_db} WHERE_INDEX_IS {id = 5}");

    std::cout << "\n--- 7. Testing SELECT (Not Found) ---" << std::endl;
    engine.execute("SELECT {name} FROM {users} IN {test_db} WHERE_INDEX_IS {id = 99}");

    std::cout << "\n--- 8. Testing SELECT (Default Index) ---" << std::endl;
    engine.execute("SELECT {*} FROM {logs} IN {test_db} WHERE_INDEX_IS {row_num = 2}");

    std::cout << "\n--- 9. Testing SELECT (Range Query) ---" << std::endl;
    engine.execute("SELECT {name} FROM {users} IN {test_db} WHERE_INDEX_IS {id > 5 AND id < 15}");

    std::cout << "\n--- 10. Testing Other Commands (Stubs) ---" << std::endl;
    engine.execute("UPDATE_ROW {users} IN {test_db} SET {name=\"Robert\"} WHERE_INDEX_IS {id = 10}");
    engine.execute("DELETE_ROW FROM {users} IN {test_db} WHERE_INDEX_IS {id = 12}");

    std::cout << "\n--- 11. Testing Parse Error (Bad Delimiter) ---" << std::endl;
    engine.execute("SELECT <name> FROM <users> IN <test_db>");

    std::cout << "\n--- 12. Testing Parse Error (Bad Keyword) ---" << std::endl;
    engine.execute("SELECT {name} FROM {users} WHERE {id = 10}");

    return 0;
}
