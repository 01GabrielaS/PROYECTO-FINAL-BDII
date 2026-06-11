#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <map>
#include <set>

// ESTRUCTURAS DE CONTROL DE BAJO NIVEL
struct RecordID {
    int plato;
    int cara;
    int pista;
    int sector;
    long long lba;
    long long offset_inicio;
    long long offset_fin;
};

struct Attribute {
    std::string name;
    std::string type; // INT, FLOAT, CHAR
    int size;
};

struct TableSchema {
    std::string name;
    std::vector<Attribute> attributes;
    int record_size;
};

// NODO DEL ÁRBOL AVL COMPATIBLE CON DUPLICADOS
struct AVLNode {
    std::string key;
    std::vector<RecordID> records; // Almacena múltiples ubicaciones para claves idénticas
    int height;
    AVLNode* left;
    AVLNode* right;

    AVLNode(std::string k, RecordID rid) {
        key = k;
        records.push_back(rid);
        height = 1;
        left = nullptr;
        right = nullptr;
    }
};

// CLASE ADMINISTRADORA DEL HARDWARE VIRTUAL
class VirtualDisk {
private:
    int platos;
    int pistas;
    int sectores;
    int bytes_sector;
    long long total_sectores;
    long long capacidad_total;

public:
    VirtualDisk() : platos(0), pistas(0), sectores(0), bytes_sector(0), total_sectores(0), capacidad_total(0) {}

    bool configure(int p, int t, int s, int b) {
        if (p <= 0 || t <= 0 || s <= 0 || b <= 0) return false;
        platos = p;
        pistas = t;
        sectores = s;
        bytes_sector = b;
        total_sectores = (long long)p * 2 * t * s;
        capacidad_total = total_sectores * b;
        return true;
    }

    long long getTotalSectors() const { return total_sectores; }
    long long getCapacidadTotal() const { return capacidad_total; }
    int getBytesSector() const { return bytes_sector; }

    RecordID calculatePhysicalAddress(long long lba, int slot, int record_size) {
        RecordID rid;
        rid.lba = lba;
        rid.pista = lba / (2 * platos * sectores);
        rid.cara = (lba % (2 * platos * sectores)) / sectores;
        rid.sector = (lba % sectores) + 1;
        rid.plato = rid.cara / 2;
        rid.offset_inicio = (lba * bytes_sector) + (slot * record_size);
        rid.offset_fin = rid.offset_inicio + record_size - 1;
        return rid;
    }
};

// FUNCIONES AUXILIARES DEL ÁRBOL AVL
int getHeight(AVLNode* n) { return n == nullptr ? 0 : n->height; }
int getBalance(AVLNode* n) { return n == nullptr ? 0 : getHeight(n->left) - getHeight(n->right); }

AVLNode* rightRotate(AVLNode* y) {
    AVLNode* x = y->left;
    AVLNode* T2 = x->right;
    x->right = y;
    y->left = T2;
    y->height = std::max(getHeight(y->left), getHeight(y->right)) + 1;
    x->height = std::max(getHeight(x->left), getHeight(x->right)) + 1;
    return x;
}

AVLNode* leftRotate(AVLNode* x) {
    AVLNode* y = x->right;
    AVLNode* T2 = y->left;
    y->left = x;
    x->right = T2;
    x->height = std::max(getHeight(x->left), getHeight(x->right)) + 1;
    y->height = std::max(getHeight(y->left), getHeight(y->right)) + 1;
    return y;
}

