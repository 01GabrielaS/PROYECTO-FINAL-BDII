#include "MotorDB.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <set>
#include <algorithm>

std::vector<std::string> DatabaseEngine::splitCSVRow(const std::string& line) {
    std::vector<std::string> row;
    std::stringstream ss(line);
    std::string cell;
    while (std::getline(ss, cell, ',')) {
        if (!cell.empty() && cell.back() == '\r') cell.pop_back();
        row.push_back(cell);
    }
    return row;
}

void DatabaseEngine::executeConfig(int p, int t, int s, int b) {
    if (disk.configure(p, t, s, b)) {
        hardware_configured = true;
        std::ofstream f("disk.bin", std::ios::binary | std::ios::trunc);
        std::vector<char> empty_sector(b, 0);
        for (long long i = 0; i < disk.getTotalSectors(); ++i) {
            f.write(empty_sector.data(), b);
        }
        f.close();

        std::ofstream meta("hardware.cfg");
        meta << p << " " << t << " " << s << " " << b << "\n";
        meta.close();

        std::cout << "SUCCESS|DISK_INITIALIZED|CAPACITY_BYTES:" << disk.getCapacidadTotal() 
                  << "|TOTAL_SECTORS:" << disk.getTotalSectors() << "\n";
    } else {
        std::cout << "ERROR|INVALID_HARDWARE_PARAMETERS\n";
    }
}

void DatabaseEngine::loadHardwareConfig() {
    std::ifstream meta("hardware.cfg");
    if (meta.is_open()) {
        int p, t, s, b;
        if (meta >> p >> t >> s >> b) {
            disk.configure(p, t, s, b);
            hardware_configured = true;
        }
        meta.close();
    }
}

void DatabaseEngine::createTable(const std::string& name, const std::vector<std::string>& params) {
    if (!hardware_configured) {
        std::cout << "ERROR|HARDWARE_NOT_INITIALIZED\n";
        return;
    }
    schema.name = name;
    schema.record_size = 1; // 1 byte flag
    
    for (const auto& p : params) {
        std::stringstream ss(p);
        std::string attr_name, attr_type;
        std::getline(ss, attr_name, ':');
        std::getline(ss, attr_type, ':');

        Attribute attr;
        attr.name = attr_name;
        attr.type = attr_type;
        if (attr_type == "INT") attr.size = 4;
        else if (attr_type == "FLOAT") attr.size = 4;
        else if (attr_type.find("CHAR") != std::string::npos) {
            int start = attr_type.find('(') + 1;
            int end = attr_type.find(')');
            attr.size = std::stoi(attr_type.substr(start, end - start));
        }
        schema.record_size += attr.size;
        schema.attributes.push_back(attr);
    }

    records_per_sector = disk.getBytesSector() / schema.record_size;
    if (records_per_sector == 0) {
        std::cout << "ERROR|RECORD_SIZE_EXCEEDS_SECTOR_SIZE\n";
        return;
    }
    schema_created = true;
    
    std::ofstream sf("schema.cfg");
    sf << schema.name << " " << schema.record_size << "\n";
    for (const auto& attr : schema.attributes) {
        sf << attr.name << " " << attr.type << " " << attr.size << "\n";
    }
    sf.close();

    std::cout << "SUCCESS|TABLE_CREATED|RECORD_SIZE_BYTES:" << schema.record_size 
              << "|RECORDS_PER_SECTOR:" << records_per_sector << "\n";
}

void DatabaseEngine::loadSchema() {
    std::ifstream sf("schema.cfg");
    if (sf.is_open()) {
        sf >> schema.name >> schema.record_size;
        std::string attr_name, attr_type;
        int attr_size;
        schema.attributes.clear();
        while (sf >> attr_name >> attr_type >> attr_size) {
            Attribute attr = {attr_name, attr_type, attr_size};
            schema.attributes.push_back(attr);
        }
        sf.close();
        if (hardware_configured && disk.getBytesSector() > 0) {
            records_per_sector = disk.getBytesSector() / schema.record_size;
            schema_created = true;
        }
    }
}

