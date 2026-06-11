#ifndef MOTORDB_H
#define MOTORDB_H

#include "DiscoVirtual.h"
#include "ArbolAVL.h"
#include <string>
#include <vector>

class DatabaseEngine {
private:
    VirtualDisk disk;
    TableSchema schema;
    bool hardware_configured = false;
    bool schema_created = false;
    long long next_lba = 0;
    int next_slot = 0;
    int records_per_sector = 0;

    std::vector<std::string> splitCSVRow(const std::string& line);

public:
    void executeConfig(int p, int t, int s, int b);
    void loadHardwareConfig();
    void createTable(const std::string& name, const std::vector<std::string>& params);
    void loadSchema();
    void loadCSV(const std::string& csv_path);
    void loadState();
    
    AVLNode* buildIndex(const std::string& target_column);
    void printRecordData(long long offset_inicio);
    void executeQueryExact(const std::string& column, const std::string& value);
    void executeQueryRange(const std::string& column, const std::string& min_v, const std::string& max_v);
    
    // Método auxiliar para inserción manual unitaria requerida por el app.js
    void insertManual(const std::vector<std::string>& values);
};

#endif // MOTORDB_H