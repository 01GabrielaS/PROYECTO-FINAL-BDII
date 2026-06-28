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

class DiskEngine {
public:
    DiskEngine();
    ~DiskEngine() = default;

    // ── Configuración de hardware ──────────────────────────────
    void configure(uint32_t platters,
                   uint32_t tracks,
                   uint32_t sectors_per_track,
                   uint32_t bytes_per_sector,
                   const std::string& disk_filepath = "disk.bin");

    bool is_configured() const { return configured_; }

    // ── Validación capa 1 (Diego la llama al hacer CREATE TABLE)
    void validate_schema(const std::vector<uint32_t>& field_sizes) const;
    void validate_schema(uint32_t record_size) const;

    // ── INSERT ─────────────────────────────────────────────────
    WriteResult insert(RecordID        record_id,
                       const uint8_t* data,
                       uint32_t        size);

    WriteResult insert_fields(RecordID record_id,
    const std::vector<std::pair<const uint8_t*, uint32_t>>& fields);

    uint32_t sectors_needed_for_fields(
    const std::vector<uint32_t>& field_sizes) const;

    // ── READ ───────────────────────────────────────────────────
    ReadResult  read(uint32_t start_lba);

    // ── DELETE lógico ──────────────────────────────────────────
    void remove(uint32_t start_lba);

    // ── Info del disco (para Panel A de la UI) ─────────────────
    uint32_t free_sectors()  const;
    uint32_t used_sectors()  const;
    uint32_t total_sectors() const;
    double   usage_percent() const;   

    // TAREA 1: Formateador de cadena física CHS (Requerido por Gabi para Panel B)
    std::string format_chs_chain(const WriteResult& wr, uint32_t record_bytes) const;

    uint32_t sectors_needed(uint32_t record_size) const;
    std::string geometry_info() const;
    std::string bitmap_info()   const;

    const DiskGeometry& geometry() const { return *geometry_; }
    const FreeBitmap&   bitmap()   const { return *bitmap_;   }

private:
    bool configured_ = false;

    std::unique_ptr<DiskGeometry> geometry_;
    std::unique_ptr<FreeBitmap>   bitmap_;
    std::unique_ptr<DiskWriter>   writer_;

    void check_configured() const;
};