void DatabaseEngine::loadCSV(const std::string& csv_path) {
    if (!hardware_configured || !schema_created) {
        std::cout << "ERROR|HARDWARE_OR_SCHEMA_MISSING\n";
        return;
    }

    std::ifstream csv(csv_path);
    if (!csv.is_open()) {
        std::cout << "ERROR|CSV_FILE_NOT_FOUND\n";
        return;
    }

    std::fstream binary_file("disk.bin", std::ios::in | std::ios::out | std::ios::binary);
    if (!binary_file.is_open()) {
        std::cout << "ERROR|DISK_BINARY_NOT_FOUND\n";
        return;
    }

    std::string line;
    if (!std::getline(csv, line)) return;

    long long records_inserted = 0;
    long long records_rejected = 0;

    while (std::getline(csv, line)) {
        if (line.empty()) continue;
        std::vector<std::string> tokens = splitCSVRow(line);
        if (tokens.size() < schema.attributes.size()) {
            records_rejected++;
            continue;
        }

        if (next_lba >= disk.getTotalSectors()) {
            std::cout << "OVERFLOW|DISK_FULL|INSERTED:" << records_inserted 
                      << "|REJECTED_BY_SPACE:" << (records_rejected + 1) << "\n";
            binary_file.close();
            csv.close();
            return;
        }

        std::vector<char> record_buffer(schema.record_size, 0);
        record_buffer[0] = 1; // Active flag
        int current_offset = 1;

        for (size_t i = 0; i < schema.attributes.size(); ++i) {
            const auto& attr = schema.attributes[i];
            std::string val = tokens[i];

            if (attr.type == "INT") {
                int int_val = std::stoi(val);
                std::memcpy(&record_buffer[current_offset], &int_val, 4);
            } else if (attr.type == "FLOAT") {
                float float_val = std::stof(val);
                std::memcpy(&record_buffer[current_offset], &float_val, 4);
            } else {
                std::string clean_str = (val.length() > (size_t)attr.size) ? val.substr(0, attr.size) : val;
                std::memcpy(&record_buffer[current_offset], clean_str.c_str(), clean_str.length());
            }
            current_offset += attr.size;
        }

        RecordID rid = disk.calculatePhysicalAddress(next_lba, next_slot, schema.record_size);
        binary_file.seekp(rid.offset_inicio);
        binary_file.write(record_buffer.data(), schema.record_size);

        records_inserted++;
        next_slot++;
        if (next_slot >= records_per_sector) {
            next_slot = 0;
            next_lba++;
        }
    }

    binary_file.close();
    csv.close();

    std::ofstream state("disk_state.dat");
    state << next_lba << " " << next_slot << "\n";
    state.close();

    std::cout << "SUCCESS|LOAD_COMPLETE|TOTAL_INSERTED:" << records_inserted 
              << "|OCCUPIED_SECTORS:" << (next_slot > 0 ? next_lba + 1 : next_lba) << "\n";
}

