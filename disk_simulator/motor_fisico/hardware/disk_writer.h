#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include "disk_geometry.h"
#include "free_bitmap.h"
#include "sector_header.h"
#include "../storage/record_id.h"

// ─────────────────────────────────────────────────────────────────
//  Resultado de una escritura: informa al llamador (y al AVL)
//  qué LBAs se usaron y cuántos sectores ocupó el registro.
// ─────────────────────────────────────────────────────────────────
struct WriteResult {
    uint32_t              start_lba;      // LBA del primer sector (el que guarda el AVL)
    uint32_t              num_sectors;    // cantidad de sectores usados
    std::vector<uint32_t> lba_chain;      // cadena completa de LBAs
};

// ─────────────────────────────────────────────────────────────────
//  Resultado de una lectura: devuelve los bytes del registro
//  y la cadena de sectores leídos (para la métrica I/O).
// ─────────────────────────────────────────────────────────────────
struct ReadResult {
    std::vector<uint8_t>  data;           // bytes del registro (sin headers)
    std::vector<uint32_t> lba_chain;      // sectores leídos
    uint32_t              sectors_read;   // == lba_chain.size()
};

// ─────────────────────────────────────────────────────────────────
//  DiskWriter — escritura y lectura física sobre disk.bin
//
//  Implementa los 6 pasos del spanning multi-sector del PDF:
//    1. Calcular sectores necesarios
//    2. Pedir LBAs al bitmap
//    3. Escribir primer fragmento con FLAG_START
//    4. Fragmentos intermedios con FLAG_CONT
//    5. Último fragmento con FLAG_END + NEXT_LBA_NONE
//    6. Bitmap ya marcado por alloc()
//
//  El archivo disk.bin se trata como un array de sectores:
//    offset(lba) = lba * bytes_per_sector
// ─────────────────────────────────────────────────────────────────
class DiskWriter {
public:
    // ── Constructor: abre (o crea) el archivo de disco ─────────
    // Si create_new=true inicializa el archivo con ceros.
    DiskWriter(const std::string& filepath,
               DiskGeometry&      geometry,
               FreeBitmap&        bitmap,
               bool               create_new = false);

    ~DiskWriter();

    // ── Escritura ──────────────────────────────────────────────
    // Escribe 'data' (size bytes) en disco asociándolo a record_id.
    // Aplica spanning si size > useful_bytes().
    // Devuelve la info que el AVL necesita guardar.
    WriteResult write_record(RecordID record_id, const uint8_t*  data, uint32_t        size);
    
    WriteResult write_record_fields(RecordID record_id,const std::vector<std::pair<const uint8_t*, uint32_t>>& fields);

    // Simula el packing sin escribir nada — útil para Capa 2.
    // Recibe solo los tamaños de los campos, no los datos.
    static uint32_t sectors_for_fields(
        uint32_t                     useful_bytes,
        const std::vector<uint32_t>& field_sizes);
                             
    // ── Lectura ────────────────────────────────────────────────
    // Lee el registro que empieza en start_lba siguiendo la cadena.
    ReadResult read_record(uint32_t start_lba);

    // ── Eliminación lógica ─────────────────────────────────────
    // Marca el FLAG del primer sector como FLAG_FREE y libera el
    // bitmap. El AVL se encarga de remover el nodo en RAM.
    void delete_record(uint32_t start_lba);

    // ── Estado del archivo ─────────────────────────────────────
    bool is_open() const { return file_.is_open(); }

private:
    std::string   filepath_;
    DiskGeometry& geometry_;
    FreeBitmap&   bitmap_;
    std::fstream  file_;

    // ── I/O de bajo nivel ──────────────────────────────────────
    // Escribe exactamente bytes_per_sector bytes en el LBA dado
    void write_sector(uint32_t lba, const uint8_t* sector_buf);
    // Lee exactamente bytes_per_sector bytes del LBA dado
    void read_sector (uint32_t lba, uint8_t* sector_buf);

    // Calcula el offset en bytes dentro del archivo para un LBA
    std::streampos offset_of(uint32_t lba) const;
};
