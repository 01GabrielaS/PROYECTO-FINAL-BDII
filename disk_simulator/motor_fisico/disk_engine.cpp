#include "disk_engine.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>

// ─────────────────────────────────────────────────────────────────
DiskEngine::DiskEngine() : configured_(false) {}

// ─────────────────────────────────────────────────────────────────
void DiskEngine::check_configured() const {
    if (!configured_)
        throw std::logic_error(
            "DiskEngine no configurado. Llame a configure() primero.");
}

// ─────────────────────────────────────────────────────────────────
//  configure — inicializa todos los sub-módulos en orden
// ─────────────────────────────────────────────────────────────────
void DiskEngine::configure(uint32_t           platters,
                           uint32_t           tracks,
                           uint32_t           sectors_per_track,
                           uint32_t           bytes_per_sector,
                           const std::string& disk_filepath)
{
    // 1. Geometría (valida los parámetros de hardware)
    geometry_ = std::make_unique<DiskGeometry>(
                    platters, tracks, sectors_per_track, bytes_per_sector);

    // 2. Bitmap (todos los sectores libres)
    bitmap_ = std::make_unique<FreeBitmap>(geometry_->total_sectors());

    // 3. DiskWriter (crea disk.bin inicializado con ceros)
    writer_ = std::make_unique<DiskWriter>(
                    disk_filepath, *geometry_, *bitmap_, /*create_new=*/true);

    configured_ = true;
}

// ─────────────────────────────────────────────────────────────────
//  validate_schema — capa 1 (Diego la llama al hacer CREATE TABLE)
// ─────────────────────────────────────────────────────────────────
void DiskEngine::validate_schema(const std::vector<uint32_t>& field_sizes) const {
    check_configured();
    uint32_t record_size = Validator::compute_record_size(field_sizes);
    Validator::validate_ddl(record_size, *geometry_);
}

void DiskEngine::validate_schema(uint32_t record_size) const {
    check_configured();
    Validator::validate_ddl(record_size, *geometry_);
}

// ─────────────────────────────────────────────────────────────────
//  insert — valida capa 2 y escribe en disco
// ─────────────────────────────────────────────────────────────────
WriteResult DiskEngine::insert(RecordID       record_id,
                               const uint8_t* data,
                               uint32_t       size)
{
    check_configured();

    // Capa 2: verifica espacio ANTES de tocar el disco
    // Lanza InsertSpaceError si no hay sectores suficientes
    Validator::validate_insert(size, *geometry_, *bitmap_);

    // Escritura física (con spanning si size > useful_bytes)
    return writer_->write_record(record_id, data, size);
}

// ─────────────────────────────────────────────────────────────────
//  read — lectura siguiendo la cadena de LBAs
// ─────────────────────────────────────────────────────────────────
ReadResult DiskEngine::read(uint32_t start_lba) {
    check_configured();
    return writer_->read_record(start_lba);
}

// ─────────────────────────────────────────────────────────────────
//  remove — eliminación lógica
// ─────────────────────────────────────────────────────────────────
void DiskEngine::remove(uint32_t start_lba) {
    check_configured();
    writer_->delete_record(start_lba);
}

// ─────────────────────────────────────────────────────────────────
//  Info del disco
// ─────────────────────────────────────────────────────────────────
uint32_t DiskEngine::free_sectors()  const { check_configured(); return bitmap_->count_free(); }
uint32_t DiskEngine::used_sectors()  const { check_configured(); return bitmap_->count_used(); }
uint32_t DiskEngine::total_sectors() const { check_configured(); return geometry_->total_sectors(); }

double DiskEngine::usage_percent() const {
    check_configured();
    if (geometry_->total_sectors() == 0) return 0.0;
    return (static_cast<double>(bitmap_->count_used()) /
            geometry_->total_sectors()) * 100.0;
}

uint32_t DiskEngine::sectors_needed(uint32_t record_size) const {
    check_configured();
    return geometry_->sectors_needed(record_size);
}

std::string DiskEngine::geometry_info() const {
    check_configured();
    return geometry_->to_string();
}

std::string DiskEngine::bitmap_info() const {
    check_configured();
    return bitmap_->to_summary();
}