AVLNode* insertAVL(AVLNode* node, std::string key, RecordID rid) {
    if (node == nullptr) return new AVLNode(key, rid);

    if (key == node->key) {
        node->records.push_back(rid); // Manejo de colisión/duplicado
        return node;
    }

    // Comparación numérica adaptativa si aplica, de lo contrario lexicográfica
    double k_num, node_num;
    std::stringstream ss1(key), ss2(node->key);
    if ((ss1 >> k_num) && (ss2 >> node_num)) {
        if (k_num < node_num) node->left = insertAVL(node->left, key, rid);
        else node->right = insertAVL(node->right, key, rid);
    } else {
        if (key < node->key) node->left = insertAVL(node->left, key, rid);
        else node->right = insertAVL(node->right, key, rid);
    }

    node->height = 1 + std::max(getHeight(node->left), getHeight(node->right));
    int balance = getBalance(node);

    if (balance > 1) {
        std::stringstream ss_l(node->left->key);
        double left_num;
        bool num = (ss1 >> k_num) && (ss_l >> left_num);
        if ((num && k_num < left_num) || (!num && key < node->left->key)) {
            return rightRotate(node);
        }
        if ((num && k_num > left_num) || (!num && key > node->left->key)) {
            node->left = leftRotate(node->left);
            return rightRotate(node);
        }
    }
    if (balance < -1) {
        std::stringstream ss_r(node->right->key);
        double right_num;
        bool num = (ss1 >> k_num) && (ss_r >> right_num);
        if ((num && k_num > right_num) || (!num && key > node->right->key)) {
            return leftRotate(node);
        }
        if ((num && k_num < right_num) || (!num && key < node->right->key)) {
            node->right = rightRotate(node->right);
            return leftRotate(node);
        }
    }
    return node;
}

// OBTENCIÓN DE MÉTRICAS OPERATIVAS DE BÚSQUEDA
void collectExact(AVLNode* root, std::string key, std::vector<RecordID>& results, int& nodes_visited) {
    if (root == nullptr) return;
    nodes_visited++;

    if (root->key == key) {
        results.insert(results.end(), root->records.begin(), root->records.end());
        return;
    }

    double k_num, node_num;
    std::stringstream ss1(key), ss2(root->key);
    if ((ss1 >> k_num) && (ss2 >> node_num)) {
        if (k_num < node_num) collectExact(root->left, key, results, nodes_visited);
        else collectExact(root->right, key, results, nodes_visited);
    } else {
        if (key < root->key) collectExact(root->left, key, results, nodes_visited);
        else collectExact(root->right, key, results, nodes_visited);
    }
}

void collectRange(AVLNode* root, std::string min_k, std::string max_k, std::vector<RecordID>& results, int& nodes_visited) {
    if (root == nullptr) return;
    nodes_visited++;

    double val_num, min_num, max_num;
    std::stringstream ss_v(root->key), ss_min(min_k), ss_max(max_k);
    bool is_num = (ss_v >> val_num) && (ss_min >> min_num) && (ss_max >> max_num);

    if (is_num) {
        if (val_num >= min_num) collectRange(root->left, min_k, max_k, results, nodes_visited);
        if (val_num >= min_num && val_num <= max_num) {
            results.insert(results.end(), root->records.begin(), root->records.end());
        }
        if (val_num <= max_num) collectRange(root->right, min_k, max_k, results, nodes_visited);
    } else {
        if (root->key >= min_k) collectRange(root->left, min_k, max_k, results, nodes_visited);
        if (root->key >= min_k && root->key <= max_k) {
            results.insert(results.end(), root->records.begin(), root->records.end());
        }
        if (root->key <= max_k) collectRange(root->right, min_k, max_k, results, nodes_visited);
    }
}

int calculateTreeHeight(AVLNode* node) {
    return node == nullptr ? 0 : node->height;
}

// MOTOR CENTRAL DE PERSISTENCIA Y CONSULTAS
class DatabaseEngine {
private:
    VirtualDisk disk;
    TableSchema schema;
    bool hardware_configured = false;
    bool schema_created = false;
    long long next_lba = 0;
    int next_slot = 0;
    int records_per_sector = 0;

