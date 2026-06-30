#include "disk_writer.h"
#include <cstring>
#include <stdexcept>
#include <iostream>
//  Constructor — abre o crea el archivo disk.bin
DiskWriter::DiskWriter(const std::string& filepath,
                       DiskGeometry&      geometry,
                       FreeBitmap&        bitmap,
                       bool               create_new)
    : filepath_(filepath)
    , geometry_(geometry)
    , bitmap_  (bitmap)
{
    if (create_new) {
        file_.open(filepath, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file_.is_open())
            throw std::runtime_error("No se pudo crear el archivo: " + filepath);

        uint32_t B = geometry_.get_bytes_per_sector();
        std::vector<uint8_t> zeros(B, 0x00);
        for (uint32_t lba = 0; lba < geometry_.total_sectors(); ++lba)
            file_.write(reinterpret_cast<const char*>(zeros.data()), B);

        file_.close();
    }

    file_.open(filepath, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_.is_open())
        throw std::runtime_error("No se pudo abrir el archivo: " + filepath);
}

DiskWriter::~DiskWriter() {
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

//  Helpers de I/O de bajo nivel
std::streampos DiskWriter::offset_of(uint32_t lba) const {
    return static_cast<std::streampos>(lba) * geometry_.get_bytes_per_sector();
}

void DiskWriter::write_sector(uint32_t lba, const uint8_t* sector_buf) {
    geometry_.check_lba(lba);
    file_.seekp(offset_of(lba));
    file_.write(reinterpret_cast<const char*>(sector_buf),
                geometry_.get_bytes_per_sector());
    if (!file_.good())
        throw std::runtime_error("Error de escritura en LBA " + std::to_string(lba));
}

void DiskWriter::read_sector(uint32_t lba, uint8_t* sector_buf) {
    geometry_.check_lba(lba);
    file_.seekg(offset_of(lba));
    file_.read(reinterpret_cast<char*>(sector_buf),
               geometry_.get_bytes_per_sector());
    if (!file_.good())
        throw std::runtime_error("Error de lectura en LBA " + std::to_string(lba));
}

//Uso de spnaning para registros de tamaño mayor
WriteResult DiskWriter::write_record(RecordID       record_id,
                                     const uint8_t* data,
                                     uint32_t       size)
{
    const uint32_t B  = geometry_.get_bytes_per_sector();
    const uint32_t UB = geometry_.useful_bytes(); 

    uint32_t n_sectors = geometry_.sectors_needed(size);   
    std::vector<uint32_t> lbas = bitmap_.alloc(n_sectors); //n° de lbas
    std::vector<uint8_t> sector_buf(B, 0x00);

    uint32_t data_offset = 0;  // cuántos bytes ya escribimos

    for (uint32_t i = 0; i < n_sectors; ++i) {
        uint32_t lba = lbas[i];

        // Determinar FLAG del sector actual
        uint8_t flag;
        if      (n_sectors == 1)    flag = FLAG_START;   // único sector: START = END
        else if (i == 0)            flag = FLAG_START;
        else if (i == n_sectors-1)  flag = FLAG_END;
        else                        flag = FLAG_CONT;
        // NEXT_SECTOR_LBA
        uint32_t next_lba = (i < n_sectors - 1) ? lbas[i + 1] : NEXT_LBA_NONE;
        SectorHeader header(flag, record_id, next_lba);
        uint32_t bytes_left    = size - data_offset; //bytes restantes en sector
        uint32_t bytes_to_copy = (bytes_left < UB) ? bytes_left : UB;

        std::fill(sector_buf.begin(), sector_buf.end(), 0x00);
        header.to_bytes(sector_buf.data());
        std::memcpy(sector_buf.data() + SECTOR_HEADER_SIZE,
                    data + data_offset,
                    bytes_to_copy);

        write_sector(lba, sector_buf.data());

        data_offset += bytes_to_copy;
    }
    WriteResult result;
    result.start_lba   = lbas[0];
    result.num_sectors = n_sectors;
    result.lba_chain   = lbas;
    return result;
}

uint32_t DiskWriter::sectors_for_fields(
    uint32_t                     useful_bytes,
    const std::vector<uint32_t>& field_sizes)
{
    uint32_t sectors = 1;
    uint32_t cursor  = 0;   // bytes usados en el sector actual

    for (uint32_t fs : field_sizes) {
        if (fs == 0) continue;

        if (fs > useful_bytes)
            throw std::runtime_error(
                "sectors_for_fields: campo de " + std::to_string(fs) +
                " bytes supera useful_bytes=" + std::to_string(useful_bytes) +
                ". Redefina el esquema o aumente el tamaño del sector.");

        if (cursor + fs > useful_bytes) {
            // El campo no cabe → cerramos el sector actual
            ++sectors;
            cursor = 0;
        }
        cursor += fs;
    }
    return sectors;
}


WriteResult DiskWriter::write_record_fields(
    RecordID record_id,
    const std::vector<std::pair<const uint8_t*, uint32_t>>& fields)
{
    const uint32_t B  = geometry_.get_bytes_per_sector();
    const uint32_t UB = geometry_.useful_bytes();   // B - SECTOR_HEADER_SIZE

    // ── Paso 1: construir payloads de sectores en RAM ──────────
    //   Cada sector tendrá exactamente UB bytes de payload
    //   (el último puede tener datos reales + ceros de padding).
    std::vector<std::vector<uint8_t>> payloads;
    std::vector<uint8_t> current(UB, 0x00);
    uint32_t cursor = 0;   // bytes escritos en el sector actual

    for (auto& [field_ptr, field_size] : fields) {
        if (field_size == 0) continue;

        // Verificación: ningún campo puede ser mayor que UB
        if (field_size > UB)
            throw std::runtime_error(
                "write_record_fields: campo de " + std::to_string(field_size) +
                " bytes supera useful_bytes=" + std::to_string(UB) +
                ". Redefina el esquema o aumente el tamaño del sector.");

        // Si el campo no cabe en los bytes restantes del sector actual,
        // cerramos ese sector (los bytes sin usar quedan como padding 0x00)
        // y abrimos uno nuevo.
        if (cursor + field_size > UB) {
            payloads.push_back(current);
            current.assign(UB, 0x00);
            cursor = 0;
        }

        // Copiar el campo al sector actual
        std::memcpy(current.data() + cursor, field_ptr, field_size);
        cursor += field_size;
    }

    // Guardar el último sector (puede estar parcialmente lleno → fragmentación interna OK)
    payloads.push_back(current);

    const uint32_t n_sectors = static_cast<uint32_t>(payloads.size());

    // ── Paso 2: pedir N LBAs al bitmap ────────────────────────
    std::vector<uint32_t> lbas = bitmap_.alloc(n_sectors);

    // ── Pasos 3-5: escribir cada sector con su header ──────────
    std::vector<uint8_t> sector_buf(B, 0x00);

    for (uint32_t i = 0; i < n_sectors; ++i) {
        uint32_t lba = lbas[i];

        // FLAG según posición en la cadena
        uint8_t flag;
        if      (n_sectors == 1)    flag = FLAG_START;   // registro de un solo sector
        else if (i == 0)            flag = FLAG_START;
        else if (i == n_sectors-1)  flag = FLAG_END;
        else                        flag = FLAG_CONT;

        uint32_t next_lba = (i < n_sectors - 1) ? lbas[i + 1] : NEXT_LBA_NONE;

        SectorHeader header(flag, record_id, next_lba);

        std::fill(sector_buf.begin(), sector_buf.end(), 0x00);
        header.to_bytes(sector_buf.data());
        std::memcpy(sector_buf.data() + SECTOR_HEADER_SIZE,
                    payloads[i].data(),
                    UB);   // siempre UB bytes (ya paddeado con ceros)

        write_sector(lba, sector_buf.data());
    }
    // bitmap ya marcado como OCUPADO por alloc()

    WriteResult result;
    result.start_lba   = lbas[0];
    result.num_sectors = n_sectors;
    result.lba_chain   = lbas;
    return result;
}

//  lee del primer sector al ultimo
ReadResult DiskWriter::read_record(uint32_t start_lba) {
    const uint32_t B  = geometry_.get_bytes_per_sector();
    const uint32_t UB = geometry_.useful_bytes();

    ReadResult result;
    std::vector<uint8_t> sector_buf(B);

    uint32_t current_lba = start_lba;

    while (true) {
        read_sector(current_lba, sector_buf.data());
        result.lba_chain.push_back(current_lba);
        SectorHeader header = SectorHeader::from_bytes(sector_buf.data());

        // Verificar que el sector no está libre (corrupción / LBA incorrecto)
        if (header.is_free())
            throw std::runtime_error(
                "read_record: sector LBA " + std::to_string(current_lba) +
                " está marcado como libre (posible corrupción)");

        // Copiar la parte de datos de este sector al resultado
        // En el último sector puede haber menos de UB bytes de datos reales,
        // pero los leemos todos (el serializador de Diego sabe el tamaño exacto)
        const uint8_t* data_start = sector_buf.data() + SECTOR_HEADER_SIZE;
        result.data.insert(result.data.end(), data_start, data_start + UB);

        // ¿Es el último sector?
        if (header.is_last() || header.is_end() || header.is_start())
        {
            // is_start() cubre el caso de registro de un solo sector
            if (header.next_sector_lba == NEXT_LBA_NONE) break;
        }

        current_lba = header.next_sector_lba;
    }

    result.sectors_read = static_cast<uint32_t>(result.lba_chain.size());
    return result;
}

// ─────────────────────────────────────────────────────────────────
//  delete_record — eliminación lógica
//
//  Sigue la cadena desde start_lba, recolecta todos los LBAs,
//  pone FLAG_FREE en el primer sector y libera el bitmap.
// ─────────────────────────────────────────────────────────────────
void DiskWriter::delete_record(uint32_t start_lba) {
    const uint32_t B = geometry_.get_bytes_per_sector();
    std::vector<uint8_t>  sector_buf(B);
    std::vector<uint32_t> chain;

    uint32_t current_lba = start_lba;

    // Recorrer la cadena para recolectar todos los LBAs
    while (true) {
        read_sector(current_lba, sector_buf.data());
        SectorHeader header = SectorHeader::from_bytes(sector_buf.data());

        chain.push_back(current_lba);

        bool last = (header.next_sector_lba == NEXT_LBA_NONE);
        if (!last) current_lba = header.next_sector_lba;
        else break;
    }

    // Marcar el primer sector como FLAG_FREE en disco
    read_sector(start_lba, sector_buf.data());
    sector_buf[0] = FLAG_FREE;   // byte 0 del sector = flag
    write_sector(start_lba, sector_buf.data());

    // Liberar todos los sectores en el bitmap
    bitmap_.free_sectors(chain);
}