void DatabaseEngine::insertManual(const std::vector<std::string>& values) {
    if (!hardware_configured || !schema_created) {
        std::cout << "ERROR|HARDWARE_OR_SCHEMA_MISSING\n";
        return;
    }
    std::fstream binary_file("disk.bin", std::ios::in | std::ios::out | std::ios::binary);
    if (!binary_file.is_open()) {
        std::cout << "ERROR|DISK_BINARY_NOT_FOUND\n";
        return;
    }
    if (next_lba >= disk.getTotalSectors()) {
        std::cout << "OVERFLOW|DISK_FULL\n";
        binary_file.close();
        return;
    }

    std::vector<char> record_buffer(schema.record_size, 0);
    record_buffer[0] = 1;
    int current_offset = 1;

    // Nota: El app.js envía campos originales (sin el ROW_ID autoincremental explícito en el vector de envío manual)
    // El motor asigna internamente el índice basándose en el estado acumulado histórico total
    int simulated_id = (next_lba * records_per_sector) + next_slot;

    for (size_t i = 0; i < schema.attributes.size(); ++i) {
        const auto& attr = schema.attributes[i];
        
        // Inyectar automáticamente el ID si el campo corresponde a la llave primaria del sistema
        if (attr.name == "ROW_ID_SISTEMA") {
            std::memcpy(&record_buffer[current_offset], &simulated_id, 4);
            current_offset += attr.size;
            continue;
        }

        // Mapeo seguro contra el set de datos que viene por parámetros limpios de consola
        size_t param_index = (schema.attributes[0].name == "ROW_ID_SISTEMA") ? i - 1 : i;
        std::string val = (param_index < values.size()) ? values[param_index] : "0";

        if (attr.type == "INT") {
            int int_val = std::stoi(val);
            std::memcpy(&record_buffer[current_offset], &int_val, 4);
        } else if (attr.type == "FLOAT") {
            float float_val = std::stof(val);
            std::memcpy(&record_buffer[current_offset], &float_val, 4);
        } else {
            std::string clean_str = (val.length() > (size_t)attr.size) ? val.substr(0, attr.size) : val;
            std::memcpy(&record_buffer[current_offset], clean_str.c_str(), clean_str.length());
        }
        current_offset += attr.size;
    }

    RecordID rid = disk.calculatePhysicalAddress(next_lba, next_slot, schema.record_size);
    binary_file.seekp(rid.offset_inicio);
    binary_file.write(record_buffer.data(), schema.record_size);
    binary_file.close();

    next_slot++;
    if (next_slot >= records_per_sector) {
        next_slot = 0;
        next_lba++;
    }

    std::ofstream state("disk_state.dat");
    state << next_lba << " " << next_slot << "\n";
    state.close();

    std::cout << "SUCCESS|INSERT_MANUAL|OCCUPIED_SECTORS:" << (next_slot > 0 ? next_lba + 1 : next_lba) << "\n";
}

void DatabaseEngine::loadState() {
    std::ifstream state("disk_state.dat");
    if (state.is_open()) {
        state >> next_lba >> next_slot;
        state.close();
    }
}

AVLNode* DatabaseEngine::buildIndex(const std::string& target_column) {
    loadHardwareConfig();
    loadSchema();
    loadState();

    int target_idx = -1;
    int target_offset = 1;
    for (size_t i = 0; i < schema.attributes.size(); ++i) {
        if (schema.attributes[i].name == target_column) {
            target_idx = i;
            break;
        }
        target_offset += schema.attributes[i].size;
    }

    if (target_idx == -1) return nullptr;

    AVLNode* root = nullptr;
    std::ifstream binary_file("disk.bin", std::ios::binary);
    if (!binary_file.is_open()) return nullptr;

    long long max_occupied_lba = (next_slot > 0) ? next_lba : (next_lba - 1);
    if (max_occupied_lba < 0) max_occupied_lba = 0;

    for (long long lba = 0; lba <= max_occupied_lba; ++lba) {
        for (int slot = 0; slot < records_per_sector; ++slot) {
            if (lba == next_lba && slot >= next_slot) break;

            RecordID rid = disk.calculatePhysicalAddress(lba, slot, schema.record_size);
            binary_file.seekg(rid.offset_inicio);
            
            char flag;
            binary_file.read(&flag, 1);
            if (flag != 1) continue;

            binary_file.seekg(rid.offset_inicio + target_offset);
            std::string extracted_key = "";
            const auto& attr = schema.attributes[target_idx];

            if (attr.type == "INT") {
                int val;
                binary_file.read(reinterpret_cast<char*>(&val), 4);
                extracted_key = std::to_string(val);
            } else if (attr.type == "FLOAT") {
                float val;
                binary_file.read(reinterpret_cast<char*>(&val), 4);
                extracted_key = std::to_string(val);
            } else {
                std::vector<char> buf(attr.size, 0);
                binary_file.read(buf.data(), attr.size);
                extracted_key = std::string(buf.data());
                extracted_key.erase(std::find(extracted_key.begin(), extracted_key.end(), '\0'), extracted_key.end());
            }

            root = insertAVL(root, extracted_key, rid);
        }
    }
    binary_file.close();
    return root;
}