    std::vector<std::string> splitCSVRow(const std::string& line) {
        std::vector<std::string> row;
        std::stringstream ss(line);
        std::string cell;
        while (std::getline(ss, cell, ',')) {
            // Limpieza básica de retornos de carro
            if (!cell.empty() && cell.back() == '\r') cell.pop_back();
            row.push_back(cell);
        }
        return row;
    }

public:
    void executeConfig(int p, int t, int s, int b) {
        if (disk.configure(p, t, s, b)) {
            hardware_configured = true;
            std::ofstream f("disk.bin", std::ios::binary | std::ios::trunc);
            std::vector<char> empty_sector(b, 0);
            for (long long i = 0; i < disk.getTotalSectors(); ++i) {
                f.write(empty_sector.data(), b);
            }
            f.close();

            // Persistir archivo de configuración de control
            std::ofstream meta("hardware.cfg");
            meta << p << " " << t << " " << s << " " << b << "\n";
            meta.close();

            std::cout << "SUCCESS|DISK_INITIALIZED|CAPACITY_BYTES:" << disk.getCapacidadTotal() 
                      << "|TOTAL_SECTORS:" << disk.getTotalSectors() << "\n";
        } else {
            std::cout << "ERROR|INVALID_HARDWARE_PARAMETERS\n";
        }
    }

    void loadHardwareConfig() {
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

    void createTable(const std::string& name, const std::vector<std::string>& params) {
        if (!hardware_configured) {
            std::cout << "ERROR|HARDWARE_NOT_INITIALIZED\n";
            return;
        }
        schema.name = name;
        schema.record_size = 1; // 1 byte para el flag de estado (1=Activo, 0=Borrado)
        
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
        
        // Guardar esquema estructurado para lectura posterior
        std::ofstream sf("schema.cfg");
        sf << schema.name << " " << schema.record_size << "\n";
        for (const auto& attr : schema.attributes) {
            sf << attr.name << " " << attr.type << " " << attr.size << "\n";
        }
        sf.close();

        std::cout << "SUCCESS|TABLE_CREATED|RECORD_SIZE_BYTES:" << schema.record_size 
                  << "|RECORDS_PER_SECTOR:" << records_per_sector << "\n";
    }

    void loadSchema() {
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

    void loadCSV(const std::string& csv_path) {
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
        // Leer cabecera descriptiva
        if (!std::getline(csv, line)) return;
        std::vector<std::string> headers = splitCSVRow(line);

        long long records_inserted = 0;
        long long records_rejected = 0;

        while (std::getline(csv, line)) {
            if (line.empty()) continue;
            std::vector<std::string> tokens = splitCSVRow(line);
            if (tokens.size() < schema.attributes.size()) {
                records_rejected++;
                continue;
            }

            // Validar espacio libre en geometría física
            if (next_lba >= disk.getTotalSectors()) {
                std::cout << "OVERFLOW|DISK_FULL|INSERTED:" << records_inserted 
                          << "|REJECTED_BY_SPACE:" << (records_rejected + 1) << "\n";
                binary_file.close();
                csv.close();
                return;
            }

            // Construir buffer binario del registro estático
            std::vector<char> record_buffer(schema.record_size, 0);
            record_buffer[0] = 1; // Flag de registro activo lógico
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
                } else { // Tipo CHAR(N)
                    std::string clean_str = (val.length() > (size_t)attr.size) ? val.substr(0, attr.size) : val;
                    std::memcpy(&record_buffer[current_offset], clean_str.c_str(), clean_str.length());
                }
                current_offset += attr.size;
            }

            // Calcular Offsets absolutos e inyectar en archivo binario
            RecordID rid = disk.calculatePhysicalAddress(next_lba, next_slot, schema.record_size);
            binary_file.seekp(rid.offset_inicio);
            binary_file.write(record_buffer.data(), schema.record_size);

            records_inserted++;

            // Mover punteros de asignación física secuencial
            next_slot++;
            if (next_slot >= records_per_sector) {
                next_slot = 0;
                next_lba++; // Saltar al siguiente sector lógico
            }
        }

        binary_file.close();
        csv.close();

        // Persistir punteros de almacenamiento secuencial para consistencia
        std::ofstream state("disk_state.dat");
        state << next_lba << " " << next_slot << "\n";
        state.close();

        std::cout << "SUCCESS|LOAD_COMPLETE|TOTAL_INSERTED:" << records_inserted 
                  << "|OCCUPIED_SECTORS:" << (next_slot > 0 ? next_lba + 1 : next_lba) << "\n";
    }

    void loadState() {
        std::ifstream state("disk_state.dat");
        if (state.is_open()) {
            state >> next_lba >> next_slot;
            state.close();
        }
    }

