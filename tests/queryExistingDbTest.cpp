#include <iostream>
#include "db_engine.hpp"
#include <filesystem>

int main() {
    // --- THIS IS THE "QUERY" PROGRAM ---
    // *** NOTICE: Run it only after running setupDbTest ***

    DatabaseEngine engine;

    std::cout << "\n--- 1. Testing SELECT (Full Scan) ---" << std::endl;
    engine.execute("SELECT {*} FROM {users} IN {test_db}");

    std::cout << "\n--- 2. Testing SELECT (Selective Columns) ---" << std::endl;
    engine.execute("SELECT {name, email} FROM {users} IN {test_db} WHERE_INDEX_IS {id = 10}");

    std::cout << "\n--- 3. Testing SELECT (Range Query) ---" << std::endl;
    engine.execute("SELECT {name} FROM {users} IN {test_db} WHERE_INDEX_IS {id > 6 AND id < 15}");

    std::cout << "\n--- 4. Testing SELECT (String Index) ---" << std::endl;
    engine.execute("SELECT {name, price} FROM {products} IN {string_db} WHERE_INDEX_IS {sku = \"A-100\"}");

    std::cout << "\n--- 5. Testing SELECT (Default Index) ---" << std::endl;
    engine.execute("SELECT {*} FROM {logs} IN {test_db} WHERE_INDEX_IS {row_num = 2}");

    std::cout << "\n--- 6. Testing UPDATE ---" << std::endl;
    engine.execute("UPDATE_ROW {users} IN {test_db} SET {name=\"Robert\", email=\"rob@b.com\"} WHERE_INDEX_IS {id = 5}");

    std::cout << "\n--- 7. Testing SELECT (Confirm Update) ---" << std::endl;
    engine.execute("SELECT {name, email} FROM {users} IN {test_db} WHERE_INDEX_IS {id = 5}");

    std::cout << "\n--- 8. Testing DELETE ---" << std::endl;
    engine.execute("DELETE_ROW FROM {users} IN {test_db} WHERE_INDEX_IS {id = 12}");

    std::cout << "\n--- 9. Testing SELECT (Confirm Delete) ---" << std::endl;
    engine.execute("SELECT {*} FROM {users} IN {test_db} WHERE_INDEX_IS {id = 12}");

    std::cout << "\n--- 10. Testing Full Scan (After Delete) ---" << std::endl;
    std::cout << "(Should only show 10 and 5, skipping the zeroed-out '12')" << std::endl;
    engine.execute("SELECT {*} FROM {users} IN {test_db}");

    std::cout << "\n--- Query Test Complete. ---" << std::endl;
    std::cout << "--- Run './query_db' again to see that the changes (like UPDATE/DELETE) were also persistent. ---" << std::endl;

    return 0;
}
