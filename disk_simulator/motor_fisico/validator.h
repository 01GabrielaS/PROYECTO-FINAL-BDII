#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include "disk_geometry.h"
#include "free_bitmap.h"

// ─────────────────────────────────────────────────────────────────
//  Excepciones tipadas — el motor lanza estas, Diego e Iair
//  las capturan para mostrar el error correcto en consola/UI.
// ─────────────────────────────────────────────────────────────────

// Lanzada por validate_ddl() — capa 1
struct DDLCapacityError : public std::runtime_error {
    uint32_t record_size;      // tamaño del esquema en bytes
    uint64_t total_capacity;   // capacidad neta del disco en bytes

    DDLCapacityError(uint32_t rec_sz, uint64_t cap)
        : std::runtime_error(build_msg(rec_sz, cap))
        , record_size(rec_sz)
        , total_capacity(cap)
    {}

private:
    static std::string build_msg(uint32_t rec_sz, uint64_t cap) {
        return "ERROR DDL: El esquema definido supera la capacidad total del disco.\n"
               "  Tamanio del registro : " + std::to_string(rec_sz)  + " bytes\n"
               "  Capacidad neta disco : " + std::to_string(cap)     + " bytes\n"
               "  Reduzca los campos o reconfigure el hardware.";
    }
};

// Lanzada por validate_insert() — capa 2
struct InsertSpaceError : public std::runtime_error {
    uint32_t sectors_needed;
    uint32_t sectors_free;

    InsertSpaceError(uint32_t needed, uint32_t free_s)
        : std::runtime_error(build_msg(needed, free_s))
        , sectors_needed(needed)
        , sectors_free(free_s)
    {}

private:
    static std::string build_msg(uint32_t needed, uint32_t free_s) {
        return "ERROR INSERT: Espacio insuficiente en disco.\n"
               "  Sectores requeridos : " + std::to_string(needed)  + "\n"
               "  Sectores libres     : " + std::to_string(free_s)  + "\n"
               "  Reduzca los campos o reconfigure el hardware.";
    }
};

// ─────────────────────────────────────────────────────────────────
//  Validator — funciones de validación por capas
//
//  Capa 1 (DDL): se llama UNA VEZ al definir la tabla.
//    Compara el tamaño del registro contra la capacidad TOTAL del
//    disco. Si ni siquiera un registro cabe, bloquea la creación.
//
//  Capa 2 (INSERT): se llama ANTES de cada escritura individual.
//    Compara los sectores necesarios contra los libres en el bitmap.
//    Si no hay espacio, lanza sin tocar el disco ni el bitmap.
// ─────────────────────────────────────────────────────────────────
class Validator {
public:
    // ── Capa 1: validación al definir el esquema (DDL) ─────────
    //
    //   capacidad_total = P × 2 × T × S × useful_bytes
    //   Si record_size > capacidad_total → DDLCapacityError
    //
    // Parámetro record_size: suma de bytes de todos los campos
    // de la tabla (lo calcula Diego con su parser DDL).
    static void validate_ddl(uint32_t           record_size,
                             const DiskGeometry& geometry);

    // ── Capa 2: validación antes de insertar un registro ───────
    //
    //   sectores_necesarios = ceil(record_size / useful_bytes)
    //   sectores_libres     = bitmap.count_free()
    //   Si necesarios > libres → InsertSpaceError
    //
    // No modifica el bitmap. Solo consulta.
    static void validate_insert(uint32_t           record_size,
                                const DiskGeometry& geometry,
                                const FreeBitmap&   bitmap);

    // ── Helper: calcula el tamaño de un esquema desde sus campos ─
    // Recibe un vector con los tamaños en bytes de cada campo.
    // Útil para que Diego no tenga que hacer la suma él mismo.
    static uint32_t compute_record_size(const std::vector<uint32_t>& field_sizes);
};
