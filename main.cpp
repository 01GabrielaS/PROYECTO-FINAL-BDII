#include "MotorDB.h"
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "ERROR|COMMAND_REQUIRED\n";
        return 1;
    }

    DatabaseEngine engine;
    std::string command = argv[1];

    if (command == "init") {
        if (argc < 6) return 1;
        engine.executeConfig(std::stoi(argv[2]), std::stoi(argv[3]), std::stoi(argv[4]), std::stoi(argv[5]));
    } 
    else if (command == "create_table") {
        if (argc < 4) return 1;
        std::string table_name = argv[2];
        std::vector<std::string> params;
        for (int i = 3; i < argc; ++i) {
            params.push_back(argv[i]);
        }
        engine.loadHardwareConfig();
        engine.createTable(table_name, params);
    } 
    else if (command == "load") {
        if (argc < 3) return 1;
        engine.loadHardwareConfig();
        engine.loadSchema();
        engine.loadState();
        engine.loadCSV(argv[2]);
    } 
    else if (command == "insert_manual") {
        if (argc < 3) return 1;
        std::vector<std::string> values;
        for (int i = 2; i < argc; ++i) {
            values.push_back(argv[i]);
        }
        engine.loadHardwareConfig();
        engine.loadSchema();
        engine.loadState();
        engine.insertManual(values);
    }
    else if (command == "search") {
        if (argc < 4) return 1;
        engine.executeQueryExact(argv[2], argv[3]);
    } 
    else if (command == "range") {
        if (argc < 5) return 1;
        engine.executeQueryRange(argv[2], argv[3], argv[4]);
    }
    return 0;
}