void DatabaseEngine::printRecordData(long long offset_inicio) {
    std::ifstream f("disk.bin", std::ios::binary);
    f.seekg(offset_inicio + 1);
    for (size_t i = 0; i < schema.attributes.size(); ++i) {
        const auto& attr = schema.attributes[i];
        if (attr.type == "INT") {
            int v; f.read(reinterpret_cast<char*>(&v), 4);
            std::cout << v;
        } else if (attr.type == "FLOAT") {
            float v; f.read(reinterpret_cast<char*>(&v), 4);
            std::cout << v;
        } else {
            std::vector<char> buf(attr.size, 0);
            f.read(buf.data(), attr.size);
            std::string s(buf.data());
            s.erase(std::find(s.begin(), s.end(), '\0'), s.end());
            std::cout << s;
        }
        if (i < schema.attributes.size() - 1) std::cout << ",";
    }
    f.close();
}

void DatabaseEngine::executeQueryExact(const std::string& column, const std::string& value) {
    AVLNode* index_root = buildIndex(column);
    if (index_root == nullptr) {
        std::cout << "RESULT_STATUS|NOT_FOUND|ALTURA_INDICE:0|NODOS_VISITADOS:0|IO_AVL:0|IO_SECUENCIAL:0\n";
        return;
    }

    std::vector<RecordID> matching_records;
    int nodes_visited = 0;
    collectExact(index_root, value, matching_records, nodes_visited);

    std::set<long long> unique_avl_sectors;
    for (const auto& rid : matching_records) {
        unique_avl_sectors.insert(rid.lba);
    }
    long long io_avl = unique_avl_sectors.size();
    long long io_secuencial = (next_slot > 0) ? next_lba + 1 : next_lba;

    std::cout << "IO_AVL:" << io_avl << "\n";
    std::cout << "IO_SECUENCIAL:" << io_secuencial << "\n";
    std::cout << "DATA_START\n";
    for (const auto& rid : matching_records) {
        std::cout << "[PLATO: " << rid.plato << " (CARA: " << rid.cara 
                  << ") | PISTA: " << rid.pista << " | SECTOR: " << rid.sector 
                  << " [Offsets: " << rid.offset_inicio << "B - " << rid.offset_fin << "B]] -> {";
        
        // Convertir salida plana a estructura JSON esperada por app.js
        std::cout << "\"ROW_ID_SISTEMA\":" << (rid.offset_inicio / schema.record_size) << ",";
        printRecordData(rid.offset_inicio);
        std::cout << "}\n";
    }
    std::cout << "DATA_END\n";
}

void DatabaseEngine::executeQueryRange(const std::string& column, const std::string& min_v, const std::string& max_v) {
    AVLNode* index_root = buildIndex(column);
    if (index_root == nullptr) {
        std::cout << "RESULT_STATUS|NOT_FOUND|ALTURA_INDICE:0|NODOS_VISITADOS:0|IO_AVL:0|IO_SECUENCIAL:0\n";
        return;
    }

    std::vector<RecordID> matching_records;
    int nodes_visited = 0;
    collectRange(index_root, min_v, max_v, matching_records, nodes_visited);

    std::set<long long> unique_avl_sectors;
    for (const auto& rid : matching_records) {
        unique_avl_sectors.insert(rid.lba);
    }
    long long io_avl = unique_avl_sectors.size();
    long long io_secuencial = (next_slot > 0) ? next_lba + 1 : next_lba;

    std::cout << "IO_AVL:" << io_avl << "\n";
    std::cout << "IO_SECUENCIAL:" << io_secuencial << "\n";
    std::cout << "DATA_START\n";
    for (const auto& rid : matching_records) {
        std::cout << "[PLATO: " << rid.plato << " (CARA: " << rid.cara 
                  << ") | PISTA: " << rid.pista << " | SECTOR: " << rid.sector 
                  << " [Offsets: " << rid.offset_inicio << "B - " << rid.offset_fin << "B]] -> {";
        printRecordData(rid.offset_inicio);
        std::cout << "}\n";
    }
    std::cout << "DATA_END\n";
}