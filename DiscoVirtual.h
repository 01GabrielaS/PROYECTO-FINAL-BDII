#ifndef DISCOVIRTUAL_H
#define DISCOVIRTUAL_H

#include <vector>
#include <string>

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
    VirtualDisk();
    bool configure(int p, int t, int s, int b);
    long long getTotalSectors() const;
    long long getCapacidadTotal() const;
    int getBytesSector() const;
    RecordID calculatePhysicalAddress(long long lba, int slot, int record_size);
};

#endif // DISCOVIRTUAL_H