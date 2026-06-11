#include "DiscoVirtual.h"

VirtualDisk::VirtualDisk() : platos(0), pistas(0), sectores(0), bytes_sector(0), total_sectores(0), capacidad_total(0) {}

bool VirtualDisk::configure(int p, int t, int s, int b) {
    if (p <= 0 || t <= 0 || s <= 0 || b <= 0) return false;
    platos = p;
    pistas = t;
    sectores = s;
    bytes_sector = b;
    total_sectores = (long long)p * 2 * t * s;
    capacidad_total = total_sectores * b;
    return true;
}

long long VirtualDisk::getTotalSectors() const { return total_sectores; }
long long VirtualDisk::getCapacidadTotal() const { return capacidad_total; }
int VirtualDisk::getBytesSector() const { return bytes_sector; }

RecordID VirtualDisk::calculatePhysicalAddress(long long lba, int slot, int record_size) {
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