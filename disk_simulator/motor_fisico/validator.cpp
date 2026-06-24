#include "validator.h"
#include <numeric>

// ─────────────────────────────────────────────────────────────────
//  Capa 1 — validación al definir el esquema (DDL)
//
//  Fórmula exacta del PDF:
//    capacidad_total = P × 2 × T × S × bytes_útiles_por_sector
//
//  Nota: usamos usable_capacity() de DiskGeometry que ya
//  implementa exactamente esa fórmula.
// ─────────────────────────────────────────────────────────────────
void Validator::validate_ddl(uint32_t            record_size,
                             const DiskGeometry& geometry)
{
    uint64_t capacidad_total = geometry.usable_capacity();

    if (static_cast<uint64_t>(record_size) > capacidad_total)
        throw DDLCapacityError(record_size, capacidad_total);
}

// ─────────────────────────────────────────────────────────────────
//  Capa 2 — validación antes de cada INSERT individual
//
//  Fórmula exacta del PDF:
//    sectores_necesarios = ceil(tamaño_registro / bytes_útiles)
//    sectores_libres     = contar_ceros(Bitmap)
// ─────────────────────────────────────────────────────────────────
void Validator::validate_insert(uint32_t            record_size,
                                const DiskGeometry& geometry,
                                const FreeBitmap&   bitmap)
{
    uint32_t sectores_necesarios = geometry.sectors_needed(record_size);
    uint32_t sectores_libres     = bitmap.count_free();

    if (sectores_necesarios > sectores_libres)
        throw InsertSpaceError(sectores_necesarios, sectores_libres);
}

// ─────────────────────────────────────────────────────────────────
//  Helper: suma de bytes de todos los campos
// ─────────────────────────────────────────────────────────────────
uint32_t Validator::compute_record_size(const std::vector<uint32_t>& field_sizes) {
    return std::accumulate(field_sizes.begin(), field_sizes.end(), 0u);
}

//Recibe el n°de sectores ya calculado con integridad de campo
void Validator::validate_insert_sectors(uint32_t          sectors_needed,
                                        const FreeBitmap& bitmap)
{
    uint32_t sectores_libres = bitmap.count_free();

    if (sectors_needed > sectores_libres)
        throw InsertSpaceError(sectors_needed, sectores_libres);
}
