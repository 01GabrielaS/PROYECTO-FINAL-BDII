#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "disk_geometry.h"
#include "free_bitmap.h"
#include "disk_writer.h"
#include "validator.h"
#include "record_id.h"

// ─────────────────────────────────────────────────────────────────
//  DiskEngine — fachada pública del motor físico
//
//  Es el ÚNICO archivo que Iair y Diego necesitan incluir.
//  Ellos nunca tocan DiskWriter, FreeBitmap ni DiskGeometry
//  directamente; todo pasa por aquí.
//
//  Flujo típico:
//    1. DiskEngine engine;
//    2. engine.configure(P, T, S, B);         ← Gabriela / UI
//    3. engine.validate_schema(field_sizes);   ← Diego (DDL)
//    4. rid = engine.insert(data, size);       ← Diego (INSERT)
//    5. result = engine.read(start_lba);       ← Iair (búsqueda)
//    6. engine.remove(start_lba);              ← Iair (delete)
// ─────────────────────────────────────────────────────────────────
class DiskEngine {
public:
    DiskEngine();
    ~DiskEngine() = default;

    // ── Configuración de hardware ──────────────────────────────
    // Debe llamarse ANTES de cualquier otra operación.
    // Crea el archivo disk.bin y reinicia el bitmap.
    void configure(uint32_t platters,
                   uint32_t tracks,
                   uint32_t sectors_per_track,
                   uint32_t bytes_per_sector,
                   const std::string& disk_filepath = "disk.bin");

    bool is_configured() const { return configured_; }

    // ── Validación capa 1 (Diego la llama al hacer CREATE TABLE)
    // Lanza DDLCapacityError si el esquema no cabe en el disco.
    void validate_schema(const std::vector<uint32_t>& field_sizes) const;
    void validate_schema(uint32_t record_size) const;

    // ── INSERT ─────────────────────────────────────────────────
    // Valida (capa 2), escribe en disco, devuelve WriteResult.
    // Lanza InsertSpaceError si no hay espacio.
    // Diego llama a esto con los bytes serializados del registro.
    WriteResult insert(RecordID        record_id,
                       const uint8_t*  data,
                       uint32_t        size);

    WriteResult insert_fields( RecordID record_id,
    const std::vector<std::pair<const uint8_t*, uint32_t>>& fields);

    uint32_t sectors_needed_for_fields(
    const std::vector<uint32_t>& field_sizes) const;

    // ── READ ───────────────────────────────────────────────────
    // Iair llama a esto con el start_lba que guardó en el AVL.
    ReadResult  read(uint32_t start_lba);

    // ── DELETE lógico ──────────────────────────────────────────
    // Iair llama a esto antes de sacar el nodo del AVL.
    void remove(uint32_t start_lba);

    // ── Info del disco (para Panel A de la UI) ─────────────────
    uint32_t free_sectors()  const;
    uint32_t used_sectors()  const;
    uint32_t total_sectors() const;
    double   usage_percent() const;   // 0.0 – 100.0

    // Sectores necesarios para un registro de ese tamaño
    uint32_t sectors_needed(uint32_t record_size) const;

    // Resumen legible de la geometría configurada
    std::string geometry_info() const;
    std::string bitmap_info()   const;

    // ── Acceso a los sub-módulos (solo lectura, para tests) ────
    const DiskGeometry& geometry() const { return *geometry_; }
    const FreeBitmap&   bitmap()   const { return *bitmap_;   }

private:
    bool configured_ = false;

    std::unique_ptr<DiskGeometry> geometry_;
    std::unique_ptr<FreeBitmap>   bitmap_;
    std::unique_ptr<DiskWriter>   writer_;

    void check_configured() const;
};