    // CONSTRUCCIÓN DEL ÍNDICE EN MEMORIA RAM
    AVLNode* buildIndex(const std::string& target_column) {
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
                if (flag != 1) continue; // Registro borrado u omitido

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
                    // Eliminar ceros binarios residuales
                    extracted_key.erase(std::find(extracted_key.begin(), extracted_key.end(), '\0'), extracted_key.end());
                }

                root = insertAVL(root, extracted_key, rid);
            }
        }
        binary_file.close();
        return root;
    }

    // IMPRESIÓN ASOCIADA A REGISTROS DE FORMA ALINEADA NATIVA
    void printRecordData(long long offset_inicio) {
        std::ifstream f("disk.bin", std::ios::binary);
        f.seekg(offset_inicio + 1); // Saltar flag de estado
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

    void executeQueryExact(const std::string& column, const std::string& value) {
        AVLNode* index_root = buildIndex(column);
        if (index_root == nullptr) {
            std::cout << "RESULT_STATUS|NOT_FOUND|ALTURA_INDICE:0|NODOS_VISITADOS:0|IO_AVL:0|IO_SECUENCIAL:0\n";
            return;
        }

        std::vector<RecordID> matching_records;
        int nodes_visited = 0;
        collectExact(index_root, value, matching_records, nodes_visited);

        // Cálculo analítico de accesos I/O simulados
        std::set<long long> unique_avl_sectors;
        for (const auto& rid : matching_records) {
            unique_avl_sectors.insert(rid.lba);
        }
        long long io_avl = unique_avl_sectors.size();
        long long io_secuencial = (next_slot > 0) ? next_lba + 1 : next_lba;

        std::cout << "METRICS_START\n";
        std::cout << "STATUS:" << (matching_records.empty() ? "NOT_FOUND" : "SUCCESS") << "\n";
        std::cout << "ALTURA_INDICE:" << calculateTreeHeight(index_root) << "\n";
        std::cout << "NODOS_VISITADOS:" << nodes_visited << "\n";
        std::cout << "IO_AVL:" << io_avl << "\n";
        std::cout << "IO_SECUENCIAL:" << io_secuencial << "\n";
        std::cout << "METRICS_END\n";

        std::cout << "DATA_START\n";
        for (const auto& rid : matching_records) {
            std::cout << "[PLATO:" << rid.plato << ",CARA:" << rid.cara 
                      << ",PISTA:" << rid.pista << ",SECTOR:" << rid.sector 
                      << ",LBA:" << rid.lba << ",START:" << rid.offset_inicio 
                      << ",END:" << rid.offset_fin << "] -> ";
            printRecordData(rid.offset_inicio);
            std::cout << "\n";
        }
        std::cout << "DATA_END\n";
    }

    void executeQueryRange(const std::string& column, const std::string& min_v, const std::string& max_v) {
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

        std::cout << "METRICS_START\n";
        std::cout << "STATUS:" << (matching_records.empty() ? "NOT_FOUND" : "SUCCESS") << "\n";
        std::cout << "ALTURA_INDICE:" << calculateTreeHeight(index_root) << "\n";
        std::cout << "NODOS_VISITADOS:" << nodes_visited << "\n";
        std::cout << "IO_AVL:" << io_avl << "\n";
        std::cout << "IO_SECUENCIAL:" << io_secuencial << "\n";
        std::cout << "METRICS_END\n";

        std::cout << "DATA_START\n";
        for (const auto& rid : matching_records) {
            std::cout << "[PLATO:" << rid.plato << ",CARA:" << rid.cara 
                      << ",PISTA:" << rid.pista << ",SECTOR:" << rid.sector 
                      << ",LBA:" << rid.lba << ",START:" << rid.offset_inicio 
                      << ",END:" << rid.offset_fin << "] -> ";
            printRecordData(rid.offset_inicio);
            std::cout << "\n";
        }
        std::cout << "DATA_END\n";
    }
};

// PUNTO DE ENTRADA ADAPTATIVO PARA SUBPROCESOS (NODE.JS / PYTHON)
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